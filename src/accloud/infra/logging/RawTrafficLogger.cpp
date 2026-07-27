#include "infra/logging/RawTrafficLogger.h"

#if defined(ACCLOUD_DEBUG)

#include "infra/config/AppPaths.h"
#include "infra/logging/JsonlLogger.h"
#include "infra/logging/Redactor.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>

namespace accloud::logging::raw {
namespace {

constexpr std::string_view kRawLogFilename = "log_brut.txt";
constexpr std::size_t kMaximumTextPayloadBytes = 16U * 1024U * 1024U;

std::mutex& rawLogMutex() {
    static std::mutex mutex;
    return mutex;
}

std::atomic<std::uint64_t>& correlationCounter() {
    static std::atomic<std::uint64_t> counter{0};
    return counter;
}

bool& sessionHeaderWritten() {
    static bool written = false;
    return written;
}

std::string nowIso8601Local() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm localTm{};
#if defined(_WIN32)
    localtime_s(&localTm, &raw);
#else
    localtime_r(&raw, &localTm);
#endif

    std::ostringstream out;
    out << std::put_time(&localTm, "%Y-%m-%dT%H:%M:%S") << '.'
        << std::setw(3) << std::setfill('0') << millis.count();
    return out.str();
}

std::filesystem::path rawLogPath() {
    std::filesystem::path dir = logging::isInitialized()
        ? logging::logDirectory()
        : accloud::config::logDir();
    return dir / kRawLogFilename;
}

bool looksLikeUrl(std::string_view value) {
    return value.rfind("https://", 0) == 0 || value.rfind("http://", 0) == 0
        || value.rfind("//", 0) == 0;
}

bool isAdditionalSensitiveKey(std::string_view key) {
    std::string lowered;
    lowered.reserve(key.size());
    for (const char c : key) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lowered == "email" || lowered == "user_email" || lowered == "user_id"
        || lowered == "userid" || lowered == "pc_id" || lowered == "ip"
        || lowered == "last_login_ip" || lowered == "oauth_code";
}

std::string sanitizeString(std::string_view value) {
    if (looksLikeUrl(value)) {
        return logging::safeUrlForLogs(value);
    }
    return logging::redactMessage(value);
}

void redactJson(nlohmann::json& value, std::string_view key = {}) {
    if (!key.empty() && (logging::isSensitiveKey(key) || isAdditionalSensitiveKey(key))) {
        value = "<redacted>";
        return;
    }

    if (value.is_object()) {
        for (auto& [childKey, childValue] : value.items()) {
            redactJson(childValue, childKey);
        }
        return;
    }
    if (value.is_array()) {
        for (auto& child : value) {
            redactJson(child);
        }
        return;
    }
    if (value.is_string()) {
        value = sanitizeString(value.get_ref<const std::string&>());
    }
}

std::string redactFallback(std::string payload) {
    payload = logging::redactMessage(payload);
    static const std::regex sensitiveJsonValue(
        R"REGEX(("(?:access_token|refresh_token|id_token|token|authorization|cookie|password|secret|signature|credential|session|email|user_email|user_id|userid|pc_id|last_login_ip|ip)"\s*:\s*")([^"]*)("))REGEX",
        std::regex::icase);
    return std::regex_replace(payload, sensitiveJsonValue, "$1<redacted>$3");
}

bool likelyText(std::string_view payload) {
    if (payload.empty()) {
        return true;
    }
    std::size_t controls = 0;
    for (const unsigned char c : payload) {
        if (c == 0) {
            return false;
        }
        if (c < 0x09 || (c > 0x0D && c < 0x20)) {
            ++controls;
        }
    }
    return controls * 100U < payload.size();
}

std::string sanitizePayload(std::string_view payload) {
    if (payload.empty()) {
        return "<empty>";
    }
    if (!likelyText(payload)) {
        return "<binary omitted: " + std::to_string(payload.size()) + " bytes>";
    }

    const bool truncated = payload.size() > kMaximumTextPayloadBytes;
    std::string text(payload.substr(0, std::min(payload.size(), kMaximumTextPayloadBytes)));
    try {
        nlohmann::json parsed = nlohmann::json::parse(text);
        redactJson(parsed);
        text = parsed.dump(2);
    } catch (...) {
        text = redactFallback(std::move(text));
    }
    if (truncated) {
        text += "\n<truncated after " + std::to_string(kMaximumTextPayloadBytes) + " bytes>";
    }
    return text;
}

HeaderList sanitizeHeaders(const HeaderList& headers) {
    HeaderList sanitized;
    sanitized.reserve(headers.size());
    for (const auto& [name, value] : headers) {
        if (logging::isSensitiveKey(name) || isAdditionalSensitiveKey(name)) {
            sanitized.emplace_back(name, "<redacted>");
        } else {
            sanitized.emplace_back(name, sanitizeString(value));
        }
    }
    return sanitized;
}

void appendRecord(std::string_view title, std::string_view body) {
    std::lock_guard<std::mutex> lock(rawLogMutex());
    const std::filesystem::path path = rawLogPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::out | std::ios::app | std::ios::binary);
    if (!out.is_open()) {
        return;
    }
    if (!sessionHeaderWritten()) {
        out << "\n################ DEV RAW TRAFFIC SESSION " << nowIso8601Local()
            << " ################\n";
        out << "# HTTP and MQTT payloads are captured only in ACCLOUD_DEBUG builds.\n";
        out << "# Credentials, personal identifiers and signed URL queries are redacted.\n";
        sessionHeaderWritten() = true;
    }
    out << "\n===== " << nowIso8601Local() << ' ' << title << " =====\n";
    out << body;
    if (body.empty() || body.back() != '\n') {
        out << '\n';
    }
    out << "===== END " << title << " =====\n";
    out.flush();
}

void appendHeaders(std::ostringstream& out, const HeaderList& headers) {
    out << "HEADERS:\n";
    if (headers.empty()) {
        out << "<none>\n";
        return;
    }
    for (const auto& [name, value] : sanitizeHeaders(headers)) {
        out << name << ": " << value << '\n';
    }
}

} // namespace

std::string nextCorrelationId(std::string_view prefix) {
    const std::uint64_t value = correlationCounter().fetch_add(1, std::memory_order_relaxed) + 1;
    std::string id(prefix.empty() ? "raw" : prefix);
    id.push_back('-');
    id += std::to_string(value);
    return id;
}

void logHttpRequest(std::string_view correlationId,
                    std::string_view method,
                    std::string_view url,
                    const HeaderList& headers,
                    std::string_view body) {
    std::ostringstream out;
    out << "ID: " << correlationId << '\n';
    out << "METHOD: " << method << '\n';
    out << "URL: " << logging::safeUrlForLogs(url) << '\n';
    appendHeaders(out, headers);
    out << "BODY:\n" << sanitizePayload(body) << '\n';
    appendRecord("HTTP REQUEST", out.str());
}

void logHttpResponse(std::string_view correlationId,
                     int status,
                     std::string_view reason,
                     const HeaderList& headers,
                     std::string_view body,
                     std::string_view networkError) {
    std::ostringstream out;
    out << "ID: " << correlationId << '\n';
    out << "STATUS: " << status;
    if (!reason.empty()) {
        out << ' ' << reason;
    }
    out << '\n';
    if (!networkError.empty()) {
        out << "NETWORK_ERROR: " << sanitizeString(networkError) << '\n';
    }
    appendHeaders(out, headers);
    out << "BODY:\n" << sanitizePayload(body) << '\n';
    appendRecord("HTTP RESPONSE", out.str());
}

void logMqttMessage(std::string_view direction,
                    std::string_view topic,
                    std::string_view payload) {
    std::ostringstream out;
    out << "DIRECTION: " << direction << '\n';
    out << "TOPIC: " << topic << '\n';
    out << "PAYLOAD:\n" << sanitizePayload(payload) << '\n';
    appendRecord("MQTT MESSAGE", out.str());
}

} // namespace accloud::logging::raw

#else

namespace accloud::logging::raw {

std::string nextCorrelationId(std::string_view) { return {}; }
void logHttpRequest(std::string_view, std::string_view, std::string_view,
                    const HeaderList&, std::string_view) {}
void logHttpResponse(std::string_view, int, std::string_view,
                     const HeaderList&, std::string_view, std::string_view) {}
void logMqttMessage(std::string_view, std::string_view, std::string_view) {}

} // namespace accloud::logging::raw

#endif
