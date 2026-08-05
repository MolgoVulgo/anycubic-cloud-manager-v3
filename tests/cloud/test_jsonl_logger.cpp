#include "infra/logging/JsonlLogger.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <iostream>
#include <optional>
#include <sstream>
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

bool test_render3d_events_have_a_dedicated_sink() {
  const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path logDir =
      std::filesystem::temp_directory_path()
      / ("accloud-render3d-log-test-" + std::to_string(unique));
  std::error_code ec;
  std::filesystem::remove_all(logDir, ec);

  accloud::logging::shutdown();
  accloud::logging::Config config;
  config.logDir = logDir;
  config.mirrorToStderr = false;
  accloud::logging::initialize(config);
  accloud::logging::log(
      accloud::logging::Level::kDebug,
      "render3d",
      "mesher",
      "build_started",
      {},
      {{"generation", "7"}, {"layer_step", "2"}});
  accloud::logging::log(
      accloud::logging::Level::kDebug,
      "render3d",
      "support_analysis",
      "completed",
      {},
      {{"generation", "7"},
       {"raft_last_layer", "10"},
       {"support_runs", "35"},
       {"free_support_runs", "35"},
       {"projected_support_runs", "0"},
       {"projected_contact_pixels", "0"},
       {"rejected_projection_runs", "0"},
       {"rejected_growth_pixels", "23"},
       {"untapered_model_contacts", "3"},
       {"contacts_without_valid_projection", "0"},
       {"maximum_contact_growth_ratio", "0.000000"},
       {"terminal_support_stops", "7"},
       {"expanding_model_contacts", "7"},
       {"maximum_model_expansion_ratio", "12.500000"}});
  accloud::logging::shutdown();

  const std::filesystem::path dedicatedPath = logDir / "render3d.jsonl";
  std::ifstream input(dedicatedPath);
  std::ostringstream content;
  content << input.rdbuf();
  const std::string line = content.str();
  const bool ok = expect(std::filesystem::exists(dedicatedPath),
                         "Render3D events must create render3d.jsonl")
      && expect(line.find("\"source\":\"render3d\"") != std::string::npos,
                "Dedicated Render3D log must preserve its source")
      && expect(line.find("\"event\":\"build_started\"") != std::string::npos,
                "Dedicated Render3D log must preserve generation events")
      && expect(line.find("\"layer_step\":\"2\"") != std::string::npos,
                "Dedicated Render3D log must preserve sampling diagnostics")
      && expect(line.find("\"component\":\"support_analysis\"")
                    != std::string::npos,
                "Dedicated Render3D log must preserve support-analysis events")
      && expect(line.find("\"raft_last_layer\":\"10\"")
                    != std::string::npos,
                "Support-analysis logs must preserve phase diagnostics")
      && expect(line.find("\"support_runs\":\"35\"")
                    != std::string::npos,
                "Support-analysis logs must preserve semantic summaries")
      && expect(line.find("\"free_support_runs\":\"35\"")
                    != std::string::npos,
                "Support-analysis logs must distinguish free support runs")
      && expect(line.find("\"projected_support_runs\":\"0\"")
                    != std::string::npos,
                "Support-analysis logs must confirm that model contacts are not projected")
      && expect(line.find("\"rejected_projection_runs\":\"0\"")
                    != std::string::npos,
                "Support-analysis logs must keep the compatibility projection counter")
      && expect(line.find("\"projected_contact_pixels\":\"0\"")
                    != std::string::npos,
                "Support-analysis logs must confirm that no model pixel inherits support")
      && expect(line.find("\"rejected_growth_pixels\":\"23\"")
                    != std::string::npos,
                "Support-analysis logs must expose prevented post-contact growth")
      && expect(line.find("\"untapered_model_contacts\":\"3\"")
                    != std::string::npos,
                "Support-analysis logs must expose rejected untapered contacts")
      && expect(line.find("\"contacts_without_valid_projection\":\"0\"")
                    != std::string::npos,
                "Support-analysis logs must keep the compatibility invalid-projection counter")
      && expect(line.find("\"maximum_contact_growth_ratio\":\"0.000000\"")
                    != std::string::npos,
                "Support-analysis logs must confirm that contact projection is disabled")
      && expect(line.find("\"terminal_support_stops\":\"7\"")
                    != std::string::npos,
                "Support-analysis logs must expose terminal stops before model matter")
      && expect(line.find("\"expanding_model_contacts\":\"7\"")
                    != std::string::npos,
                "Support-analysis logs must count small-to-large model transitions")
      && expect(line.find("\"maximum_model_expansion_ratio\":\"12.500000\"")
                    != std::string::npos,
                "Support-analysis logs must expose the largest model expansion ratio");

  std::filesystem::remove_all(logDir, ec);
  return ok;
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
  if (!test_local_timestamp_uses_effective_dst_offset()
      || !test_render3d_events_have_a_dedicated_sink()) {
    return 1;
  }
  std::cout << "Jsonl logger tests passed\n";
  return 0;
}
