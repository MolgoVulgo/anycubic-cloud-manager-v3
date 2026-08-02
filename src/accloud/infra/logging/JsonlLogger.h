#pragma once

#include "infra/logging/Rotator.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <string>

namespace accloud::logging {

enum class Level {
  kDebug,
  kInfo,
  kWarn,
  kError,
  kFatal,
};

struct Config {
  std::filesystem::path logDir;
  RotationPolicy rotation;
  bool mirrorToStderr = true;
};

using FieldMap = std::map<std::string, std::string>;

[[nodiscard]] std::string formatIso8601LocalTimestamp(
    std::chrono::system_clock::time_point timestamp);

void initialize(const Config& config = {});
[[nodiscard]] bool isInitialized();
[[nodiscard]] std::filesystem::path logDirectory();
void shutdown();

void log(Level level, std::string source, std::string component, std::string event,
         std::string message = {}, FieldMap fields = {});
void debug(std::string source, std::string component, std::string event, std::string message = {},
           FieldMap fields = {});
void info(std::string source, std::string component, std::string event, std::string message = {},
          FieldMap fields = {});
void warn(std::string source, std::string component, std::string event, std::string message = {},
          FieldMap fields = {});
void error(std::string source, std::string component, std::string event, std::string message = {},
           FieldMap fields = {});
void fatal(std::string source, std::string component, std::string event, std::string message = {},
           FieldMap fields = {});

} // namespace accloud::logging
