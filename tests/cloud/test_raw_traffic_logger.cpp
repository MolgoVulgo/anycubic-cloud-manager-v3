#include "infra/logging/RawTrafficLogger.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

std::optional<std::string> envValue(const char* key) {
    const char* value = std::getenv(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

void restoreEnv(const char* key, const std::optional<std::string>& value) {
    if (value.has_value()) {
        setenv(key, value->c_str(), 1);
    } else {
        unsetenv(key);
    }
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool test_dev_raw_log_captures_http_and_mqtt_with_redaction() {
    const auto previousLogDir = envValue("ACCLOUD_LOG_DIR");
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto pid = static_cast<long long>(::getpid());
    const std::filesystem::path tempRoot = std::filesystem::temp_directory_path()
        / ("accloud_raw_traffic_test_" + std::to_string(pid) + "_" + std::to_string(now));

    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot, ec);
    if (ec) {
        return expect(false, "Cannot create temporary raw-log directory");
    }
    setenv("ACCLOUD_LOG_DIR", tempRoot.string().c_str(), 1);

    const std::string id = accloud::logging::raw::nextCorrelationId("http");
    accloud::logging::raw::logHttpRequest(
        id,
        "POST",
        "https://cloud.example.test/files?X-Amz-Signature=raw-signature&token=url-token",
        {{"Content-Type", "application/json"}, {"Authorization", "Bearer header-secret"}},
        R"json({"name":"cube.pwsz","access_token":"body-secret","thumbnail":"https://bucket.example.test/cloud/cube.jpg?X-Amz-Signature=image-secret","user_id":94829})json");
    accloud::logging::raw::logHttpResponse(
        id,
        200,
        "OK",
        {{"Set-Cookie", "session=raw-cookie"}},
        R"json({"code":1,"data":{"filename":"cube.pwsz","refresh_token":"response-secret"}})json");
    accloud::logging::raw::logMqttMessage(
        "RX",
        "/v1/server/printer/status/report",
        R"json({"action":202,"state":"printing","password":"mqtt-secret"})json");

    const std::filesystem::path rawPath = tempRoot / "log_brut.txt";
    const std::string content = readFile(rawPath);

    restoreEnv("ACCLOUD_LOG_DIR", previousLogDir);
    std::filesystem::remove_all(tempRoot, ec);

    return expect(!content.empty(), "Raw log file should be created in dev mode")
        && expect(content.find("HTTP REQUEST") != std::string::npos,
                  "Raw log should contain the HTTP request")
        && expect(content.find("HTTP RESPONSE") != std::string::npos,
                  "Raw log should contain the HTTP response")
        && expect(content.find("MQTT MESSAGE") != std::string::npos,
                  "Raw log should contain the MQTT payload")
        && expect(content.find("cube.pwsz") != std::string::npos,
                  "Non-sensitive raw fields should remain visible")
        && expect(content.find("/cloud/cube.jpg") != std::string::npos,
                  "Signed image path should remain visible")
        && expect(content.find("raw-signature") == std::string::npos,
                  "Signed URL query must be removed")
        && expect(content.find("header-secret") == std::string::npos,
                  "Authorization header must be redacted")
        && expect(content.find("body-secret") == std::string::npos,
                  "Request token must be redacted")
        && expect(content.find("response-secret") == std::string::npos,
                  "Response token must be redacted")
        && expect(content.find("mqtt-secret") == std::string::npos,
                  "MQTT credential-like fields must be redacted")
        && expect(content.find("94829") == std::string::npos,
                  "Persistent user identifiers must be redacted");
}

} // namespace

int main() {
    if (!test_dev_raw_log_captures_http_and_mqtt_with_redaction()) {
        return 1;
    }
    std::cout << "Raw traffic logger tests passed\n";
    return 0;
}
