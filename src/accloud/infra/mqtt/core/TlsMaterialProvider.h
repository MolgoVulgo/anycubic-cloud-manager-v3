#pragma once

#include <filesystem>
#include <string>

namespace accloud::mqtt::core {

struct TlsMaterialPaths {
    std::filesystem::path caCertificatePath;
    std::filesystem::path clientCertificatePath;
    std::filesystem::path clientKeyPath;
    bool allowInsecureTls{false};
    bool usingDevFallback{false};
};

struct TlsMaterialLoadResult {
    bool ok{false};
    std::string code;
    std::string message;
    TlsMaterialPaths paths;
};

class TlsMaterialProvider {
public:
    // Loads TLS material paths from environment and validates readability/content.
    // Dev fallback is disabled by default and requires ACCLOUD_MQTT_TLS_DEV_FALLBACK=1.
    TlsMaterialLoadResult loadFromEnvironment() const;

    // Validates explicit TLS material paths (readability + minimal PEM structure checks).
    TlsMaterialLoadResult validate(const TlsMaterialPaths& paths) const;
};

} // namespace accloud::mqtt::core

