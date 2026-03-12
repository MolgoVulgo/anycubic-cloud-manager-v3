#include "infra/logging/JsonlLogger.h"

#include "infra/logging/Redactor.h"
#include "infra/logging/Rotator.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace accloud::logging {
namespace {

constexpr std::uintmax_t kDefaultRotateBytes = 2U * 1024U * 1024U;
constexpr int kDefaultRetention = 5;

struct Sink {
  std::filesystem::path path;
  std::ofstream stream;
};

struct State {
  std::mutex mutex;
  Config config;
  bool initialized = false;
  std::map<std::string, Sink> sinks;
};

State& state() {
  static State gState;
  return gState;
}

std::string levelToString(Level level) {
  switch (level) {
    case Level::kDebug:
      return "DEBUG";
    case Level::kInfo:
      return "INFO";
    case Level::kWarn:
      return "WARN";
    case Level::kError:
      return "ERROR";
    case Level::kFatal:
      return "FATAL";
  }
  return "INFO";
}

bool isErrorLevel(Level level) {
  return level == Level::kError || level == Level::kFatal;
}

std::string sanitizeSinkName(std::string_view source) {
  std::string sink;
  sink.reserve(source.size());
  for (char c : source) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_') {
      sink.push_back(c);
    }
  }
  if (sink.empty()) {
    return "app";
  }
  return sink;
}

std::filesystem::path defaultLogDir() {
  if (const char* env = std::getenv("ACCLOUD_LOG_DIR"); env != nullptr && *env != '\0') {
    return std::filesystem::path(env);
  }
  return std::filesystem::path("logs");
}

std::string toIso8601Local(std::chrono::system_clock::time_point ts) {
  const std::time_t rawTime = std::chrono::system_clock::to_time_t(ts);
  const auto millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()) % 1000;

  std::tm localTm {};
  std::tm utcTm {};
#if defined(_WIN32)
  localtime_s(&localTm, &rawTime);
  gmtime_s(&utcTm, &rawTime);
#else
  localtime_r(&rawTime, &localTm);
  gmtime_r(&rawTime, &utcTm);
#endif

  const std::time_t localAsEpoch = std::mktime(&localTm);
  const std::time_t utcAsEpoch = std::mktime(&utcTm);
  long offsetSeconds = static_cast<long>(std::difftime(localAsEpoch, utcAsEpoch));
  const char sign = offsetSeconds >= 0 ? '+' : '-';
  offsetSeconds = std::labs(offsetSeconds);
  const int offsetHours = static_cast<int>(offsetSeconds / 3600);
  const int offsetMinutes = static_cast<int>((offsetSeconds % 3600) / 60);

  std::ostringstream out;
  out << std::put_time(&localTm, "%Y-%m-%dT%H:%M:%S") << '.'
      << std::setw(3) << std::setfill('0') << millis.count() << sign << std::setw(2)
      << std::setfill('0') << offsetHours << ':' << std::setw(2) << std::setfill('0')
      << offsetMinutes;
  return out.str();
}

std::string nowIso8601Local() {
  return toIso8601Local(std::chrono::system_clock::now());
}

std::string jsonEscape(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char c : value) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20U) {
          std::ostringstream escape;
          escape << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                 << static_cast<int>(static_cast<unsigned char>(c));
          out += escape.str();
        } else {
          out.push_back(c);
        }
    }
  }
  return out;
}

void appendJsonField(std::ostringstream& out, std::string_view key, std::string_view value,
                     bool& first) {
  if (!first) {
    out << ',';
  }
  first = false;
  out << '"' << jsonEscape(key) << "\":\"" << jsonEscape(value) << '"';
}

FieldMap redactFields(FieldMap fields) {
  FieldMap redacted;
  for (auto& [key, value] : fields) {
    redacted[key] = redactValueForKey(key, value);
  }
  return redacted;
}

std::string buildJsonLine(std::string_view timestamp, Level level, std::string_view source,
                          std::string_view component, std::string_view event,
                          std::string_view message, const FieldMap& fields) {
  std::ostringstream out;
  out << '{';
  bool first = true;
  appendJsonField(out, "ts", timestamp, first);
  appendJsonField(out, "level", levelToString(level), first);
  appendJsonField(out, "source", source, first);
  if (!component.empty()) {
    appendJsonField(out, "component", component, first);
  }
  if (!event.empty()) {
    appendJsonField(out, "event", event, first);
  }
  if (!message.empty()) {
    appendJsonField(out, "message", message, first);
  }
  if (!fields.empty()) {
    if (!first) {
      out << ',';
    }
    out << "\"fields\":{";
    bool firstField = true;
    for (const auto& [key, value] : fields) {
      appendJsonField(out, key, value, firstField);
    }
    out << '}';
  }
  out << '}';
  return out.str();
}

std::string buildConsoleLine(std::string_view timestamp, Level level, std::string_view source,
                             std::string_view component, std::string_view event,
                             std::string_view message, const FieldMap& fields) {
  std::ostringstream out;
  out << timestamp << ' ' << source << ' ' << levelToString(level);
  if (!component.empty()) {
    out << ' ' << component;
  }
  if (!event.empty()) {
    out << '.' << event;
  }
  if (!message.empty()) {
    out << " - " << message;
  }
  for (const auto& [key, value] : fields) {
    out << ' ' << key << '=' << value;
  }
  return out.str();
}

void ensureInitializedLocked(State& s, const Config* configOverride) {
  if (s.initialized) {
    return;
  }
  s.config = configOverride != nullptr ? *configOverride : Config{};
  if (s.config.logDir.empty()) {
    s.config.logDir = defaultLogDir();
  }
  if (s.config.rotation.maxBytes == 0) {
    s.config.rotation.maxBytes = kDefaultRotateBytes;
  }
  if (s.config.rotation.retention <= 0) {
    s.config.rotation.retention = kDefaultRetention;
  }
  std::error_code ec;
  std::filesystem::create_directories(s.config.logDir, ec);
  s.initialized = true;
}

void closeAllStreamsLocked(State& s) {
  for (auto& [_, sink] : s.sinks) {
    if (sink.stream.is_open()) {
      sink.stream.flush();
      sink.stream.close();
    }
  }
}

Sink& ensureSinkLocked(State& s, std::string_view sinkName, std::size_t pendingWriteBytes) {
  Sink& sink = s.sinks[std::string(sinkName)];
  if (sink.path.empty()) {
    sink.path = s.config.logDir / (std::string(sinkName) + ".jsonl");
  }
  if (shouldRotateFile(sink.path, s.config.rotation, pendingWriteBytes)) {
    if (sink.stream.is_open()) {
      sink.stream.flush();
      sink.stream.close();
    }
    rotateFile(sink.path, s.config.rotation);
  }
  if (!sink.stream.is_open()) {
    sink.stream.open(sink.path, std::ios::out | std::ios::app);
  }
  return sink;
}

void writeLineToSinkLocked(State& s, std::string_view sinkName, std::string_view line) {
  Sink& sink = ensureSinkLocked(s, sinkName, line.size() + 1);
  if (!sink.stream.is_open()) {
    return;
  }
  sink.stream << line << '\n';
  sink.stream.flush();
}

std::vector<std::string> sinksForEvent(std::string_view source, std::string_view component,
                                       std::string_view event, Level level) {
  std::vector<std::string> sinks;
  sinks.emplace_back("app");
  const std::string sourceSink = sanitizeSinkName(source);
  if (sourceSink != "app") {
    sinks.push_back(sourceSink);
  }
  const bool mqttRelated = sourceSink == "mqtt" || component.find("mqtt") != std::string_view::npos ||
                           event.find("mqtt") != std::string_view::npos;
  if (mqttRelated && std::find(sinks.begin(), sinks.end(), "mqtt") == sinks.end()) {
    sinks.emplace_back("mqtt");
  }
  if (isErrorLevel(level) &&
      std::find(sinks.begin(), sinks.end(), "fault") == sinks.end()) {
    sinks.emplace_back("fault");
  }
  return sinks;
}

} // namespace

void initialize(const Config& config) {
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  ensureInitializedLocked(s, &config);
}

bool isInitialized() {
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  return s.initialized;
}

std::filesystem::path logDirectory() {
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  ensureInitializedLocked(s, nullptr);
  return s.config.logDir;
}

void shutdown() {
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  closeAllStreamsLocked(s);
  s.sinks.clear();
  s.initialized = false;
}

void log(Level level, std::string source, std::string component, std::string event,
         std::string message, FieldMap fields) {
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  ensureInitializedLocked(s, nullptr);

  if (source.empty()) {
    source = "app";
  }

  std::string redactedMessage = redactMessage(message);
  FieldMap redactedFields = redactFields(std::move(fields));
  const std::string timestamp = nowIso8601Local();
  const std::string jsonLine = buildJsonLine(timestamp, level, source, component, event,
                                             redactedMessage, redactedFields);

  for (const std::string& sinkName : sinksForEvent(source, component, event, level)) {
    writeLineToSinkLocked(s, sinkName, jsonLine);
  }

  if (s.config.mirrorToStderr) {
    std::cerr << buildConsoleLine(timestamp, level, source, component, event, redactedMessage,
                                  redactedFields)
              << '\n';
    std::cerr.flush();
  }
}

void debug(std::string source, std::string component, std::string event, std::string message,
           FieldMap fields) {
#if defined(ACCLOUD_DEBUG)
  log(Level::kDebug, std::move(source), std::move(component), std::move(event), std::move(message),
      std::move(fields));
#else
  (void)source;
  (void)component;
  (void)event;
  (void)message;
  (void)fields;
#endif
}

void info(std::string source, std::string component, std::string event, std::string message,
          FieldMap fields) {
  log(Level::kInfo, std::move(source), std::move(component), std::move(event), std::move(message),
      std::move(fields));
}

void warn(std::string source, std::string component, std::string event, std::string message,
          FieldMap fields) {
  log(Level::kWarn, std::move(source), std::move(component), std::move(event), std::move(message),
      std::move(fields));
}

void error(std::string source, std::string component, std::string event, std::string message,
           FieldMap fields) {
  log(Level::kError, std::move(source), std::move(component), std::move(event), std::move(message),
      std::move(fields));
}

void fatal(std::string source, std::string component, std::string event, std::string message,
           FieldMap fields) {
  log(Level::kFatal, std::move(source), std::move(component), std::move(event), std::move(message),
      std::move(fields));
}

} // namespace accloud::logging
