#include "MqttCredentialProvider.h"

#include "infra/logging/JsonlLogger.h"

#if defined(ACCLOUD_WITH_OPENSSL)
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#endif

#if defined(__linux__)
#include <crypt.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace accloud::mqtt::core {
namespace {

constexpr const char* kEnvMqttAuthMode = "ACCLOUD_MQTT_AUTH_MODE";
constexpr const char* kEnvMqttCompatMode = "ACCLOUD_MQTT_ANDROID_COMPAT";
constexpr const char* kEnvMqttCaPath = "ACCLOUD_MQTT_TLS_CA_PATH";
constexpr const char* kEnvMqttTlsDevFallback = "ACCLOUD_MQTT_TLS_DEV_FALLBACK";

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trimAscii(std::string value) {
    auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool parseBoolEnv(const char* key, bool defaultValue = false) {
    const char* raw = std::getenv(key);
    if (raw == nullptr || *raw == '\0') {
        return defaultValue;
    }
    const std::string lowered = toLowerAscii(raw);
    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

std::string getEnvString(const char* key) {
    const char* raw = std::getenv(key);
    if (raw == nullptr) {
        return {};
    }
    return trimAscii(std::string(raw));
}

std::filesystem::path repositoryRootFromSource() {
    std::filesystem::path p(__FILE__);
    p = p.parent_path();
    for (int i = 0; i < 8 && !p.empty(); ++i) {
        if (std::filesystem::exists(p / "accloud" / "resources" / "mqtt" / "tls")) {
            return p;
        }
        p = p.parent_path();
    }
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
}

std::filesystem::path defaultCaPathFromRepo() {
    const std::filesystem::path root = repositoryRootFromSource();
    const std::filesystem::path preferred =
        root / "accloud" / "resources" / "mqtt" / "tls" / "anycubic_mqtt_tls_ca.crt";
    if (std::filesystem::exists(preferred)) {
        return preferred;
    }
    const std::filesystem::path legacy =
        root / "accloud" / "resources" / "mqtt" / "tls" / "anycubic_mqqt_tls_ca.crt";
    if (std::filesystem::exists(legacy)) {
        return legacy;
    }
    return preferred;
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

#if defined(ACCLOUD_WITH_OPENSSL)
std::string md5LowerHex(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        return {};
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1
        || EVP_DigestUpdate(ctx, input.data(), input.size()) != 1
        || EVP_DigestFinal_ex(ctx, digest, &digestLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return {};
    }
    EVP_MD_CTX_free(ctx);

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(digestLen * 2);
    for (unsigned int i = 0; i < digestLen; ++i) {
        out[2 * i] = kHex[(digest[i] >> 4) & 0xF];
        out[2 * i + 1] = kHex[digest[i] & 0xF];
    }
    return out;
}

std::string base64Encode(const std::vector<unsigned char>& bytes) {
    if (bytes.empty()) {
        return {};
    }
    const int outLen = 4 * ((static_cast<int>(bytes.size()) + 2) / 3);
    std::string out;
    out.resize(outLen);
    const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                                        bytes.data(),
                                        static_cast<int>(bytes.size()));
    if (written <= 0) {
        return {};
    }
    out.resize(static_cast<std::size_t>(written));
    return out;
}

std::optional<std::vector<unsigned char>> rsaEncryptPkcs1v15(const std::string& caPem,
                                                             const std::string& plaintext) {
    BIO* bio = BIO_new_mem_buf(caPem.data(), static_cast<int>(caPem.size()));
    if (bio == nullptr) {
        return std::nullopt;
    }

    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (cert == nullptr) {
        return std::nullopt;
    }

    EVP_PKEY* pubKey = X509_get_pubkey(cert);
    X509_free(cert);
    if (pubKey == nullptr) {
        return std::nullopt;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pubKey, nullptr);
    EVP_PKEY_free(pubKey);
    if (ctx == nullptr) {
        return std::nullopt;
    }
    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return std::nullopt;
    }
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return std::nullopt;
    }

    std::size_t outLen = 0;
    if (EVP_PKEY_encrypt(ctx, nullptr, &outLen,
                         reinterpret_cast<const unsigned char*>(plaintext.data()),
                         plaintext.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return std::nullopt;
    }
    std::vector<unsigned char> out(outLen);
    if (EVP_PKEY_encrypt(ctx, out.data(), &outLen,
                         reinterpret_cast<const unsigned char*>(plaintext.data()),
                         plaintext.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return std::nullopt;
    }
    EVP_PKEY_CTX_free(ctx);
    out.resize(outLen);
    return out;
}

#else
std::string md5LowerHex(const std::string&) {
    return {};
}
#endif

#if defined(__linux__)
std::string randomBcryptSalt22() {
    static constexpr std::array<char, 64> kAlphabet = {
        '.', '/', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
        'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0, static_cast<int>(kAlphabet.size() - 1));
    std::string out;
    out.reserve(22);
    for (int i = 0; i < 22; ++i) {
        out.push_back(kAlphabet[static_cast<std::size_t>(dist(rng))]);
    }
    return out;
}

std::string bcryptHash(const std::string& source, const std::string& prefix, int cost) {
    const std::string salt = "$" + prefix + "$" + (cost < 10 ? "0" : "") + std::to_string(cost)
        + "$" + randomBcryptSalt22();

    struct crypt_data data {};
    data.initialized = 0;
    char* output = crypt_r(source.c_str(), salt.c_str(), &data);
    if (output == nullptr) {
        return {};
    }
    return std::string(output);
}
#else
std::string bcryptHash(const std::string&, const std::string&, int) {
    return {};
}
#endif

} // namespace

MqttAuthMode MqttCredentialProvider::authModeFromString(const std::string& value) {
    const std::string lowered = toLowerAscii(trimAscii(value));
    if (lowered == "android") {
        return MqttAuthMode::Android;
    }
    return MqttAuthMode::Slicer;
}

std::string MqttCredentialProvider::authModeToString(MqttAuthMode mode) {
    switch (mode) {
        case MqttAuthMode::Slicer:
            return "slicer";
        case MqttAuthMode::Android:
            return "android";
    }
    return "slicer";
}

MqttAuthMode MqttCredentialProvider::resolvePreferredMode(const std::optional<std::string>& settingsValue) {
    const std::string envMode = getEnvString(kEnvMqttAuthMode);
    if (!envMode.empty()) {
        return authModeFromString(envMode);
    }
    if (settingsValue.has_value()) {
        return authModeFromString(*settingsValue);
    }
    return MqttAuthMode::Slicer;
}

MqttCredentialBuildResult MqttCredentialProvider::buildCandidates(const MqttCredentialInput& rawInput) {
    MqttCredentialInput input = rawInput;
    input.email = trimAscii(input.email);
    input.userId = trimAscii(input.userId);
    input.authToken = trimAscii(input.authToken);
    input.brokerHost = trimAscii(input.brokerHost);

    if (input.email.empty()) {
        return {false, "mqtt_credentials_missing_email", "Email is required for MQTT credentials", {}};
    }
    if (input.authToken.empty()) {
        return {false, "mqtt_credentials_missing_token", "Auth token is required for MQTT credentials", {}};
    }
    if (input.userId.empty()) {
        return {false, "mqtt_credentials_missing_user_id", "User ID is required for MQTT credentials", {}};
    }

    if (input.authMode == MqttAuthMode::Slicer) {
        auto candidate = buildSlicerCandidate(input);
        if (!candidate.has_value()) {
            return {false, "mqtt_slicer_crypto_failed",
                    "Unable to build Slicer MQTT credentials (RSA/CA prerequisites unmet).", {}};
        }
        return {true, "ok", "MQTT Slicer credentials generated", {*candidate}};
    }

    const bool compatMode = input.compatibilityMode || parseBoolEnv(kEnvMqttCompatMode, false);
    if (auto cached = findCachedAndroidProfile(input); cached.has_value()) {
        auto candidate = buildAndroidCandidate(input, *cached);
        if (candidate.has_value()) {
            return {true, "ok", "MQTT Android credentials generated (cached profile)", {*candidate}};
        }
    }

    std::vector<AndroidCryptoProfile> profiles = {AndroidCryptoProfile::A_2b_12};
    if (compatMode) {
        profiles = {
            AndroidCryptoProfile::A_2b_12,
            AndroidCryptoProfile::B_2a_12,
            AndroidCryptoProfile::C_2b_10,
        };
    }

    std::vector<MqttCredentialCandidate> candidates;
    for (AndroidCryptoProfile profile : profiles) {
        auto candidate = buildAndroidCandidate(input, profile);
        if (candidate.has_value()) {
            candidates.push_back(std::move(*candidate));
        }
    }
    if (candidates.empty()) {
        return {false, "mqtt_android_crypto_failed",
                "Unable to build Android MQTT credentials (bcrypt backend unavailable).", {}};
    }
    return {true, "ok", "MQTT Android credentials generated", std::move(candidates)};
}

void MqttCredentialProvider::rememberSuccessfulAndroidProfile(const MqttCredentialInput& input,
                                                              AndroidCryptoProfile profile) {
    m_androidProfileCache[cacheKey(input)] = profile;
}

void MqttCredentialProvider::clearCachedAndroidProfile(const MqttCredentialInput& input) {
    m_androidProfileCache.erase(cacheKey(input));
}

std::optional<AndroidCryptoProfile> MqttCredentialProvider::findCachedAndroidProfile(
    const MqttCredentialInput& input) const {
    const auto it = m_androidProfileCache.find(cacheKey(input));
    if (it == m_androidProfileCache.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::string MqttCredentialProvider::cacheKey(const MqttCredentialInput& input) {
    return input.brokerHost + "|" + authModeToString(input.authMode) + "|" + input.email;
}

std::optional<MqttCredentialCandidate> MqttCredentialProvider::buildSlicerCandidate(
    const MqttCredentialInput& input) const {
#if !defined(ACCLOUD_WITH_OPENSSL)
    logging::error("cloud", "mqtt_credentials", "openssl_unavailable",
                   "OpenSSL backend is required for Slicer MQTT credentials");
    return std::nullopt;
#else
    std::filesystem::path caPath(getEnvString(kEnvMqttCaPath));
    if (caPath.empty()) {
        const std::filesystem::path repoDefault = defaultCaPathFromRepo();
        if (std::filesystem::exists(repoDefault)) {
            caPath = repoDefault;
        }
    }
    if (caPath.empty() && parseBoolEnv(kEnvMqttTlsDevFallback, false)) {
        caPath = defaultCaPathFromRepo();
    }
    if (caPath.empty()) {
        logging::error("cloud", "mqtt_credentials", "missing_ca_path",
                       "Missing ACCLOUD_MQTT_TLS_CA_PATH for Slicer mode");
        return std::nullopt;
    }

    const std::string caPem = readTextFile(caPath);
    if (caPem.empty()) {
        logging::error("cloud", "mqtt_credentials", "ca_read_failed",
                       "Unable to read MQTT CA certificate",
                       {{"ca_file", caPath.filename().string()}});
        return std::nullopt;
    }

    const auto encrypted = rsaEncryptPkcs1v15(caPem, input.authToken);
    if (!encrypted.has_value()) {
        logging::error("cloud", "mqtt_credentials", "rsa_encrypt_failed",
                       "Unable to encrypt MQTT token with CA public key");
        return std::nullopt;
    }
    const std::string mqttToken = base64Encode(*encrypted);
    if (mqttToken.empty()) {
        return std::nullopt;
    }

    const std::string clientId = buildClientId(input, MqttAuthMode::Slicer);
    const std::string username = buildUsername("pcf", input.email, clientId, mqttToken);
    if (clientId.empty() || username.empty()) {
        return std::nullopt;
    }
    return MqttCredentialCandidate{
        MqttCredentials{clientId, username, mqttToken},
        std::nullopt,
    };
#endif
}

std::optional<MqttCredentialCandidate> MqttCredentialProvider::buildAndroidCandidate(
    const MqttCredentialInput& input,
    AndroidCryptoProfile profile) const {
    const std::string tokenMd5 = md5LowerHex(input.authToken);
    if (tokenMd5.empty()) {
        return std::nullopt;
    }

    std::string prefix = "2b";
    int cost = 12;
    switch (profile) {
        case AndroidCryptoProfile::A_2b_12:
            prefix = "2b";
            cost = 12;
            break;
        case AndroidCryptoProfile::B_2a_12:
            prefix = "2a";
            cost = 12;
            break;
        case AndroidCryptoProfile::C_2b_10:
            prefix = "2b";
            cost = 10;
            break;
    }

    const std::string mqttToken = bcryptHash(tokenMd5, prefix, cost);
    if (mqttToken.empty()) {
        logging::error("cloud", "mqtt_credentials", "bcrypt_failed",
                       "Unable to generate Android MQTT token",
                       {{"profile", androidProfileToString(profile)}});
        return std::nullopt;
    }

    const std::string clientId = buildClientId(input, MqttAuthMode::Android);
    const std::string username = buildUsername("app", input.email, clientId, mqttToken);
    if (clientId.empty() || username.empty()) {
        return std::nullopt;
    }

    return MqttCredentialCandidate{
        MqttCredentials{clientId, username, mqttToken},
        profile,
    };
}

std::string MqttCredentialProvider::buildClientId(const MqttCredentialInput& input,
                                                  MqttAuthMode mode) const {
    if (mode == MqttAuthMode::Slicer) {
        return md5LowerHex(input.email + "pcf");
    }
    return md5LowerHex(input.email);
}

std::string MqttCredentialProvider::buildUsername(const std::string& appId,
                                                  const std::string& email,
                                                  const std::string& clientId,
                                                  const std::string& mqttToken) const {
    const std::string signature = md5LowerHex(clientId + mqttToken + clientId);
    if (signature.empty()) {
        return {};
    }
    return "user|" + appId + "|" + email + "|" + signature;
}

std::string MqttCredentialProvider::androidProfileToString(AndroidCryptoProfile profile) {
    switch (profile) {
        case AndroidCryptoProfile::A_2b_12:
            return "A_2b_12";
        case AndroidCryptoProfile::B_2a_12:
            return "B_2a_12";
        case AndroidCryptoProfile::C_2b_10:
            return "C_2b_10";
    }
    return "A_2b_12";
}

} // namespace accloud::mqtt::core
