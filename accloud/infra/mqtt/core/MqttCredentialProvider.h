#pragma once

#include "MqttSessionManager.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace accloud::mqtt::core {

enum class MqttAuthMode {
    Slicer,
    Android,
};

enum class AndroidCryptoProfile {
    A_2b_12,
    B_2a_12,
    C_2b_10,
};

struct MqttCredentialInput {
    std::string brokerHost;
    std::string email;
    std::string userId;
    std::string authToken;
    MqttAuthMode authMode{MqttAuthMode::Slicer};
    bool compatibilityMode{false};
};

struct MqttCredentialCandidate {
    MqttCredentials credentials;
    std::optional<AndroidCryptoProfile> androidProfile;
};

struct MqttCredentialBuildResult {
    bool ok{false};
    std::string code;
    std::string message;
    std::vector<MqttCredentialCandidate> candidates;
};

class MqttCredentialProvider {
public:
    static MqttAuthMode authModeFromString(const std::string& value);
    static std::string authModeToString(MqttAuthMode mode);

    // Default policy: Slicer unless explicitly set to Android.
    static MqttAuthMode resolvePreferredMode(const std::optional<std::string>& settingsValue);

    MqttCredentialBuildResult buildCandidates(const MqttCredentialInput& input);

    void rememberSuccessfulAndroidProfile(const MqttCredentialInput& input, AndroidCryptoProfile profile);
    void clearCachedAndroidProfile(const MqttCredentialInput& input);

private:
    std::optional<AndroidCryptoProfile> findCachedAndroidProfile(const MqttCredentialInput& input) const;
    static std::string cacheKey(const MqttCredentialInput& input);

    std::optional<MqttCredentialCandidate> buildSlicerCandidate(const MqttCredentialInput& input) const;
    std::optional<MqttCredentialCandidate> buildAndroidCandidate(const MqttCredentialInput& input,
                                                                 AndroidCryptoProfile profile) const;

    std::string buildClientId(const MqttCredentialInput& input, MqttAuthMode mode) const;
    std::string buildUsername(const std::string& appId,
                              const std::string& email,
                              const std::string& clientId,
                              const std::string& mqttToken) const;

    static std::string androidProfileToString(AndroidCryptoProfile profile);

    std::map<std::string, AndroidCryptoProfile> m_androidProfileCache;
};

} // namespace accloud::mqtt::core
