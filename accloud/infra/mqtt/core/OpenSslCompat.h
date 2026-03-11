#pragma once

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace accloud::mqtt::core {

struct OpenSslCompatResult {
    bool ok{false};
    bool applied{false};
    std::string code;
    std::string message;
    std::string configPath;
};

inline bool parseBoolEnvValue(const char* value, bool defaultValue) {
    if (value == nullptr || *value == '\0') {
        return defaultValue;
    }
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

inline bool shouldEnableMqttOpenSslCompatFromEnv() {
    return parseBoolEnvValue(std::getenv("ACCLOUD_MQTT_TLS_ALLOW_INSECURE"), true);
}

inline OpenSslCompatResult ensureOpenSslSecurityLevelCompat(bool enableCompatMode) {
    if (!enableCompatMode) {
        return {true, false, "disabled", "Compatibility mode disabled", {}};
    }

    constexpr const char* kOpenSslConfEnv = "OPENSSL_CONF";
    constexpr const char* kOpenSslCompatPathEnv = "ACCLOUD_MQTT_OPENSSL_CONF_PATH";

    const char* existingConf = std::getenv(kOpenSslConfEnv);
    if (existingConf != nullptr && *existingConf != '\0') {
        return {true, false, "already_configured", "OPENSSL_CONF already set",
                std::string(existingConf)};
    }

    std::filesystem::path confPath;
    const char* overridePath = std::getenv(kOpenSslCompatPathEnv);
    if (overridePath != nullptr && *overridePath != '\0') {
        confPath = std::filesystem::path(overridePath);
    } else {
        std::error_code ec;
        auto tempDir = std::filesystem::temp_directory_path(ec);
        if (ec) {
            return {false, false, "temp_dir_unavailable", ec.message(), {}};
        }
        confPath = tempDir / "accloud_openssl_seclevel0.cnf";
    }

    if (confPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(confPath.parent_path(), ec);
        if (ec) {
            return {false, false, "mkdir_failed", ec.message(), confPath.string()};
        }
    }

    {
        std::ofstream out(confPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return {false, false, "write_failed", "Unable to open OpenSSL compat file",
                    confPath.string()};
        }
        out << "openssl_conf = default_conf\n\n"
               "[default_conf]\n"
               "ssl_conf = ssl_sect\n\n"
               "[ssl_sect]\n"
               "system_default = system_default_sect\n\n"
               "[system_default_sect]\n"
               "CipherString = DEFAULT:@SECLEVEL=0\n";
        out.flush();
        if (!out.good()) {
            return {false, false, "write_failed", "Unable to write OpenSSL compat file",
                    confPath.string()};
        }
    }

#if defined(_WIN32)
    const int setenvRc = _putenv_s(kOpenSslConfEnv, confPath.string().c_str());
    if (setenvRc != 0) {
        return {false, false, "setenv_failed", "_putenv_s failed", confPath.string()};
    }
#else
    const int setenvRc = setenv(kOpenSslConfEnv, confPath.string().c_str(), 0);
    if (setenvRc != 0) {
        return {false, false, "setenv_failed", std::strerror(errno), confPath.string()};
    }
#endif

    return {true, true, "applied", "OPENSSL_CONF configured for SECLEVEL=0", confPath.string()};
}

} // namespace accloud::mqtt::core
