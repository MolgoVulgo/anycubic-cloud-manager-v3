#include "app/LogBridge.h"

#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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

bool writeJsonLine(const std::filesystem::path& path, const std::string& line) {
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << line << '\n';
    return file.good();
}

bool test_log_bridge_reads_jsonl_files_from_accloud_log_dir() {
    const auto previousLogDir = envValue("ACCLOUD_LOG_DIR");

    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto pid = static_cast<long long>(::getpid());
    const std::filesystem::path tempRoot = std::filesystem::temp_directory_path()
        / ("accloud_log_bridge_test_" + std::to_string(pid) + "_" + std::to_string(now));
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot, ec);
    if (ec) {
        return expect(false, "Cannot create temporary log directory");
    }

    const bool appOk = writeJsonLine(
        tempRoot / "app.jsonl",
        R"json({"ts":"2026-03-14T10:00:00.000+01:00","level":"INFO","source":"app","component":"bootstrap","event":"startup","message":"boot"})json");
    const bool mqttOk = writeJsonLine(
        tempRoot / "mqtt.jsonl",
        R"json({"ts":"2026-03-14T10:00:01.000+01:00","level":"INFO","source":"mqtt","component":"mqtt_flow","event":"topic_routed","message":"routed"})json");
    const bool cloudOk = writeJsonLine(
        tempRoot / "cloud.jsonl",
        R"json({"ts":"2026-03-14T10:00:02.000+01:00","level":"WARN","source":"cloud","component":"session","event":"refresh","message":"retry"})json");
    const bool rotatedOk = writeJsonLine(
        tempRoot / "mqtt.jsonl.1",
        R"json({"ts":"2026-03-14T10:00:03.000+01:00","level":"INFO","source":"mqtt","component":"mqtt_flow","event":"topic_routed","message":"rotated"})json");
    const bool invalidOk = writeJsonLine(
        tempRoot / "fault.jsonl",
        "NOT_JSON_LINE_SHOULD_NOT_BREAK_LOG_SNAPSHOT");

    if (!expect(appOk && mqttOk && cloudOk && rotatedOk && invalidOk, "Cannot create fixture log files")) {
        restoreEnv("ACCLOUD_LOG_DIR", previousLogDir);
        return false;
    }

    setenv("ACCLOUD_LOG_DIR", tempRoot.string().c_str(), 1);

    accloud::LogBridge bridge;
    const QVariantMap snapshot = bridge.fetchSnapshot(100);

    restoreEnv("ACCLOUD_LOG_DIR", previousLogDir);
    std::filesystem::remove_all(tempRoot, ec);

    const bool okFlag = snapshot.value("ok").toBool();
    const QVariantList entries = snapshot.value("entries").toList();
    const QStringList sources = snapshot.value("sources").toStringList();
    const int totalEntries = snapshot.value("totalEntries").toInt();
    const QString snapshotLogDir = snapshot.value("logDir").toString();

    bool hasApp = false;
    bool hasMqtt = false;
    bool hasCloud = false;
    for (const auto& source : sources) {
        if (source == "app") hasApp = true;
        if (source == "mqtt") hasMqtt = true;
        if (source == "cloud") hasCloud = true;
    }

    return expect(okFlag, "Log snapshot should be ok")
        && expect(snapshotLogDir == QString::fromStdString(tempRoot.string()),
                  "Snapshot logDir should match ACCLOUD_LOG_DIR")
        && expect(totalEntries >= 4, "Log snapshot should include base + rotated fixture lines")
        && expect(entries.size() >= 4, "Entries list should include base + rotated fixture lines")
        && expect(hasApp, "Sources should include app sink")
        && expect(hasMqtt, "Sources should include mqtt sink")
        && expect(hasCloud, "Sources should include cloud sink");
}

} // namespace

int main() {
    bool ok = true;
    ok = test_log_bridge_reads_jsonl_files_from_accloud_log_dir() && ok;
    if (!ok) {
        return 1;
    }
    std::cout << "Log bridge tests passed\n";
    return 0;
}
