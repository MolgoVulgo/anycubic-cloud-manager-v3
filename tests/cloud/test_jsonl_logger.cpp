#include "infra/logging/JsonlLogger.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    return false;
  }
  return true;
}

std::optional<std::string> environmentValue(const char* key) {
  const char* value = std::getenv(key);
  if (value == nullptr) {
    return std::nullopt;
  }
  return std::string(value);
}

void applyTimeZone(const std::optional<std::string>& value) {
#if defined(_WIN32)
  _putenv_s("TZ", value.has_value() ? value->c_str() : "");
  _tzset();
#else
  if (value.has_value()) {
    setenv("TZ", value->c_str(), 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
#endif
}

std::chrono::system_clock::time_point timePointFromEpochMillis(long long epochMillis) {
  return std::chrono::system_clock::time_point{std::chrono::milliseconds{epochMillis}};
}

bool test_local_timestamp_uses_effective_dst_offset() {
#if defined(_WIN32)
  // The Windows CRT does not consistently accept portable POSIX DST transition
  // rules. The production implementation still uses the active local timezone.
  return true;
#else
  const auto previousTimeZone = environmentValue("TZ");
  applyTimeZone(std::string("CET-1CEST,M3.5.0,M10.5.0/3"));

  const std::string winter = accloud::logging::formatIso8601LocalTimestamp(
      timePointFromEpochMillis(1768480496789LL));
  const std::string summer = accloud::logging::formatIso8601LocalTimestamp(
      timePointFromEpochMillis(1785613370886LL));

  applyTimeZone(previousTimeZone);

  return expect(winter == "2026-01-15T13:34:56.789+01:00",
                "Winter timestamp must use the standard-time offset")
      && expect(summer == "2026-08-01T21:42:50.886+02:00",
                "Summer timestamp must use the daylight-saving offset");
#endif
}

}  // namespace

int main() {
  if (!test_local_timestamp_uses_effective_dst_offset()) {
    return 1;
  }
  std::cout << "Jsonl logger timestamp tests passed\n";
  return 0;
}
