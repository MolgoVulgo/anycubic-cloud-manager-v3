#include "TlsMaterialProvider.h"

#include "infra/logging/JsonlLogger.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace accloud::mqtt::core {
namespace {

constexpr const char* kEnvCaPath = "ACCLOUD_MQTT_TLS_CA_PATH";
constexpr const char* kEnvClientCertPath = "ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH";
constexpr const char* kEnvClientKeyPath = "ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH";
constexpr const char* kEnvAllowInsecureTls = "ACCLOUD_MQTT_TLS_ALLOW_INSECURE";
constexpr const char* kEnvDevFallback = "ACCLOUD_MQTT_TLS_DEV_FALLBACK";

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool parseBoolEnv(const char* key, bool defaultValue = false) {
    const char* value = std::getenv(key);
    if (value == nullptr || *value == '\0') {
        return defaultValue;
    }
    const std::string lowered = toLowerAscii(value);
    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

std::filesystem::path getenvPath(const char* key) {
    const char* value = std::getenv(key);
    if (value == nullptr || *value == '\0') {
        return {};
    }
    return std::filesystem::path(value);
}

std::filesystem::path repositoryRootFromSource() {
    std::filesystem::path p(__FILE__);
    return p.parent_path().parent_path().parent_path().parent_path();
}

std::filesystem::path defaultDevPath(const char* filename) {
    return repositoryRootFromSource() / "Docs" / "MQTT" / "resources" / filename;
}

std::string safePathForLogs(const std::filesystem::path& path) {
    if (path.empty()) {
        return "<empty>";
    }
    const std::string name = path.filename().string();
    if (!name.empty()) {
        return name;
    }
    return "<no-filename>";
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool hasPemMarker(const std::string& content, const std::string& marker) {
    return content.find(marker) != std::string::npos;
}

TlsMaterialLoadResult failure(std::string code,
                              std::string message,
                              const TlsMaterialPaths& paths = {}) {
    return {false, std::move(code), std::move(message), paths};
}

} // namespace

TlsMaterialLoadResult TlsMaterialProvider::loadFromEnvironment() const {
    TlsMaterialPaths paths;
    paths.caCertificatePath = getenvPath(kEnvCaPath);
    paths.clientCertificatePath = getenvPath(kEnvClientCertPath);
    paths.clientKeyPath = getenvPath(kEnvClientKeyPath);
    paths.allowInsecureTls = parseBoolEnv(kEnvAllowInsecureTls, false);

    const bool devFallback = parseBoolEnv(kEnvDevFallback, false);
    if (devFallback) {
        if (paths.caCertificatePath.empty()) {
            paths.caCertificatePath = defaultDevPath("anycubic_mqqt_tls_ca.crt");
        }
        if (paths.clientCertificatePath.empty()) {
            paths.clientCertificatePath = defaultDevPath("anycubic_mqqt_tls_client.crt");
        }
        if (paths.clientKeyPath.empty()) {
            paths.clientKeyPath = defaultDevPath("anycubic_mqqt_tls_client.key");
        }
        paths.usingDevFallback = true;
    }

    if (paths.allowInsecureTls) {
        logging::warn("cloud", "mqtt_tls", "insecure_mode_enabled",
                      "TLS insecure mode is enabled. This must remain disabled in production.");
    }

    return validate(paths);
}

TlsMaterialLoadResult TlsMaterialProvider::validate(const TlsMaterialPaths& paths) const {
    if (paths.caCertificatePath.empty() || paths.clientCertificatePath.empty()
        || paths.clientKeyPath.empty()) {
        logging::error("cloud", "mqtt_tls", "missing_paths",
                       "Missing TLS material path(s) for MQTT",
                       {
                           {"ca", safePathForLogs(paths.caCertificatePath)},
                           {"client_cert", safePathForLogs(paths.clientCertificatePath)},
                           {"client_key", safePathForLogs(paths.clientKeyPath)},
                       });
        return failure("mqtt_tls_missing_paths",
                       "Missing TLS material path(s). Configure MQTT TLS paths or enable explicit "
                       "dev fallback with ACCLOUD_MQTT_TLS_DEV_FALLBACK=1.",
                       paths);
    }

    const auto validateReadableFile = [&](const std::filesystem::path& path,
                                          const char* code,
                                          const char* fieldName) -> std::optional<TlsMaterialLoadResult> {
        if (!std::filesystem::exists(path)) {
            logging::error("cloud", "mqtt_tls", "path_not_found",
                           "TLS material file does not exist",
                           {
                               {"field", fieldName},
                               {"path", safePathForLogs(path)},
                           });
            return failure(code, "TLS material file not found: " + path.string(), paths);
        }
        if (!std::filesystem::is_regular_file(path)) {
            logging::error("cloud", "mqtt_tls", "not_regular_file",
                           "TLS material path is not a regular file",
                           {
                               {"field", fieldName},
                               {"path", safePathForLogs(path)},
                           });
            return failure(code, "TLS material path is not a regular file: " + path.string(), paths);
        }
        std::ifstream in(path, std::ios::binary);
        if (!in.good()) {
            logging::error("cloud", "mqtt_tls", "file_not_readable",
                           "TLS material file is not readable",
                           {
                               {"field", fieldName},
                               {"path", safePathForLogs(path)},
                           });
            return failure(code, "TLS material file is not readable: " + path.string(), paths);
        }
        return std::nullopt;
    };

    if (auto e = validateReadableFile(paths.caCertificatePath, "mqtt_tls_ca_invalid", "ca")) {
        return *e;
    }
    if (auto e = validateReadableFile(paths.clientCertificatePath, "mqtt_tls_client_cert_invalid",
                                      "client_cert")) {
        return *e;
    }
    if (auto e = validateReadableFile(paths.clientKeyPath, "mqtt_tls_client_key_invalid", "client_key")) {
        return *e;
    }

    const std::string caPem = readTextFile(paths.caCertificatePath);
    if (!hasPemMarker(caPem, "BEGIN CERTIFICATE")) {
        return failure("mqtt_tls_ca_malformed",
                       "CA certificate does not look like a PEM certificate.",
                       paths);
    }

    const std::string certPem = readTextFile(paths.clientCertificatePath);
    if (!hasPemMarker(certPem, "BEGIN CERTIFICATE")) {
        return failure("mqtt_tls_client_cert_malformed",
                       "Client certificate does not look like a PEM certificate.",
                       paths);
    }

    const std::string keyPem = readTextFile(paths.clientKeyPath);
    const bool hasPrivateKeyPem = hasPemMarker(keyPem, "BEGIN PRIVATE KEY")
        || hasPemMarker(keyPem, "BEGIN RSA PRIVATE KEY")
        || hasPemMarker(keyPem, "BEGIN EC PRIVATE KEY");
    if (!hasPrivateKeyPem) {
        return failure("mqtt_tls_client_key_malformed",
                       "Client key does not look like a PEM private key.",
                       paths);
    }

    if (paths.clientCertificatePath == paths.clientKeyPath) {
        return failure("mqtt_tls_cert_key_same_path",
                       "Client certificate and key must be different files.",
                       paths);
    }

    logging::info("cloud", "mqtt_tls", "materials_loaded",
                  "MQTT TLS materials loaded",
                  {
                      {"ca", safePathForLogs(paths.caCertificatePath)},
                      {"client_cert", safePathForLogs(paths.clientCertificatePath)},
                      {"client_key", safePathForLogs(paths.clientKeyPath)},
                      {"dev_fallback", paths.usingDevFallback ? "1" : "0"},
                      {"allow_insecure_tls", paths.allowInsecureTls ? "1" : "0"},
                  });

    return {true, "ok", "TLS materials ready", paths};
}

} // namespace accloud::mqtt::core
