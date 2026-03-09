#include "SessionProvider.h"

namespace accloud::cloud::core {

cloud::LoadSessionResult SessionProvider::loadCurrentSession(
    const std::optional<std::filesystem::path>& pathOverride) const {
    return cloud::loadSessionFile(pathOverride);
}

std::optional<std::string> SessionProvider::requireAccessToken(const cloud::SessionData& session) const {
    const auto accessTokenIt = session.tokens.find("access_token");
    if (accessTokenIt == session.tokens.end() || accessTokenIt->second.empty()) {
        return std::nullopt;
    }
    return accessTokenIt->second;
}

std::string SessionProvider::optionalXxToken(const cloud::SessionData& session) const {
    const auto xxTokenIt = session.tokens.find("token");
    if (xxTokenIt == session.tokens.end()) {
        return {};
    }
    return xxTokenIt->second;
}

SessionContextResult SessionProvider::loadRequestContext(
    const std::optional<std::filesystem::path>& pathOverride) const {
    const cloud::LoadSessionResult loaded = loadCurrentSession(pathOverride);
    if (!loaded.ok) {
        return {false, loaded.message, {}};
    }

    const std::optional<std::string> accessToken = requireAccessToken(loaded.session);
    if (!accessToken.has_value()) {
        return {false, "session_missing_access_token", {}};
    }

    CloudRequestContext context;
    context.accessToken = *accessToken;
    context.xxToken = optionalXxToken(loaded.session);
    context.sessionPath = loaded.path;
    return {true, "ok", std::move(context)};
}

} // namespace accloud::cloud::core
