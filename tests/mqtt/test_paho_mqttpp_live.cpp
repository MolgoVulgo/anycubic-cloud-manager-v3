#include "infra/mqtt/core/OpenSslCompat.h"

#include <mqtt/async_client.h>
#include <MQTTAsync.h>

#include <nlohmann/json.hpp>

#include <openssl/bio.h>
#include <openssl/buffer.h>
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

void mqttAsyncTrace(enum MQTTASYNC_TRACE_LEVELS level, char* message) {
    std::cerr << "[PAHO-CPP TRACE] level=" << static_cast<int>(level)
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

    MQTTAsync_setTraceLevel(MQTTASYNC_TRACE_PROTOCOL);
    MQTTAsync_setTraceCallback(mqttAsyncTrace);

    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::filesystem::path sessionPath = root / "session.json";
    const std::string sessionRaw = readTextFile(sessionPath);
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

    const std::filesystem::path caPath = tlsPath(root,
                                                 "ACCLOUD_MQTT_TLS_CA_PATH",
                                                 "anycubic_mqtt_tls_ca.crt",
                                                 "anycubic_mqqt_tls_ca.crt");
    const std::filesystem::path certPath = tlsPath(root,
                                                   "ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH",
                                                   "anycubic_mqtt_tls_client.crt",
                                                   "anycubic_mqqt_tls_client.crt");
    const std::filesystem::path keyPath = tlsPath(root,
                                                  "ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH",
                                                  "anycubic_mqtt_tls_client.key",
                                                  "anycubic_mqqt_tls_client.key");
    if (!std::filesystem::exists(caPath) || !std::filesystem::exists(certPath)
        || !std::filesystem::exists(keyPath)) {
        std::cerr << "[FAIL] Missing TLS material files\n";
        return 2;
    }

    const std::string caPem = readTextFile(caPath);
    auto encrypted = rsaEncryptPkcs1v15(caPem, authToken);
    if (!encrypted.has_value()) {
        std::cerr << "[FAIL] RSA PKCS1 v1.5 encryption failed\n";
        return 2;
    }
    const std::string mqttToken = base64Encode(*encrypted);
    const std::string clientId = md5LowerHex(email + "pcf");
    const std::string signature = md5LowerHex(clientId + mqttToken + clientId);
    const std::string username = "user|pcf|" + email + "|" + signature;

    mqtt::async_client cli(kBroker, clientId);

    mqtt::ssl_options sslopts;
    sslopts.set_trust_store(caPath.string());
    sslopts.set_key_store(certPath.string());
    sslopts.set_private_key(keyPath.string());
    sslopts.set_enabled_cipher_suites("ALL:@SECLEVEL=0");
    sslopts.set_enable_server_cert_auth(false);
    sslopts.set_verify(false);
    sslopts.set_ssl_version(MQTT_SSL_VERSION_TLS_1_2);

    mqtt::connect_options conn;
    conn.set_mqtt_version(MQTTVERSION_3_1_1);
    conn.set_keep_alive_interval(kKeepAlive);
    conn.set_clean_session(true);
    conn.set_ssl(sslopts);
    conn.set_user_name(username);
    conn.set_password(mqttToken);

    try {
        auto tok = cli.connect(conn);
        tok->wait_for(std::chrono::seconds(15));
        if (!tok->is_complete()) {
            std::cerr << "[FAIL] PAHO connect timeout\n";
            return 1;
        }

        if (!userId.empty()) {
            const std::string userTopic =
                "anycubic/anycubicCloud/v1/server/app/" + userId + "/" + md5LowerHex(userId)
                + "/slice/report";
            auto stok = cli.subscribe(userTopic, 0);
            stok->wait_for(std::chrono::seconds(10));
        }

        cli.disconnect()->wait();
        std::cout << "[PASS] PAHO MQTT C++ live connection succeeded\n";
        return 0;
    } catch (const mqtt::exception& ex) {
        std::cerr << "[FAIL] PAHO MQTT exception code=" << ex.get_reason_code()
                  << " what=" << ex.what() << '\n';
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] std::exception " << ex.what() << '\n';
        return 1;
    }
}
