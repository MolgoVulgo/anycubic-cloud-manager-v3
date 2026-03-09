#pragma once

#include "infra/cloud/HarImporter.h"

#include <filesystem>
#include <optional>
#include <string>

namespace accloud::cloud::core {

struct CloudRequestContext {
    std::string accessToken;
    std::string xxToken;
    std::filesystem::path sessionPath;
};

struct SessionContextResult {
    bool ok{false};
    std::string message;
    CloudRequestContext context;
};

class SessionProvider {
public:
    cloud::LoadSessionResult loadCurrentSession(
        const std::optional<std::filesystem::path>& pathOverride = std::nullopt) const;

    std::optional<std::string> requireAccessToken(const cloud::SessionData& session) const;
    std::string optionalXxToken(const cloud::SessionData& session) const;

    SessionContextResult loadRequestContext(
        const std::optional<std::filesystem::path>& pathOverride = std::nullopt) const;
};

} // namespace accloud::cloud::core
