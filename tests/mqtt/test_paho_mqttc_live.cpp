#include "infra/mqtt/core/OpenSslCompat.h"

#include <MQTTClient.h>

#include <nlohmann/json.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const char* kBroker = "ssl://mqtt-universe.anycubic.com:8883";
constexpr int kKeepAlive = 1200;

void mqttTrace(enum MQTTCLIENT_TRACE_LEVELS level, char* message) {
    std::cerr << "[PAHO-C TRACE] level=" << static_cast<int>(level)
              << " msg=" << (message ? message : "") << '\n';
}

std::string trimAscii(std::string value) {
    auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string md5LowerHex(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) return {};
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    const bool ok = EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) == 1
        && EVP_DigestUpdate(ctx, input.data(), input.size()) == 1
        && EVP_DigestFinal_ex(ctx, digest, &len) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok) return {};
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(len * 2, '\0');
    for (unsigned int i = 0; i < len; ++i) {
        out[i * 2] = kHex[(digest[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    return out;
}

std::string readTextFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in.is_open()) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::optional<std::vector<unsigned char>> rsaEncryptPkcs1v15(const std::string& certPem,
                                                             const std::string& plaintext) {
    BIO* bio = BIO_new_mem_buf(certPem.data(), static_cast<int>(certPem.size()));
    if (bio == nullptr) return std::nullopt;
    X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (cert == nullptr) return std::nullopt;
    EVP_PKEY* pub = X509_get_pubkey(cert);
    X509_free(cert);
    if (pub == nullptr) return std::nullopt;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pub, nullptr);
    EVP_PKEY_free(pub);
    if (ctx == nullptr) return std::nullopt;
    if (EVP_PKEY_encrypt_init(ctx) <= 0
        || EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
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

std::string base64Encode(const std::vector<unsigned char>& in) {
    if (in.empty()) return {};
    std::string out(4 * ((static_cast<int>(in.size()) + 2) / 3), '\0');
    const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                                        in.data(),
                                        static_cast<int>(in.size()));
    if (written <= 0) return {};
    out.resize(static_cast<std::size_t>(written));
    return out;
}

std::filesystem::path tlsPath(const std::filesystem::path& root,
                              const char* envKey,
                              const char* preferred,
                              const char* legacy) {
    const char* env = std::getenv(envKey);
    if (env != nullptr && *env != '\0') {
        return std::filesystem::path(env);
    }
    const auto preferredPath = root / "resources" / "mqtt" / "tls" / preferred;
    if (std::filesystem::exists(preferredPath)) return preferredPath;
    return root / "resources" / "mqtt" / "tls" / legacy;
}

} // namespace

int main() {
    const auto opensslCompat = accloud::mqtt::core::ensureOpenSslSecurityLevelCompat(true);
    if (!opensslCompat.ok) {
        std::cerr << "[WARN] OpenSSL compat profile not applied code=" << opensslCompat.code
                  << " detail=" << opensslCompat.message << '\n';
    }

    MQTTClient_setTraceLevel(MQTTCLIENT_TRACE_PROTOCOL);
    MQTTClient_setTraceCallback(mqttTrace);

    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::string sessionRaw = readTextFile(root / "session.json");
    if (sessionRaw.empty()) {
        std::cerr << "[FAIL] Missing session.json\n";
        return 2;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(sessionRaw);
    } catch (...) {
        std::cerr << "[FAIL] Invalid session.json\n";
        return 2;
    }
    const auto& tokens = j.value("tokens", nlohmann::json::object());
    std::string email = trimAscii(tokens.value("email", ""));
    std::string authToken = trimAscii(tokens.value("auth_token", ""));
    std::string userId = trimAscii(tokens.value("user_id", ""));
    if (userId.empty()) userId = trimAscii(tokens.value("uid", ""));
    if (email.empty() || authToken.empty()) {
        std::cerr << "[FAIL] Missing email/auth_token in session\n";
        return 2;
    }

    const auto caPath = tlsPath(root, "ACCLOUD_MQTT_TLS_CA_PATH",
                                "anycubic_mqtt_tls_ca.crt", "anycubic_mqqt_tls_ca.crt");
    const auto certPath = tlsPath(root, "ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH",
                                  "anycubic_mqtt_tls_client.crt", "anycubic_mqqt_tls_client.crt");
    const auto keyPath = tlsPath(root, "ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH",
                                 "anycubic_mqtt_tls_client.key", "anycubic_mqqt_tls_client.key");
    if (!std::filesystem::exists(caPath) || !std::filesystem::exists(certPath)
        || !std::filesystem::exists(keyPath)) {
        std::cerr << "[FAIL] Missing TLS files\n";
        return 2;
    }

    const std::string caPem = readTextFile(caPath);
    auto encrypted = rsaEncryptPkcs1v15(caPem, authToken);
    if (!encrypted.has_value()) {
        std::cerr << "[FAIL] RSA encryption failed\n";
        return 2;
    }
    const std::string mqttToken = base64Encode(*encrypted);
    const std::string clientId = md5LowerHex(email + "pcf");
    const std::string username = "user|pcf|" + email + "|" + md5LowerHex(clientId + mqttToken + clientId);

    MQTTClient client = nullptr;
    const int createRc = MQTTClient_create(&client, kBroker, clientId.c_str(),
                                           MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (createRc != MQTTCLIENT_SUCCESS) {
        std::cerr << "[FAIL] MQTTClient_create rc=" << createRc << '\n';
        return 1;
    }

    MQTTClient_SSLOptions sslopts = MQTTClient_SSLOptions_initializer;
    sslopts.trustStore = caPath.c_str();
    sslopts.keyStore = certPath.c_str();
    sslopts.privateKey = keyPath.c_str();
    sslopts.enabledCipherSuites = "ALL:@SECLEVEL=0";
    sslopts.enableServerCertAuth = 0;
    sslopts.sslVersion = MQTT_SSL_VERSION_TLS_1_2;
    sslopts.verify = 0;

    MQTTClient_connectOptions conn = MQTTClient_connectOptions_initializer;
    conn.keepAliveInterval = kKeepAlive;
    conn.cleansession = 1;
    conn.username = username.c_str();
    conn.password = mqttToken.c_str();
    conn.MQTTVersion = MQTTVERSION_3_1_1;
    conn.ssl = &sslopts;

    const int rc = MQTTClient_connect(client, &conn);
    if (rc != MQTTCLIENT_SUCCESS) {
        std::cerr << "[FAIL] MQTTClient_connect rc=" << rc << '\n';
        MQTTClient_destroy(&client);
        return 1;
    }

    if (!userId.empty()) {
        const std::string topic =
            "anycubic/anycubicCloud/v1/server/app/" + userId + "/" + md5LowerHex(userId) + "/slice/report";
        const int subRc = MQTTClient_subscribe(client, topic.c_str(), 0);
        if (subRc != MQTTCLIENT_SUCCESS) {
            std::cerr << "[FAIL] MQTTClient_subscribe rc=" << subRc << '\n';
            MQTTClient_disconnect(client, 1000);
            MQTTClient_destroy(&client);
            return 1;
        }
    }

    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
    std::cout << "[PASS] PAHO MQTT C live connection succeeded\n";
    return 0;
}
