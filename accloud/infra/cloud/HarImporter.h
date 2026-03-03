#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace accloud::cloud {

struct SessionData {
  std::map<std::string, std::string> tokens;

  [[nodiscard]] bool empty() const { return tokens.empty(); }
};

struct HarImportOptions {
  std::string hostContains = "anycubic";
  std::optional<std::filesystem::path> sessionPathOverride;
  bool mergeWithExistingSession = true;
  bool persistSession = true;
};

struct HarImportResult {
  bool ok = false;
  std::string message;
  SessionData session;
  std::size_t entriesVisited = 0;
  std::size_t entriesAccepted = 0;
};

struct LoadSessionResult {
  bool ok = false;
  std::string message;
  SessionData session;
  std::filesystem::path path;
};

struct SaveSessionResult {
  bool ok = false;
  std::string message;
  std::filesystem::path path;
};

std::filesystem::path resolveSessionPath(
    const std::optional<std::filesystem::path>& pathOverride = std::nullopt);

HarImportResult importHarText(const std::string& harText,
                              const HarImportOptions& options = {});
HarImportResult importHarFile(const std::filesystem::path& harPath,
                              const HarImportOptions& options = {});

LoadSessionResult loadSessionFile(
    const std::optional<std::filesystem::path>& pathOverride = std::nullopt);
SaveSessionResult saveSessionFile(const SessionData& session,
                                  const std::optional<std::filesystem::path>& pathOverride = std::nullopt);

} // namespace accloud::cloud
