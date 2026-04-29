#include "app/LocalCacheStore.h"
#include "app/usecases/cloud/OrderResponseTracker.h"
#include "infra/cloud/core/ResponseEnvelopeParser.h"

#include <QCoreApplication>
#include <QVariantList>
#include <QVariantMap>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
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

bool test_response_envelope_parser_contract() {
    accloud::cloud::core::ResponseEnvelopeParser parser;
    const auto ok = parser.parse(R"json({"code":1,"msg":"ok","data":{"a":1}})json");
    const auto badJson = parser.parse("{invalid");
    const auto missingCode = parser.parse(R"json({"msg":"x","data":{}})json");

    return expect(ok.jsonValid, "valid JSON should be marked jsonValid")
        && expect(ok.envelopePresent, "valid envelope should be present")
        && expect(ok.success, "code=1 should mark success")
        && expect(ok.code == 1, "code should be parsed")
        && expect(ok.message == "ok", "message should be parsed")
        && expect(badJson.error == "invalid_json", "invalid JSON error expected")
        && expect(missingCode.error == "missing_code", "missing code error expected");
}

bool test_order_response_tracker_lifecycle() {
    using namespace accloud::usecases::cloud;
    auto& tracker = OrderResponseTracker::instance();
    tracker.clear();

    TrackerOpenRequest req;
    req.printerId = "p-1";
    req.correlationClass = CorrelationClass::PrintStart;
    req.msgid = "m-1";
    req.timeout = std::chrono::milliseconds(500);
    const auto open = tracker.open(req);
    if (!expect(open.ok, "open should succeed")) return false;

    const auto dup = tracker.open(req);
    if (!expect(!dup.ok, "duplicate msgid should be rejected")) return false;

    const auto resolved = tracker.resolveByMsgid("m-1", true, "ok");
    if (!expect(resolved.outcome == CorrelationOutcome::Success, "resolve by msgid should succeed")) return false;
    if (!expect(tracker.pendingCount() == 0, "pending count should be 0 after resolve")) return false;

    TrackerOpenRequest fallbackReq;
    fallbackReq.printerId = "p-2";
    fallbackReq.correlationClass = CorrelationClass::ListLocalFiles;
    fallbackReq.msgid.clear();
    fallbackReq.timeout = std::chrono::milliseconds(50);
    const auto openFallback = tracker.open(fallbackReq);
    if (!expect(openFallback.ok, "fallback open should succeed")) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    const auto expired = tracker.expireTimeouts();
    if (!expect(expired >= 1, "timeout expiration should remove request")) return false;

    tracker.clear();
    TrackerOpenRequest a;
    a.printerId = "p-3";
    a.correlationClass = CorrelationClass::DeleteLocalFile;
    a.msgid = "x-1";
    a.timeout = std::chrono::milliseconds(500);
    TrackerOpenRequest b;
    b.printerId = "p-3";
    b.correlationClass = CorrelationClass::DeleteLocalFile;
    b.msgid = "x-2";
    b.timeout = std::chrono::milliseconds(500);
    if (!expect(tracker.open(a).ok && tracker.open(b).ok, "two msgid-tracked commands should open")) return false;
    const auto ambiguous = tracker.resolveByFallback("p-3", CorrelationClass::DeleteLocalFile, true, "fallback");
    const bool okAmbiguous = expect(ambiguous.outcome == CorrelationOutcome::AmbiguousFallback,
                                    "fallback should be ambiguous with multiple candidates");
    tracker.clear();
    return okAmbiguous;
}

bool test_local_cache_store_roundtrip_and_sync_state() {
    const auto previousDbPath = envValue("ACCLOUD_DB_PATH");
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto pid = static_cast<long long>(::getpid());
    const std::filesystem::path dbPath = std::filesystem::temp_directory_path()
        / ("accloud_cache_test_" + std::to_string(pid) + "_" + std::to_string(now) + ".db");
    setenv("ACCLOUD_DB_PATH", dbPath.string().c_str(), 1);

    accloud::LocalCacheStore cache;
    if (!expect(cache.isAvailable(), "cache should be available")) {
        restoreEnv("ACCLOUD_DB_PATH", previousDbPath);
        return false;
    }

    QVariantMap quota;
    quota.insert("usedBytes", 100);
    quota.insert("totalBytes", 1000);

    bool ok = expect(cache.saveQuota(quota), "saveQuota should succeed");

    const QVariantMap loadedQuota = cache.loadQuota();
    ok = ok
        && expect(loadedQuota.value("usedBytes").toInt() == 100, "loadQuota usedBytes mismatch");

    QVariantMap printer;
    printer.insert("id", "printer-1");
    printer.insert("printerKey", "printer-key-1");
    printer.insert("machineType", "128");
    printer.insert("name", "Printer One");
    printer.insert("model", "Mono M7");
    printer.insert("type", "LCD");
    printer.insert("lastSeen", "now");
    printer.insert("state", "READY");
    printer.insert("reason", "free");
    printer.insert("available", 1);
    printer.insert("currentFile", "demo.pwmb");

    QVariantList printers;
    printers.append(printer);
    ok = ok && expect(cache.replacePrinters(printers), "replacePrinters should succeed");

    const QVariantList loadedPrinters = cache.loadPrinters();
    ok = ok && expect(loadedPrinters.size() == 1, "loadPrinters should return one printer");
    if (!loadedPrinters.isEmpty()) {
        const QVariantMap p = loadedPrinters.first().toMap();
        ok = ok && expect(p.value("id").toString() == "printer-1", "printer id mismatch");
        ok = ok && expect(p.value("printerKey").toString() == "printer-key-1", "printerKey mismatch");
        ok = ok && expect(p.value("machineType").toString() == "128", "machineType mismatch");
        ok = ok && expect(p.value("state").toString() == "READY", "printer state mismatch");
    }

    QVariantMap oldJob;
    oldJob.insert("taskId", "task-old");
    oldJob.insert("printerId", "printer-1");
    oldJob.insert("printerName", "Printer One");
    oldJob.insert("gcodeName", "old.pwmb");
    oldJob.insert("printStatus", 2);
    oldJob.insert("progress", 100);
    oldJob.insert("elapsedSec", 3600);
    oldJob.insert("remainingSec", 0);
    oldJob.insert("currentLayer", 50);
    oldJob.insert("totalLayers", 50);
    oldJob.insert("currentFile", "old.pwmb");
    oldJob.insert("reason", "done");
    oldJob.insert("createTime", 10);
    oldJob.insert("endTime", 20);
    oldJob.insert("img", "old.png");

    QVariantMap updatedJob;
    updatedJob.insert("taskId", "task-active");
    updatedJob.insert("printerId", "printer-1");
    updatedJob.insert("printerName", "Printer One");
    updatedJob.insert("gcodeName", "active-v1.pwmb");
    updatedJob.insert("printStatus", 1);
    updatedJob.insert("progress", 20);
    updatedJob.insert("elapsedSec", 120);
    updatedJob.insert("remainingSec", 480);
    updatedJob.insert("currentLayer", 2);
    updatedJob.insert("totalLayers", 10);
    updatedJob.insert("currentFile", "active-v1.pwmb");
    updatedJob.insert("reason", "printing");
    updatedJob.insert("createTime", 30);
    updatedJob.insert("endTime", 0);
    updatedJob.insert("img", "active-v1.png");

    QVariantList initialJobs;
    initialJobs.append(oldJob);
    initialJobs.append(updatedJob);
    ok = ok && expect(cache.replaceJobsForPrinter("printer-1", initialJobs),
                      "replaceJobsForPrinter should seed jobs");

    QVariantMap newJob;
    newJob.insert("taskId", "task-new");
    newJob.insert("printerName", "Printer One");
    newJob.insert("gcodeName", "new.pwmb");
    newJob.insert("printStatus", 1);
    newJob.insert("progress", 5);
    newJob.insert("elapsedSec", 30);
    newJob.insert("remainingSec", 900);
    newJob.insert("currentLayer", 1);
    newJob.insert("totalLayers", 80);
    newJob.insert("currentFile", "new.pwmb");
    newJob.insert("reason", "printing");
    newJob.insert("createTime", 40);
    newJob.insert("endTime", 0);
    newJob.insert("img", "new.png");

    updatedJob.insert("gcodeName", "active-v2.pwmb");
    updatedJob.insert("progress", 55);
    updatedJob.insert("elapsedSec", 300);
    updatedJob.insert("remainingSec", 240);
    updatedJob.insert("currentLayer", 6);
    updatedJob.insert("currentFile", "active-v2.pwmb");
    updatedJob.insert("img", "active-v2.png");

    QVariantList incrementalJobs;
    incrementalJobs.append(newJob);
    incrementalJobs.append(updatedJob);
    ok = ok && expect(cache.upsertJobsForPrinter("printer-1", incrementalJobs),
                      "upsertJobsForPrinter should merge jobs");

    const QVariantList loadedJobs = cache.loadJobsForPrinter("printer-1", 1, 20);
    ok = ok && expect(loadedJobs.size() == 3, "incremental job upsert should preserve older jobs");

    bool sawOld = false;
    bool sawUpdated = false;
    bool sawNew = false;
    for (const QVariant& loadedJobVariant : loadedJobs) {
        const QVariantMap job = loadedJobVariant.toMap();
        const QString taskId = job.value("taskId").toString();
        if (taskId == "task-old") {
            sawOld = true;
        } else if (taskId == "task-active") {
            sawUpdated = true;
            ok = ok && expect(job.value("gcodeName").toString() == "active-v2.pwmb",
                              "updated job gcodeName mismatch");
            ok = ok && expect(job.value("progress").toInt() == 55,
                              "updated job progress mismatch");
            ok = ok && expect(job.value("elapsedSec").toInt() == 300,
                              "updated job elapsedSec mismatch");
            ok = ok && expect(job.value("remainingSec").toInt() == 240,
                              "updated job remainingSec mismatch");
            ok = ok && expect(job.value("currentLayer").toInt() == 6,
                              "updated job currentLayer mismatch");
            ok = ok && expect(job.value("totalLayers").toInt() == 10,
                              "updated job totalLayers mismatch");
            ok = ok && expect(job.value("currentFile").toString() == "active-v2.pwmb",
                              "updated job currentFile mismatch");
        } else if (taskId == "task-new") {
            sawNew = true;
            ok = ok && expect(job.value("printerId").toString() == "printer-1",
                              "new job printerId should come from upsert scope");
        }
    }
    ok = ok && expect(sawOld, "old cached job should be preserved");
    ok = ok && expect(sawUpdated, "updated job should be present");
    ok = ok && expect(sawNew, "new job should be present");

    restoreEnv("ACCLOUD_DB_PATH", previousDbPath);
    std::error_code ec;
    std::filesystem::remove(dbPath, ec);
    std::filesystem::remove(dbPath.string() + "-wal", ec);
    std::filesystem::remove(dbPath.string() + "-shm", ec);
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok = test_response_envelope_parser_contract() && ok;
    ok = test_order_response_tracker_lifecycle() && ok;
    ok = test_local_cache_store_roundtrip_and_sync_state() && ok;
    if (!ok) {
        return 1;
    }
    std::cout << "Cloud core regression tests passed\n";
    return 0;
}
