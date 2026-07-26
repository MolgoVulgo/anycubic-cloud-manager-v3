#pragma once

#include <filesystem>

namespace accloud::mqtt::core {

class MqttTlsPathResolver {
public:
    // Resolves the repository-local development TLS directory without relying
    // solely on __FILE__. The directory is <repo>/resources/mqtt/tls.
    static std::filesystem::path resolveDevTlsDirectory();

    // Returns the preferred filename when present, otherwise the legacy typo
    // retained by existing local material. If neither exists, returns the
    // preferred path for explicit validation/error reporting.
    static std::filesystem::path resolveDevTlsFile(const char* preferredFilename,
                                                   const char* legacyFilename);
};

} // namespace accloud::mqtt::core
