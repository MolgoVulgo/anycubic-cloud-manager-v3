#include "app/LocalCacheStore.h"
#include "app/CloudFilesWorkflowBridge.h"
#include "app/PrintWorkflowBridge.h"
#include "app/printing/PendingDirectPrintStore.h"
#include "app/usecases/cloud/OrderResponseTracker.h"
#include "app/usecases/cloud/UploadLocalFileUseCase.h"
#include "infra/config/AppPaths.h"
#include "infra/cloud/core/ResponseEnvelopeParser.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
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


std::filesystem::path uniqueTempDbPath(const std::string& prefix) {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto pid = static_cast<long long>(::getpid());
    return std::filesystem::temp_directory_path()
        / (prefix + "_" + std::to_string(pid) + "_" + std::to_string(now) + ".db");
}

void removeSqliteFiles(const std::filesystem::path& dbPath) {
    std::error_code ec;
    std::filesystem::remove(dbPath, ec);
    std::filesystem::remove(dbPath.string() + "-wal", ec);
    std::filesystem::remove(dbPath.string() + "-shm", ec);
}

bool createSchemaV3Database(const std::filesystem::path& dbPath) {
    const QString connectionName = QStringLiteral("accloud_schema_v3_seed_")
        + QString::number(static_cast<qlonglong>(::getpid()));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QString::fromStdString(dbPath.string()));
        if (!db.open()) {
            std::cerr << "FAILED: unable to create schema v3 database: "
                      << db.lastError().text().toStdString() << '\n';
        } else {
            const QStringList statements = {
                QStringLiteral("CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
                QStringLiteral("INSERT INTO meta(key, value) VALUES('schema_version', '3')"),
                QStringLiteral(
                    "CREATE TABLE cloud_files ("
                    "file_id TEXT PRIMARY KEY, file_name TEXT NOT NULL DEFAULT '', "
                    "status TEXT NOT NULL DEFAULT 'UNKNOWN', size_bytes INTEGER NOT NULL DEFAULT 0, "
                    "size_text TEXT NOT NULL DEFAULT '', machine TEXT NOT NULL DEFAULT '', "
                    "material TEXT NOT NULL DEFAULT '', upload_time TEXT NOT NULL DEFAULT '', "
                    "print_time TEXT NOT NULL DEFAULT '', layer_thickness TEXT NOT NULL DEFAULT '', "
                    "layers INTEGER NOT NULL DEFAULT 0, is_pwmb INTEGER NOT NULL DEFAULT 0, "
                    "resin_usage TEXT NOT NULL DEFAULT '', dimensions TEXT NOT NULL DEFAULT '', "
                    "thumbnail_url TEXT NOT NULL DEFAULT '', gcode_id TEXT NOT NULL DEFAULT '', "
                    "updated_at INTEGER NOT NULL)"),
                QStringLiteral(
                    "INSERT INTO cloud_files(file_id, file_name, status, thumbnail_url, updated_at) "
                    "VALUES('legacy-file', 'legacy.pwsz', 'READY', '', 1)")};
            ok = true;
            QSqlQuery query(db);
            for (const QString& statement : statements) {
                if (!query.exec(statement)) {
                    std::cerr << "FAILED: schema v3 seed statement failed: "
                              << query.lastError().text().toStdString() << '\n';
                    ok = false;
                    break;
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

bool verifySchemaV5Database(const std::filesystem::path& dbPath) {
    const QString connectionName = QStringLiteral("accloud_schema_v5_verify_")
        + QString::number(static_cast<qlonglong>(::getpid()));
    bool versionOk = false;
    bool hasStatusCode = false;
    bool hasThumbnailSource = false;
    bool hasJobCloudFileId = false;
    bool hasJobGcodeId = false;
    bool hasPendingDirectPrints = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QString::fromStdString(dbPath.string()));
        if (db.open()) {
            QSqlQuery version(db);
            if (version.exec(QStringLiteral(
                    "SELECT value FROM meta WHERE key='schema_version'"))
                && version.next()) {
                versionOk = version.value(0).toInt() == 5;
            }
            version.finish();

            QSqlQuery columns(db);
            if (columns.exec(QStringLiteral("PRAGMA table_info(cloud_files)"))) {
                while (columns.next()) {
                    const QString name = columns.value(1).toString();
                    hasStatusCode = hasStatusCode || name == QStringLiteral("status_code");
                    hasThumbnailSource = hasThumbnailSource
                        || name == QStringLiteral("thumbnail_source_url");
                }
            }
            columns.finish();

            QSqlQuery jobColumns(db);
            if (jobColumns.exec(QStringLiteral("PRAGMA table_info(jobs)"))) {
                while (jobColumns.next()) {
                    const QString name = jobColumns.value(1).toString();
                    hasJobCloudFileId = hasJobCloudFileId
                        || name == QStringLiteral("cloud_file_id");
                    hasJobGcodeId = hasJobGcodeId
                        || name == QStringLiteral("gcode_id");
                }
            }
            jobColumns.finish();

            QSqlQuery pendingTable(db);
            if (pendingTable.exec(QStringLiteral(
                    "SELECT name FROM sqlite_master WHERE type='table' "
                    "AND name='pending_direct_prints'"))) {
                hasPendingDirectPrints = pendingTable.next();
            }
            pendingTable.finish();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return versionOk && hasStatusCode && hasThumbnailSource
        && hasJobCloudFileId && hasJobGcodeId && hasPendingDirectPrints;
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


bool test_upload_readiness_rejects_zero_sentinel() {
    using accloud::usecases::cloud::UploadLocalFileUseCase;
    return expect(!UploadLocalFileUseCase::hasUsableGcodeId(""),
                  "empty gcode id should be unusable")
        && expect(!UploadLocalFileUseCase::hasUsableGcodeId("0"),
                  "zero gcode id should be a sentinel")
        && expect(!UploadLocalFileUseCase::hasUsableGcodeId(" 0 "),
                  "trimmed zero gcode id should be a sentinel")
        && expect(UploadLocalFileUseCase::hasUsableGcodeId("42"),
                  "non-zero gcode id should be usable")
        && expect(!UploadLocalFileUseCase::isUploadReady(2, "0"),
                  "PROCESSING with gcode id zero must not be READY")
        && expect(UploadLocalFileUseCase::isUploadReady(1, "0"),
                  "status 1 should be READY")
        && expect(UploadLocalFileUseCase::isUploadReady(2, "gcode-42"),
                  "a usable gcode id should mark the upload ready");
}

bool test_local_cache_store_roundtrip_and_sync_state() {
    const auto previousDbPath = envValue("ACCLOUD_DB_PATH");
    const std::filesystem::path dbPath = uniqueTempDbPath("accloud_cache_test");
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

    QVariantMap cloudFile;
    cloudFile.insert("fileId", "file-processing");
    cloudFile.insert("fileName", "processing.pwsz");
    cloudFile.insert("status", "PROCESSING");
    cloudFile.insert("statusCode", 2);
    cloudFile.insert("thumbnailUrl", "");
    cloudFile.insert("thumbnailSourceUrl", "https://example.invalid/preview.jpg");
    cloudFile.insert("gcodeId", "");
    QVariantList cloudFiles;
    cloudFiles.append(cloudFile);
    ok = ok && expect(cache.replaceFiles(cloudFiles), "replaceFiles should succeed");
    const QVariantList loadedFiles = cache.loadFiles(1, 10);
    ok = ok && expect(loadedFiles.size() == 1, "loadFiles should return one file");
    if (!loadedFiles.isEmpty()) {
        const QVariantMap loadedFile = loadedFiles.first().toMap();
        ok = ok && expect(loadedFile.value("statusCode").toInt() == 2,
                          "cached file statusCode mismatch");
        ok = ok && expect(loadedFile.value("thumbnailUrl").toString().isEmpty(),
                          "cached QML thumbnail URL should remain local-or-empty");
        ok = ok && expect(loadedFile.value("thumbnailSourceUrl").toString()
                              == "https://example.invalid/preview.jpg",
                          "cached thumbnail source URL mismatch");
        ok = ok && expect(loadedFile.value("sizeText").toString().isEmpty(),
                          "missing optional file text should be cached as an empty string");
        ok = ok && expect(loadedFile.value("machine").toString().isEmpty(),
                          "missing file machine should be cached as an empty string");
        ok = ok && expect(loadedFile.value("material").toString().isEmpty(),
                          "missing file material should be cached as an empty string");
    }


    QVariantList duplicateFiles;
    duplicateFiles.append(cloudFile);
    duplicateFiles.append(cloudFile);
    ok = ok && expect(!cache.replaceFiles(duplicateFiles),
                      "duplicate file ids should fail transactionally");
    const QVariantList filesAfterRollback = cache.loadFiles(1, 10);
    ok = ok && expect(filesAfterRollback.size() == 1,
                      "failed replaceFiles should preserve previous snapshot");
    if (!filesAfterRollback.isEmpty()) {
        ok = ok && expect(filesAfterRollback.first().toMap().value("fileId").toString()
                              == "file-processing",
                          "rollback should preserve the previous file row");
    }

    QVariantMap defaultedFile;
    defaultedFile.insert("fileId", "file-defaults");
    defaultedFile.insert("fileName", "defaults.pwsz");
    QVariantList defaultedFiles;
    defaultedFiles.append(defaultedFile);
    ok = ok && expect(cache.replaceFiles(defaultedFiles),
                      "replaceFiles should accept missing optional text fields");
    const QVariantList loadedDefaultedFiles = cache.loadFiles(1, 10);
    ok = ok && expect(loadedDefaultedFiles.size() == 1,
                      "defaulted file cache should contain one row");
    if (!loadedDefaultedFiles.isEmpty()) {
        const QVariantMap loaded = loadedDefaultedFiles.first().toMap();
        ok = ok && expect(loaded.value("status").toString() == "UNKNOWN",
                          "missing file status should default to UNKNOWN");
        ok = ok && expect(loaded.value("sizeText").toString().isEmpty(),
                          "missing file sizeText should default to an empty string");
        ok = ok && expect(loaded.value("gcodeId").toString().isEmpty(),
                          "missing file gcodeId should default to an empty string");
    }

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

    QVariantMap printerDetails;
    printerDetails.insert("firmwareVersion", "FW-1");
    printerDetails.insert("printCount", "42");
    printerDetails.insert("printTotalTime", "12h 30m");
    printerDetails.insert("materialUsed", "250 ml");
    printerDetails.insert("releaseFilmStatus", "ok");
    printerDetails.insert("releaseFilmLayers", "120");
    printerDetails.insert("releaseFilmTimes", 7);
    printerDetails.insert("releaseFilmStatusCode", 0);
    ok = ok && expect(cache.savePrinterDetails("printer-1", printerDetails),
                      "savePrinterDetails should succeed");

    const QVariantList loadedPrinters = cache.loadPrinters();
    ok = ok && expect(loadedPrinters.size() == 1, "loadPrinters should return one printer");
    if (!loadedPrinters.isEmpty()) {
        const QVariantMap p = loadedPrinters.first().toMap();
        ok = ok && expect(p.value("id").toString() == "printer-1", "printer id mismatch");
        ok = ok && expect(p.value("printerKey").toString() == "printer-key-1", "printerKey mismatch");
        ok = ok && expect(p.value("machineType").toString() == "128", "machineType mismatch");
        ok = ok && expect(p.value("state").toString() == "READY", "printer state mismatch");
        const QVariantMap details = p.value("details").toMap();
        ok = ok && expect(details.value("firmwareVersion").toString() == "FW-1",
                          "cached printer details firmware mismatch");
        ok = ok && expect(details.value("printCount").toString() == "42",
                          "cached printer details print count mismatch");
        ok = ok && expect(details.value("releaseFilmStatusCode").toInt() == 0,
                          "cached printer details release film status mismatch");
    }

    QVariantMap partialDetails;
    partialDetails.insert("mqttResinBlocking", false);
    partialDetails.insert("printTotalTime", "-");
    printer.insert("details", partialDetails);
    printer.insert("state", "OFFLINE");
    QVariantList offlinePrinters;
    offlinePrinters.append(printer);
    ok = ok && expect(cache.replacePrinters(offlinePrinters),
                      "replacePrinters should merge partial details with cached static details");
    const QVariantList offlineLoadedPrinters = cache.loadPrinters();
    if (!offlineLoadedPrinters.isEmpty()) {
        const QVariantMap p = offlineLoadedPrinters.first().toMap();
        const QVariantMap details = p.value("details").toMap();
        ok = ok && expect(p.value("state").toString() == "OFFLINE",
                          "offline printer state should be cached");
        ok = ok && expect(details.value("firmwareVersion").toString() == "FW-1",
                          "offline cached printer should keep firmware details");
        ok = ok && expect(details.value("printTotalTime").toString() == "12h 30m",
                          "partial refresh should not erase cached print total time");
    }

    QVariantMap minimalPrinter;
    minimalPrinter.insert("id", "printer-minimal");
    QVariantList minimalPrinters;
    minimalPrinters.append(minimalPrinter);
    ok = ok && expect(cache.replacePrinters(minimalPrinters),
                      "replacePrinters should accept missing optional text fields");
    const QVariantList loadedMinimalPrinters = cache.loadPrinters();
    ok = ok && expect(loadedMinimalPrinters.size() == 1,
                      "minimal printer cache should contain one row");
    if (!loadedMinimalPrinters.isEmpty()) {
        const QVariantMap p = loadedMinimalPrinters.first().toMap();
        ok = ok && expect(p.value("state").toString() == "UNKNOWN",
                          "missing printer state should default to UNKNOWN");
        ok = ok && expect(p.value("printerKey").toString().isEmpty(),
                          "missing printer key should default to an empty string");
        ok = ok && expect(p.value("currentFile").toString().isEmpty(),
                          "missing printer current file should default to an empty string");
    }

    QVariantMap minimalJob;
    minimalJob.insert("taskId", "task-minimal");
    QVariantList minimalJobs;
    minimalJobs.append(minimalJob);
    ok = ok && expect(cache.replaceJobsForPrinter("printer-1", minimalJobs),
                      "replaceJobsForPrinter should accept missing optional text fields");
    const QVariantList loadedMinimalJobs = cache.loadJobsForPrinter("printer-1", 1, 10);
    ok = ok && expect(loadedMinimalJobs.size() == 1,
                      "minimal job cache should contain one row");
    if (!loadedMinimalJobs.isEmpty()) {
        const QVariantMap job = loadedMinimalJobs.first().toMap();
        ok = ok && expect(job.value("printerName").toString().isEmpty(),
                          "missing job printer name should default to an empty string");
        ok = ok && expect(job.value("gcodeName").toString().isEmpty(),
                          "missing job gcode name should default to an empty string");
        ok = ok && expect(job.value("reason").toString().isEmpty(),
                          "missing job reason should default to an empty string");
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
    updatedJob.insert("cloudFileId", "cloud-active");
    updatedJob.insert("gcodeId", "gcode-active");
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
            ok = ok && expect(job.value("cloudFileId").toString() == "cloud-active",
                              "updated job cloudFileId mismatch");
            ok = ok && expect(job.value("gcodeId").toString() == "gcode-active",
                              "updated job gcodeId mismatch");
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
    removeSqliteFiles(dbPath);
    return ok;
}

bool test_local_cache_store_migrates_schema_v3_to_v5() {
    const auto previousDbPath = envValue("ACCLOUD_DB_PATH");
    const std::filesystem::path dbPath = uniqueTempDbPath("accloud_cache_schema_v3");
    bool ok = expect(createSchemaV3Database(dbPath),
                     "schema v3 cache seed should succeed");
    setenv("ACCLOUD_DB_PATH", dbPath.string().c_str(), 1);

    accloud::LocalCacheStore cache;
    ok = ok && expect(cache.isAvailable(), "schema v3 cache should migrate");

    QVariantMap migratedFile;
    migratedFile.insert("fileId", "migrated-processing");
    migratedFile.insert("fileName", "migrated.pwsz");
    migratedFile.insert("status", "PROCESSING");
    migratedFile.insert("statusCode", 2);
    migratedFile.insert("thumbnailUrl", "");
    migratedFile.insert("thumbnailSourceUrl", "https://example.invalid/migrated.jpg");
    QVariantList migratedFiles;
    migratedFiles.append(migratedFile);
    ok = ok && expect(cache.replaceFiles(migratedFiles),
                      "replaceFiles should succeed after schema v3 migration");

    const QVariantList loadedFiles = cache.loadFiles(1, 10);
    ok = ok && expect(loadedFiles.size() == 1,
                      "migrated cache should contain one replacement file");
    if (!loadedFiles.isEmpty()) {
        const QVariantMap loaded = loadedFiles.first().toMap();
        ok = ok && expect(loaded.value("statusCode").toInt() == 2,
                          "migrated statusCode mismatch");
        ok = ok && expect(loaded.value("thumbnailSourceUrl").toString()
                              == "https://example.invalid/migrated.jpg",
                          "migrated thumbnail source mismatch");
    }
    ok = ok && expect(verifySchemaV5Database(dbPath),
                      "schema version and v5 columns should be persisted");

    restoreEnv("ACCLOUD_DB_PATH", previousDbPath);
    removeSqliteFiles(dbPath);
    return ok;
}


bool test_pending_direct_print_persistence() {
    const auto previousDbPath = envValue("ACCLOUD_DB_PATH");
    const std::filesystem::path dbPath = uniqueTempDbPath("accloud_direct_print_pending");
    setenv("ACCLOUD_DB_PATH", dbPath.string().c_str(), 1);

    accloud::LocalCacheStore cache;
    bool ok = expect(cache.isAvailable(), "direct print cache should initialize");

    QVariantMap operation;
    operation.insert("printerId", "printer-1");
    operation.insert("cloudFileId", "cloud-direct-1");
    operation.insert("cloudGcodeId", "gcode-direct-1");
    operation.insert("cloudFileName", "cube.pwsz");
    operation.insert("cloudFileSize", 117557);
    operation.insert("printTaskId", "task-direct-1");
    operation.insert("printMsgId", "print-msg");
    operation.insert("printerLocalFilename", "cube.pwsz");
    operation.insert("printerLocalPath", "/");
    operation.insert("deleteAfterSuccess", true);
    operation.insert("deleteLocalOnFailure", false);
    operation.insert("observedActive", true);
    operation.insert("state", "PRINTING");
    operation.insert("createdAt", 1785448826);

    ok = ok && expect(cache.savePendingDirectPrint(operation),
                      "pending direct print should persist");
    const QVariantList rows = cache.loadPendingDirectPrints();
    ok = ok && expect(rows.size() == 1, "one pending direct print should load");
    if (!rows.isEmpty()) {
        const QVariantMap loaded = rows.first().toMap();
        ok = ok && expect(loaded.value("cloudFileId").toString() == "cloud-direct-1",
                          "cloud file id should round-trip");
        ok = ok && expect(loaded.value("printTaskId").toString() == "task-direct-1",
                          "print task id should round-trip");
        ok = ok && expect(loaded.value("printerLocalFilename").toString() == "cube.pwsz",
                          "printer-local filename should round-trip");
        ok = ok && expect(loaded.value("deleteAfterSuccess").toBool(),
                          "success cleanup flag should round-trip");
        ok = ok && expect(!loaded.value("deleteLocalOnFailure").toBool(),
                          "failure cleanup preference must default/persist off");
        ok = ok && expect(loaded.value("observedActive").toBool(),
                          "active observation should round-trip");
    }
    ok = ok && expect(cache.removePendingDirectPrint("printer-1"),
                      "pending direct print should be removable");
    ok = ok && expect(cache.loadPendingDirectPrints().isEmpty(),
                      "pending direct print should be removed");

    restoreEnv("ACCLOUD_DB_PATH", previousDbPath);
    removeSqliteFiles(dbPath);
    return ok;
}

bool test_print_workflow_bridge_centralizes_direct_print_persistence() {
    const auto previousDbPath = envValue("ACCLOUD_DB_PATH");
    const std::filesystem::path dbPath = uniqueTempDbPath("accloud_print_workflow_bridge");
    setenv("ACCLOUD_DB_PATH", dbPath.string().c_str(), 1);

    accloud::PrintWorkflowBridge bridge;
    QVariantMap operation;
    operation.insert("printerId", "printer-workflow-1");
    operation.insert("cloudFileId", "cloud-workflow-1");
    operation.insert("cloudFileName", "workflow.pwsz");
    operation.insert("printerLocalFilename", "workflow.pwsz");
    operation.insert("printerLocalPath", "/");
    operation.insert("deleteAfterSuccess", true);
    operation.insert("deleteLocalOnFailure", true);
    operation.insert("observedActive", true);
    operation.insert("state", "PRINTING");
    constexpr qlonglong kCreatedAt = 4294967419LL;
    constexpr qlonglong kUpdatedAt = 4294967420LL;
    operation.insert("createdAt", QVariant::fromValue<qlonglong>(kCreatedAt));
    operation.insert("updatedAt", QVariant::fromValue<qlonglong>(kUpdatedAt));

    const auto beforeTrack = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    bool ok = expect(bridge.trackDirectPrint(operation),
                     "workflow bridge should persist tracked direct print");
    const auto afterTrack = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    QVariantList rows = bridge.pendingDirectPrints();
    ok = ok && expect(rows.size() == 1,
                      "workflow bridge should restore one tracked direct print");
    if (!rows.isEmpty()) {
        const QVariantMap restored = rows.first().toMap();
        ok = ok && expect(restored.value("createdAt").toLongLong() == kCreatedAt,
                          "workflow bridge should preserve 64-bit createdAt");
        const qlonglong restoredUpdatedAt = restored.value("updatedAt").toLongLong();
        ok = ok && expect(restoredUpdatedAt >= static_cast<qlonglong>(beforeTrack)
                              && restoredUpdatedAt <= static_cast<qlonglong>(afterTrack),
                          "workflow bridge should refresh updatedAt to the persistence time");
        ok = ok && expect(restoredUpdatedAt != kUpdatedAt,
                          "workflow bridge should not preserve caller-supplied updatedAt");
    }

    QVariantMap begin = bridge.beginDirectLocalDelete(
        operation, accloud::PrintWorkflowBridge::FailureCompletion);
    QVariantMap prepared = begin.value("operation").toMap();
    ok = ok && expect(begin.value("persisted").toBool(),
                      "begin local cleanup should persist through bridge");
    ok = ok && expect(begin.value("requestLocalDelete").toBool(),
                      "begin local cleanup should request printer-local deletion");
    ok = ok && expect(prepared.value("state").toString() == "FAILURE_LOCAL_DELETE_PENDING",
                      "failure cleanup state should be typed and persisted");

    QVariantMap failedDispatch = bridge.handleDirectLocalDeleteDispatch(
        prepared, accloud::PrintWorkflowBridge::FailureCompletion, false, QString{});
    QVariantMap failedOperation = failedDispatch.value("operation").toMap();
    ok = ok && expect(failedDispatch.value("persisted").toBool(),
                      "failed local dispatch should persist cleanup error");
    ok = ok && expect(failedOperation.value("state").toString() == "FAILED_LOCAL_CLEANUP_ERROR",
                      "failed local dispatch should enter cleanup error state");

    QVariantMap cloudBegin = bridge.beginDirectCloudDelete(operation, false);
    QVariantMap cloudOperation = cloudBegin.value("operation").toMap();
    ok = ok && expect(cloudBegin.value("requestCloudDelete").toBool(),
                      "cloud cleanup should be requested by lifecycle bridge");
    QVariantMap cloudDone = bridge.handleDirectCloudDeleteResult(cloudOperation, true);
    ok = ok && expect(cloudDone.value("removed").toBool(),
                      "successful cloud cleanup should remove pending operation");
    ok = ok && expect(bridge.pendingDirectPrints().isEmpty(),
                      "workflow bridge should own pending direct print removal");

    int cloudDeleteRequests = 0;
    int lastCleanupNotice = accloud::PrintWorkflowBridge::NoCleanupNotice;
    QVariantMap requestedCloudDelete;
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::directCloudDeleteRequested,
                     [&cloudDeleteRequests, &requestedCloudDelete](const QVariantMap& requested) {
                         ++cloudDeleteRequests;
                         requestedCloudDelete = requested;
                     });
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::directCleanupNotice,
                     [&lastCleanupNotice](int noticeKind) {
                         lastCleanupNotice = noticeKind;
                     });

    QVariantMap mqttFailure = operation;
    mqttFailure.insert("printerId", "printer-workflow-mqtt-failure");
    mqttFailure.insert("cloudFileId", "cloud-workflow-mqtt-failure");
    ok = ok && expect(bridge.trackDirectPrint(mqttFailure),
                      "workflow bridge should persist MQTT-confirmed failure cleanup");
    QVariantMap mqttFailureBegin = bridge.beginDirectLocalDelete(
        mqttFailure, accloud::PrintWorkflowBridge::FailureCompletion);
    QVariantMap mqttFailurePrepared = mqttFailureBegin.value("operation").toMap();
    ok = ok && expect(mqttFailureBegin.value("requestLocalDelete").toBool(),
                      "first MQTT-confirmed cleanup should request local deletion");
    QVariantMap mqttFailureDispatch = bridge.handleDirectLocalDeleteDispatch(
        mqttFailurePrepared,
        accloud::PrintWorkflowBridge::FailureCompletion,
        true,
        QStringLiteral("mqtt-delete-failure"));
    ok = ok && expect(!mqttFailureDispatch.value("requestCloudDelete").toBool(),
                      "pending MQTT confirmation must not delete cloud early");
    QVariantMap duplicateBegin = bridge.beginDirectLocalDelete(
        mqttFailurePrepared, accloud::PrintWorkflowBridge::FailureCompletion);
    ok = ok && expect(!duplicateBegin.value("requestLocalDelete").toBool(),
                      "bridge should suppress duplicate local deletion while MQTT confirmation is pending");

    bridge.handlePrinterFileAction(QStringLiteral("printer-workflow-mqtt-failure"),
                                   QStringLiteral("deleteLocal"),
                                   QStringLiteral("success"),
                                   200,
                                   QStringLiteral("unrelated-msg"),
                                   QString{});
    ok = ok && expect(lastCleanupNotice == accloud::PrintWorkflowBridge::NoCleanupNotice,
                      "unrelated MQTT confirmation should be ignored");
    bridge.handlePrinterFileAction(QStringLiteral("printer-workflow-mqtt-failure"),
                                   QStringLiteral("deleteLocal"),
                                   QStringLiteral("success"),
                                   200,
                                   QStringLiteral("mqtt-delete-failure"),
                                   QString{});
    ok = ok && expect(lastCleanupNotice == accloud::PrintWorkflowBridge::FailureLocalDeleteCompleted,
                      "failure cleanup MQTT confirmation should emit semantic completion notice");
    ok = ok && expect(cloudDeleteRequests == 0,
                      "failed direct print must keep the cloud file after local cleanup");

    QVariantMap mqttSuccess = operation;
    mqttSuccess.insert("printerId", "printer-workflow-mqtt-success");
    mqttSuccess.insert("cloudFileId", "cloud-workflow-mqtt-success");
    ok = ok && expect(bridge.trackDirectPrint(mqttSuccess),
                      "workflow bridge should persist MQTT-confirmed success cleanup");
    QVariantMap mqttSuccessBegin = bridge.beginDirectLocalDelete(
        mqttSuccess, accloud::PrintWorkflowBridge::SuccessCompletion);
    QVariantMap mqttSuccessPrepared = mqttSuccessBegin.value("operation").toMap();
    (void)bridge.handleDirectLocalDeleteDispatch(
        mqttSuccessPrepared,
        accloud::PrintWorkflowBridge::SuccessCompletion,
        true,
        QStringLiteral("mqtt-delete-success"));
    bridge.handlePrinterFileAction(QStringLiteral("printer-workflow-mqtt-success"),
                                   QStringLiteral("deleteLocal"),
                                   QStringLiteral("success"),
                                   200,
                                   QStringLiteral("mqtt-delete-success"),
                                   QString{});
    ok = ok && expect(cloudDeleteRequests == 1,
                      "successful direct print cleanup should request cloud deletion after MQTT confirmation");
    ok = ok && expect(requestedCloudDelete.value("printerId").toString() == "printer-workflow-mqtt-success",
                      "cloud deletion request should carry the correlated direct-print operation");
    ok = ok && expect(requestedCloudDelete.value("state").toString() == "CLOUD_DELETE_PENDING",
                      "cloud deletion request should expose the typed pending state");

    QVariantMap mqttRejected = operation;
    mqttRejected.insert("printerId", "printer-workflow-mqtt-rejected");
    mqttRejected.insert("cloudFileId", "cloud-workflow-mqtt-rejected");
    ok = ok && expect(bridge.trackDirectPrint(mqttRejected),
                      "workflow bridge should persist rejected MQTT cleanup fixture");
    QVariantMap mqttRejectedBegin = bridge.beginDirectLocalDelete(
        mqttRejected, accloud::PrintWorkflowBridge::FailureCompletion);
    QVariantMap mqttRejectedPrepared = mqttRejectedBegin.value("operation").toMap();
    (void)bridge.handleDirectLocalDeleteDispatch(
        mqttRejectedPrepared,
        accloud::PrintWorkflowBridge::FailureCompletion,
        true,
        QStringLiteral("mqtt-delete-rejected"));
    bridge.handlePrinterFileAction(QStringLiteral("printer-workflow-mqtt-rejected"),
                                   QStringLiteral("deleteLocal"),
                                   QStringLiteral("failed"),
                                   500,
                                   QStringLiteral("mqtt-delete-rejected"),
                                   QStringLiteral("rejected"));
    ok = ok && expect(lastCleanupNotice == accloud::PrintWorkflowBridge::LocalDeleteConfirmationFailed,
                      "failed MQTT confirmation should emit semantic cleanup failure notice");
    ok = ok && expect(cloudDeleteRequests == 1,
                      "failed MQTT confirmation must never request cloud deletion");

    ok = ok && expect(bridge.completeDirectPrint(QStringLiteral("printer-workflow-mqtt-success")),
                      "success MQTT fixture should be removable");
    ok = ok && expect(bridge.completeDirectPrint(QStringLiteral("printer-workflow-mqtt-rejected")),
                      "rejected MQTT fixture should be removable");

    int localDeleteRequests = 0;
    int trackingReleased = 0;
    QVariantMap lastLocalDeleteRequest;
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::directLocalDeleteRequested,
                     [&localDeleteRequests, &lastLocalDeleteRequest](const QVariantMap& request) {
                         ++localDeleteRequests;
                         lastLocalDeleteRequest = request;
                     });
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::directPrintTrackingReleased,
                     [&trackingReleased](const QString&) { ++trackingReleased; });

    QVariantMap orchestrated = operation;
    orchestrated.insert("printerId", "printer-workflow-orchestrated-success");
    orchestrated.insert("cloudFileId", "cloud-workflow-orchestrated-success");
    orchestrated.insert("printerLocalFilename", "");
    orchestrated.insert("observedActive", true);
    orchestrated.insert("deleteAfterSuccess", true);
    orchestrated.insert("state", "PRINTING");
    ok = ok && expect(bridge.trackDirectPrint(orchestrated),
                      "orchestrated direct print should persist before project reconciliation");

    QVariantMap completedProject;
    completedProject.insert("printerId", "printer-workflow-orchestrated-success");
    completedProject.insert("cloudFileId", "cloud-workflow-orchestrated-success");
    completedProject.insert("currentFile", "orchestrated.pwsz");
    completedProject.insert("printStatus", 2);
    QVariantList completedProjects;
    completedProjects.push_back(completedProject);
    bridge.reconcileDirectPrints(completedProjects);
    ok = ok && expect(localDeleteRequests == 1,
                      "workflow reconciliation should request printer-local cleanup without QML orchestration");
    ok = ok && expect(lastLocalDeleteRequest.value("fileName").toString() == "orchestrated.pwsz",
                      "workflow reconciliation should carry the printer-local filename");
    ok = ok && expect(lastLocalDeleteRequest.value("completionKind").toInt()
                          == accloud::PrintWorkflowBridge::SuccessCompletion,
                      "successful print reconciliation should carry success cleanup semantics");
    bridge.reconcileDirectPrints(completedProjects);
    ok = ok && expect(localDeleteRequests == 1,
                      "workflow reconciliation must suppress duplicate local cleanup while request is in flight");

    QVariantMap directCleanupContext;
    directCleanupContext.insert("kind", "directCleanup");
    directCleanupContext.insert("completionKind", accloud::PrintWorkflowBridge::SuccessCompletion);
    bridge.handlePrinterOrderFinished(directCleanupContext,
                                      QStringLiteral("printer-workflow-orchestrated-success"),
                                      104,
                                      QVariantMap{{QStringLiteral("ok"), true},
                                                  {QStringLiteral("msgId"), QStringLiteral("orchestrated-delete-msg")}});
    bridge.handlePrinterFileAction(QStringLiteral("printer-workflow-orchestrated-success"),
                                   QStringLiteral("deleteLocal"),
                                   QStringLiteral("success"),
                                   200,
                                   QStringLiteral("orchestrated-delete-msg"),
                                   QString{});
    ok = ok && expect(cloudDeleteRequests == 2,
                      "workflow should request cloud deletion after local cleanup confirmation");
    bridge.handleCloudDeleteFinished(QStringLiteral("cloud-workflow-orchestrated-success"),
                                     QVariantMap{{QStringLiteral("ok"), true}});
    ok = ok && expect(lastCleanupNotice == accloud::PrintWorkflowBridge::DirectCleanupCompleted,
                      "workflow should emit semantic direct cleanup completion after cloud deletion");
    ok = ok && expect(trackingReleased >= 1,
                      "workflow should release UI tracking when direct cleanup completes");

    QVariantMap failedNoCleanup = operation;
    failedNoCleanup.insert("printerId", "printer-workflow-orchestrated-failure");
    failedNoCleanup.insert("cloudFileId", "cloud-workflow-orchestrated-failure");
    failedNoCleanup.insert("printerLocalFilename", "failed.pwsz");
    failedNoCleanup.insert("observedActive", true);
    failedNoCleanup.insert("deleteAfterSuccess", true);
    failedNoCleanup.insert("deleteLocalOnFailure", false);
    failedNoCleanup.insert("state", "PRINTING");
    ok = ok && expect(bridge.trackDirectPrint(failedNoCleanup),
                      "failed direct print fixture should persist");
    QVariantMap failedProject;
    failedProject.insert("printerId", "printer-workflow-orchestrated-failure");
    failedProject.insert("cloudFileId", "cloud-workflow-orchestrated-failure");
    failedProject.insert("currentFile", "failed.pwsz");
    failedProject.insert("printStatus", 3);
    const int localRequestsBeforeFailure = localDeleteRequests;
    const int cloudRequestsBeforeFailure = cloudDeleteRequests;
    QVariantList failedProjects;
    failedProjects.push_back(failedProject);
    bridge.reconcileDirectPrints(failedProjects);
    ok = ok && expect(localDeleteRequests == localRequestsBeforeFailure,
                      "failed direct print with cleanup disabled must not delete printer-local file");
    ok = ok && expect(cloudDeleteRequests == cloudRequestsBeforeFailure,
                      "failed direct print must never delete cloud file");
    ok = ok && expect(lastCleanupNotice == accloud::PrintWorkflowBridge::FailureFilesKept,
                      "workflow should emit semantic files-kept notice for failed direct print");

    restoreEnv("ACCLOUD_DB_PATH", previousDbPath);
    removeSqliteFiles(dbPath);
    return ok;
}

bool test_print_workflow_bridge_remote_cleanup_and_preparation() {
    const auto previousDbPath = envValue("ACCLOUD_DB_PATH");
    const std::filesystem::path dbPath = uniqueTempDbPath("accloud_remote_print_workflow");
    setenv("ACCLOUD_DB_PATH", dbPath.string().c_str(), 1);

    accloud::PrintWorkflowBridge bridge;
    bool ok = true;

    int localDeleteRequests = 0;
    int cloudDeleteRequests = 0;
    int trackingReleased = 0;
    int cleanupNotice = accloud::PrintWorkflowBridge::NoRemoteCleanupNotice;
    QVariantMap lastLocalDelete;
    QString lastCloudDelete;
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::remoteLocalDeleteRequested,
                     [&](const QVariantMap& request) {
                         ++localDeleteRequests;
                         lastLocalDelete = request;
                     });
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::remoteCloudDeleteRequested,
                     [&](const QString& fileId) {
                         ++cloudDeleteRequests;
                         lastCloudDelete = fileId;
                     });
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::remotePrintTrackingReleased,
                     [&](const QString&) { ++trackingReleased; });
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::remoteCleanupNotice,
                     [&](int notice, const QString&) { cleanupNotice = notice; });

    QVariantMap remoteFile;
    remoteFile.insert("fileId", "cloud-remote-1");
    remoteFile.insert("fileName", "remote.pwmb");
    remoteFile.insert("deleteAfterPrint", true);
    ok = ok && expect(bridge.trackRemotePrintCleanup("printer-remote-1", remoteFile),
                      "remote cleanup should be tracked by workflow bridge");
    bridge.beginRemotePostPrintCleanup("printer-remote-1");
    bridge.beginRemotePostPrintCleanup("printer-remote-1");
    ok = ok && expect(localDeleteRequests == 1,
                      "remote cleanup must suppress duplicate local delete dispatch");
    ok = ok && expect(lastLocalDelete.value("fileName").toString() == "remote.pwmb",
                      "remote local delete request should carry the printer-local filename");

    QVariantMap cleanupContext;
    cleanupContext.insert("kind", "remotePostPrintCleanup");
    bridge.handlePrinterOrderFinished(cleanupContext,
                                      QStringLiteral("printer-remote-1"),
                                      104,
                                      {{QStringLiteral("ok"), true}});
    ok = ok && expect(cloudDeleteRequests == 1 && lastCloudDelete == "cloud-remote-1",
                      "remote cleanup should request cloud delete only after local delete succeeds");
    bridge.handleCloudDeleteFinished(QStringLiteral("cloud-remote-1"),
                                     {{QStringLiteral("ok"), true}});
    ok = ok && expect(cleanupNotice == accloud::PrintWorkflowBridge::RemoteCleanupCompleted,
                      "remote cleanup should emit semantic completion after cloud delete");
    ok = ok && expect(trackingReleased == 1,
                      "remote cleanup should release UI tracking after completion");

    QVariantMap keepFile = remoteFile;
    keepFile.insert("fileId", "cloud-remote-keep");
    keepFile.insert("deleteAfterPrint", false);
    ok = ok && expect(bridge.trackRemotePrintCleanup("printer-remote-keep", keepFile),
                      "remote print without cleanup should still be tracked until completion");
    bridge.beginRemotePostPrintCleanup("printer-remote-keep");
    ok = ok && expect(localDeleteRequests == 1,
                      "deleteAfterPrint=false must not issue local deletion");
    ok = ok && expect(trackingReleased == 2,
                      "deleteAfterPrint=false should release tracking immediately at completion");

    int byFileRequests = 0;
    int byExtRequests = 0;
    QString activeRequestId;
    QString activeExt;
    QVariantMap preparation;
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::remoteCompatibilityByFileIdRequested,
                     [&](const QString& requestId, const QString& fileId) {
                         ++byFileRequests;
                         activeRequestId = requestId;
                         ok = expect(fileId == "cloud-prep-1", "file compatibility request should carry file id") && ok;
                     });
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::remoteCompatibilityByExtRequested,
                     [&](const QString& requestId, const QString& fileExt) {
                         ++byExtRequests;
                         activeRequestId = requestId;
                         activeExt = fileExt;
                     });
    QObject::connect(&bridge, &accloud::PrintWorkflowBridge::remotePrintPreparationReady,
                     [&](const QVariantMap& result) { preparation = result; });

    QVariantMap p1{{"id", "p1"}, {"name", "Printer One"}, {"model", "Mono M7"},
                   {"machineType", "m7"}, {"state", "READY"}};
    QVariantMap p2{{"id", "p2"}, {"name", "Printer Two"}, {"model", "Mono M5s"},
                   {"machineType", "m5s"}, {"state", "READY"}};
    QVariantList printers{p1, p2};
    QVariantMap prepFile{{"fileId", "cloud-prep-1"}, {"fileName", "route_file.pwmb"}};

    const QString requestId = bridge.beginRemotePrintPreparation(
        QStringLiteral("cloud"), QStringLiteral("cloud-prep-1"), QStringLiteral("route_file.pwmb"),
        prepFile, printers, QStringLiteral("p1"));
    ok = ok && expect(byFileRequests == 1 && !requestId.isEmpty(),
                      "remote preparation should own compatibility request sequencing");
    bridge.handleCompatiblePrintersByFileIdReady(
        QStringLiteral("stale-request"), QStringLiteral("cloud-prep-1"),
        {{QStringLiteral("ok"), true}});
    ok = ok && expect(preparation.isEmpty(), "stale compatibility response must be ignored");

    QVariantList serverRows;
    serverRows.push_back(QVariantMap{{"id", "p1"}, {"available", 0}, {"reason", "unavailable reason:file type mismatch"}});
    serverRows.push_back(QVariantMap{{"id", "p2"}, {"available", 1}, {"reason", ""}});
    bridge.handleCompatiblePrintersByFileIdReady(
        requestId, QStringLiteral("cloud-prep-1"),
        {{QStringLiteral("ok"), true}, {QStringLiteral("printers"), serverRows}});
    ok = ok && expect(preparation.value("selectedPrinterId").toString() == "p2",
                      "remote preparation should select an available compatible printer");
    ok = ok && expect(preparation.value("compatiblePrinters").toList().size() == 1,
                      "remote preparation should return a filtered printer model");

    preparation.clear();
    const QString fallbackRequestId = bridge.beginRemotePrintPreparation(
        QStringLiteral("cloud"), QStringLiteral("cloud-prep-1"), QStringLiteral("route_file.pwmb"),
        prepFile, printers, QStringLiteral("p1"));
    bridge.handleCompatiblePrintersByFileIdReady(
        fallbackRequestId, QStringLiteral("cloud-prep-1"), {{QStringLiteral("ok"), false}});
    ok = ok && expect(byExtRequests == 1 && activeExt == "pwmb",
                      "file compatibility failure should fall back to extension compatibility");

    QVariantMap localFile{{"fileId", "local-prep"}, {"fileName", "local.pwmb"},
                          {"machineType", "m5s"}};
    preparation.clear();
    const QString localRequestId = bridge.beginRemotePrintPreparation(
        QStringLiteral("direct"), QString{}, QStringLiteral("local.pwmb"),
        localFile, printers, QStringLiteral("p1"));
    ok = ok && expect(byExtRequests == 2 && activeExt == "pwmb",
                      "direct preparation should request extension compatibility through the workflow");
    bridge.handleCompatiblePrintersByExtReady(
        localRequestId, QStringLiteral("pwmb"), {{QStringLiteral("ok"), false}});
    ok = ok && expect(preparation.value("selectedPrinterId").toString() == "p2",
                      "local compatibility should be decided in C++ when server compatibility fails");
    ok = ok && expect(preparation.value("allowed").toBool(),
                      "locally compatible printer should pass remote print guard");
    const QVariantMap incompatibleGuard = bridge.evaluateRemotePrintGuard(
        QStringLiteral("cloud"), p1, localFile);
    ok = ok && expect(!incompatibleGuard.value("ok").toBool()
                          && incompatibleGuard.value("reasonKey").toString() == "machine_type_mismatch",
                      "remote print guard should use the shared C++ local compatibility evaluator");

    restoreEnv("ACCLOUD_DB_PATH", previousDbPath);
    removeSqliteFiles(dbPath);
    return ok;
}


bool test_cloud_files_workflow_bridge_sequences_delete_requests() {
    accloud::CloudFilesWorkflowBridge bridge;
    bool ok = true;
    QStringList requestedIds;
    QStringList successfulIds;
    QVariantMap finalSummary;
    QVariantMap singleResult;
    int startedTotal = 0;
    int progressCount = 0;

    QObject::connect(&bridge, &accloud::CloudFilesWorkflowBridge::deleteFileRequested,
                     [&](const QString& fileId) { requestedIds.push_back(fileId); });
    QObject::connect(&bridge, &accloud::CloudFilesWorkflowBridge::batchDeleteStarted,
                     [&](int total) { startedTotal = total; });
    QObject::connect(&bridge, &accloud::CloudFilesWorkflowBridge::batchDeleteProgress,
                     [&](int completed, int total, int succeeded, const QString&, bool) {
                         ++progressCount;
                         ok = expect(completed == progressCount,
                                     "batch workflow progress must advance one result at a time") && ok;
                         ok = expect(total == 2, "batch workflow progress must retain total") && ok;
                         ok = expect(succeeded <= completed,
                                     "batch workflow success count cannot exceed completed count") && ok;
                     });
    QObject::connect(&bridge, &accloud::CloudFilesWorkflowBridge::batchDeleteFileSucceeded,
                     [&](const QString& fileId) { successfulIds.push_back(fileId); });
    QObject::connect(&bridge, &accloud::CloudFilesWorkflowBridge::batchDeleteFinished,
                     [&](const QVariantMap& summary) { finalSummary = summary; });
    QObject::connect(&bridge, &accloud::CloudFilesWorkflowBridge::singleDeleteFinished,
                     [&](const QString&, const QVariantMap& result) { singleResult = result; });

    QVariantList files;
    files.push_back(QVariantMap{{"fileId", "batch-a"}, {"fileName", "a.pwsz"}});
    files.push_back(QVariantMap{{"fileId", "batch-b"}, {"fileName", "b.pwsz"}});
    ok = ok && expect(bridge.startBatchDelete(files), "batch bridge must accept a valid selection");
    ok = ok && expect(startedTotal == 2, "batch bridge must publish total at start");
    ok = ok && expect(requestedIds == QStringList{QStringLiteral("batch-a")},
                      "batch bridge must request only the first file initially");
    ok = ok && expect(!bridge.deleteSingleFile(QStringLiteral("single-blocked")),
                      "single delete must not interleave with an active batch");

    bridge.handleDeleteFileFinished(QStringLiteral("batch-b"),
                                    {{QStringLiteral("ok"), true}});
    ok = ok && expect(requestedIds.size() == 1,
                      "out-of-order delete completion must not advance the batch");

    bridge.handleDeleteFileFinished(QStringLiteral("batch-a"),
                                    {{QStringLiteral("ok"), true},
                                     {QStringLiteral("message"), QStringLiteral("ok")}});
    ok = ok && expect(requestedIds == QStringList{QStringLiteral("batch-a"),
                                                  QStringLiteral("batch-b")},
                      "second delete must be requested only after the first completion");
    ok = ok && expect(successfulIds == QStringList{QStringLiteral("batch-a")},
                      "successful file signal must identify the completed file");

    bridge.handleDeleteFileFinished(QStringLiteral("batch-b"),
                                    {{QStringLiteral("ok"), false},
                                     {QStringLiteral("message"), QStringLiteral("rejected")}});
    ok = ok && expect(progressCount == 2, "batch bridge must publish progress for every completion");
    ok = ok && expect(finalSummary.value("requested").toInt() == 2
                          && finalSummary.value("completed").toInt() == 2
                          && finalSummary.value("succeeded").toInt() == 1
                          && finalSummary.value("failed").toInt() == 1,
                      "batch bridge must publish aggregated summary counts");
    const QVariantList failures = finalSummary.value("failures").toList();
    ok = ok && expect(failures.size() == 1
                          && failures[0].toMap().value("fileId").toString() == "batch-b",
                      "batch bridge must publish failed file details");

    ok = ok && expect(bridge.deleteSingleFile(QStringLiteral("single-a")),
                      "single delete must start once the batch completes");
    ok = ok && expect(requestedIds.last() == "single-a",
                      "single delete must use the same semantic transport request");
    bridge.handleDeleteFileFinished(QStringLiteral("single-a"),
                                    {{QStringLiteral("ok"), true},
                                     {QStringLiteral("message"), QStringLiteral("ok")}});
    ok = ok && expect(singleResult.value("ok").toBool(),
                      "single delete result must be forwarded semantically to QML");
    return ok;
}

bool test_thumbnail_dir_defaults_to_local_share_accloud() {
    const std::filesystem::path thumbnailDir = accloud::config::thumbnailDir();
    const std::string dirText = thumbnailDir.generic_string();
    bool ok = expect(!dirText.empty(), "thumbnail dir should not be empty");
    ok = ok && expect(dirText.find("/.local/share/accloud/thumbnails") != std::string::npos
                          || dirText == ".local/share/accloud/thumbnails",
                      "thumbnail dir should default to ~/.local/share/accloud/thumbnails");
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok = test_response_envelope_parser_contract() && ok;
    ok = test_order_response_tracker_lifecycle() && ok;
    ok = test_upload_readiness_rejects_zero_sentinel() && ok;
    ok = test_local_cache_store_roundtrip_and_sync_state() && ok;
    ok = test_local_cache_store_migrates_schema_v3_to_v5() && ok;
    ok = test_pending_direct_print_persistence() && ok;
    ok = test_print_workflow_bridge_centralizes_direct_print_persistence() && ok;
    ok = test_print_workflow_bridge_remote_cleanup_and_preparation() && ok;
    ok = test_cloud_files_workflow_bridge_sequences_delete_requests() && ok;
    ok = test_thumbnail_dir_defaults_to_local_share_accloud() && ok;
    if (!ok) {
        return 1;
    }
    std::cout << "Cloud core regression tests passed\n";
    return 0;
}
