#include "App.h"

#include "infra/cloud/HarImporter.h"
#include "infra/logging/JsonlLogger.h"

#include <iostream>
#include <optional>

namespace accloud {
namespace {

std::optional<std::string> argValue(int argc, char** argv, const std::string& flag) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (flag == argv[i]) {
      return std::string(argv[i + 1]);
    }
  }
  return std::nullopt;
}

} // namespace

bool App::hasArg(int argc, char** argv, const std::string& flag) {
  for (int i = 1; i < argc; ++i) {
    if (flag == argv[i]) {
      return true;
    }
  }
  return false;
}

int App::run(int argc, char** argv) {
  logging::info("app", "cli", "run_start", "CLI entrypoint started",
                {{"argc", std::to_string(argc)}});

  if (hasArg(argc, argv, "--smoke")) {
    logging::info("app", "cli", "smoke_mode", "Smoke mode requested");
    std::cout << "accloud smoke ok" << std::endl;
    return 0;
  }

  if (auto harPath = argValue(argc, argv, "--import-har"); harPath.has_value()) {
    logging::info("app", "cli", "har_import_requested", "Starting HAR import",
                  {{"har_path", *harPath}});
    cloud::HarImportOptions options;
    if (auto sessionPath = argValue(argc, argv, "--session-path"); sessionPath.has_value()) {
      options.sessionPathOverride = *sessionPath;
      logging::debug("app", "cli", "har_import_session_override",
                     "Using session path override", {{"session_path", *sessionPath}});
    }

    cloud::HarImportResult result = cloud::importHarFile(*harPath, options);
    if (!result.ok) {
      logging::error("app", "cli", "har_import_failed", "HAR import failed",
                     {{"har_path", *harPath}, {"reason", result.message}});
      std::cerr << "HAR import failed: " << result.message << std::endl;
      return 1;
    }

    logging::info("app", "cli", "har_import_success", "HAR import succeeded",
                  {{"har_path", *harPath},
                   {"entries_visited", std::to_string(result.entriesVisited)},
                   {"entries_accepted", std::to_string(result.entriesAccepted)},
                   {"token_count", std::to_string(result.session.tokens.size())}});
    std::cout << result.message << std::endl;
    std::cout << "Session keys imported: " << result.session.tokens.size() << std::endl;
    return 0;
  }

  logging::warn("app", "cli", "no_mode_selected", "No mode selected, printing usage");
  std::cout << "accloud skeleton initialized" << std::endl;
  std::cout << "Use --smoke for CI smoke test" << std::endl;
  std::cout << "Use --import-har <file.har> [--session-path <session.json>]" << std::endl;
  return 0;
}

} // namespace accloud
