#include "CloudBridge.h"

#include "app/cloud/CloudBridgeSupport.h"
#include "app/cloud/CloudDownloadController.h"
#include "app/cloud/CloudUploadController.h"
#include "app/cloud/ThumbnailService.h"

#include "app/printing/PrinterFileCompatibility.h"

#include "LocalCacheStore.h"
#include "UiPerfTrace.h"
#include "app/realtime/PrinterRealtimeStore.h"
#include "app/usecases/cloud/DeleteCloudFileUseCase.h"
#include "app/usecases/cloud/FetchPrinterCompatibilityByExtUseCase.h"
#include "app/usecases/cloud/FetchPrinterCompatibilityByFileIdUseCase.h"
#include "app/usecases/cloud/FetchPrinterDetailsUseCase.h"
#include "app/usecases/cloud/FetchPrinterProjectsUseCase.h"
#include "app/usecases/cloud/FetchReasonCatalogUseCase.h"
#include "app/usecases/cloud/GetDownloadUrlUseCase.h"
#include "app/usecases/cloud/LoadCloudFilesUseCase.h"
#include "app/usecases/cloud/LoadCloudQuotaUseCase.h"
#include "app/usecases/cloud/LoadPrintersDashboardUseCase.h"
#include "app/usecases/cloud/SendPrinterOrderUseCase.h"
#include "app/usecases/cloud/SendPrintOrderUseCase.h"
#include "infra/cloud/HarImporter.h"
#include "infra/debug/DebugBuild.h"
#include "infra/logging/JsonlLogger.h"

#include <QDateTime>
#include <QHash>
#include <QStringList>
#include <QMetaObject>
#include <QUrl>
#include <QVariantList>

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

namespace accloud {
namespace {

constexpr bool kDebugBuildEnabled = debug::kEnabled;

} // namespace

using cloud_bridge_support::applyRealtimeOverlayToPrinterMap;
using cloud_bridge_support::compactJsonFromVariantMap;
using cloud_bridge_support::fileInfoToMap;
using cloud_bridge_support::finalizeUiMessage;
using cloud_bridge_support::printerCompatToMap;
using cloud_bridge_support::printerDetailsToMap;
using cloud_bridge_support::printerInfoToMap;
using cloud_bridge_support::printerProjectToMap;
using cloud_bridge_support::reasonCatalogItemToMap;
using thumbnail_service::localImageIsVisuallyUsable;
using thumbnail_service::resolveProjectImageFromFilesCache;
using thumbnail_service::resolveThumbnailInMap;


// ── Constructeur / destructeur ────────────────────────────────────────────

CloudBridge::CloudBridge(QObject* parent)
    : QObject(parent)
    , m_cache(new LocalCacheStore())
    , m_downloadController(new CloudDownloadController(this))
    , m_uploadController(new CloudUploadController(m_cache, this)) {
    connect(m_downloadController, &CloudDownloadController::progress,
            this, &CloudBridge::downloadProgress);
    connect(m_downloadController, &CloudDownloadController::finished,
            this, &CloudBridge::downloadFinished);
    connect(m_uploadController, &CloudUploadController::uploadProgressChanged,
            this, &CloudBridge::uploadProgressChanged);
    connect(m_uploadController, &CloudUploadController::uploadFinished,
            this, &CloudBridge::uploadFinished);
    connect(m_uploadController, &CloudUploadController::uploadContextProgressChanged,
            this, &CloudBridge::uploadContextProgressChanged);
    connect(m_uploadController, &CloudUploadController::uploadContextFinished,
            this, &CloudBridge::uploadContextFinished);
    connect(m_uploadController, &CloudUploadController::pwszCloudPreviewUpdateProgress,
            this, &CloudBridge::pwszCloudPreviewUpdateProgress);
    connect(m_uploadController, &CloudUploadController::pwszCloudPreviewUpdateFinished,
            this, &CloudBridge::pwszCloudPreviewUpdateFinished);
    m_uploadController->setRefreshFilesCallback([this]() {
        if (!m_shuttingDown.load()) {
            refreshFilesAsync(1, 20, true);
        }
    });
}

CloudBridge::~CloudBridge() {
    m_shuttingDown.store(true);
    if (m_uploadController) {
        m_uploadController->shutdown();
    }
    waitBackgroundTasks();
    if (m_downloadController) {
        m_downloadController->shutdown();
    }
    delete m_cache;
    m_cache = nullptr;
}

void CloudBridge::reapFinishedBackgroundTasksLocked() {
    auto it = m_backgroundTasks.begin();
    while (it != m_backgroundTasks.end()) {
        if (it->valid() && it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                it->get();
            } catch (...) {
                // Best effort cleanup of async tasks.
            }
            it = m_backgroundTasks.erase(it);
        } else {
            ++it;
        }
    }
}

void CloudBridge::launchBackgroundTask(std::function<void()> task) {
    if (m_shuttingDown.load()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_backgroundTasksMutex);
    reapFinishedBackgroundTasksLocked();
    m_backgroundTasks.emplace_back(std::async(std::launch::async, [this, task = std::move(task)]() mutable {
        if (m_shuttingDown.load()) {
            return;
        }
        task();
    }));
}

void CloudBridge::waitBackgroundTasks() {
    std::vector<std::future<void>> tasks;
    {
        std::lock_guard<std::mutex> lock(m_backgroundTasksMutex);
        tasks.swap(m_backgroundTasks);
    }
    for (auto& task : tasks) {
        if (!task.valid()) {
            continue;
        }
        try {
            task.wait();
            task.get();
        } catch (...) {
            // Ignore task exceptions during shutdown.
        }
    }
}

bool CloudBridge::shouldRefresh(const QString& scope, int ttlSec, bool force) const {
    if (force || ttlSec <= 0 || m_cache == nullptr) {
        return true;
    }

    const auto state = m_cache->syncState(scope);
    if (!state.has_value() || !state->hasSuccess || state->lastSuccessAt <= 0) {
        return true;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return (now - state->lastSuccessAt) >= ttlSec;
}

namespace {

QHash<QString, QString> cachedThumbnailUrlsByFileId(const LocalCacheStore* cache) {
    QHash<QString, QString> thumbnails;
    if (cache == nullptr || !cache->isAvailable()) {
        return thumbnails;
    }

    const QVariantList cachedFiles = cache->loadFiles(1, 1500);
    thumbnails.reserve(cachedFiles.size());
    for (const QVariant& cachedFile : cachedFiles) {
        const QVariantMap map = cachedFile.toMap();
        const QString fileId = map.value(QStringLiteral("fileId")).toString().trimmed();
        const QString thumbnailUrl =
            map.value(QStringLiteral("thumbnailUrl")).toString().trimmed();
        if (!fileId.isEmpty() && QUrl(thumbnailUrl).isLocalFile()) {
            thumbnails.insert(fileId, thumbnailUrl);
        }
    }
    return thumbnails;
}

void reuseCachedThumbnailCandidate(QVariantMap& item,
                                   const QHash<QString, QString>& cachedThumbnails,
                                   bool forceThumbnails) {
    if (forceThumbnails) {
        return;
    }

    const QString fileId = item.value(QStringLiteral("fileId")).toString().trimmed();
    const QString cachedUrl = cachedThumbnails.value(fileId).trimmed();
    if (cachedUrl.isEmpty()) {
        return;
    }

    QStringList candidates = item.value(QStringLiteral("thumbnailCandidates")).toStringList();
    candidates.removeAll(cachedUrl);
    candidates.prepend(cachedUrl);
    item.insert(QStringLiteral("thumbnailCandidates"), candidates);
}

} // namespace

QVariantList CloudBridge::fetchFilesWithRetry(int page, int limit, QString& message, bool& ok, bool downloadThumbnails, bool forceThumbnails) const {
    ok = false;
    QVariantList files;
    const usecases::cloud::LoadCloudFilesUseCase useCase;
    const usecases::cloud::LoadCloudFilesResult result = useCase.execute(page, limit);
    message = QString::fromStdString(result.message);
    ok = result.ok;
    if (!result.ok) {
        return files;
    }

    const QHash<QString, QString> cachedThumbnails =
        forceThumbnails ? QHash<QString, QString>{} : cachedThumbnailUrlsByFileId(m_cache);
    files.reserve(static_cast<qsizetype>(result.files.size()));
    for (const auto& f : result.files) {
        QVariantMap item = fileInfoToMap(f);
        reuseCachedThumbnailCandidate(item, cachedThumbnails, forceThumbnails);
        resolveThumbnailInMap(item, downloadThumbnails, forceThumbnails);
        files.append(item);
    }
    return files;
}

QVariantList CloudBridge::fetchAllFilesWithRetry(int pageSize,
                                                   QString& message,
                                                   bool& ok,
                                                   bool downloadThumbnails,
                                                   bool forceThumbnails) const {
    ok = false;
    QVariantList files;
    const usecases::cloud::LoadCloudFilesUseCase useCase;
    const usecases::cloud::LoadAllCloudFilesResult result = useCase.executeAll(pageSize, 100);
    message = QString::fromStdString(result.message);
    ok = result.ok && result.complete;
    if (!ok) {
        return files;
    }

    const QHash<QString, QString> cachedThumbnails =
        forceThumbnails ? QHash<QString, QString>{} : cachedThumbnailUrlsByFileId(m_cache);
    files.reserve(static_cast<qsizetype>(result.files.size()));
    for (const auto& file : result.files) {
        QVariantMap item = fileInfoToMap(file);
        reuseCachedThumbnailCandidate(item, cachedThumbnails, forceThumbnails);
        resolveThumbnailInMap(item, downloadThumbnails, forceThumbnails);
        files.append(item);
    }
    return files;
}

QVariantList CloudBridge::fetchPrintersWithRetry(QString& message, bool& ok, QString& rawJson) const {
    ok = false;
    rawJson.clear();
    QVariantList printers;
    const usecases::cloud::LoadPrintersDashboardUseCase useCase;
    const usecases::cloud::LoadPrintersDashboardResult result = useCase.execute();
    message = QString::fromStdString(result.message);
    if constexpr (kDebugBuildEnabled) {
        rawJson = QString::fromStdString(result.rawJson);
    } else {
        rawJson.clear();
    }
    ok = result.ok;
    if (!result.ok) {
        return printers;
    }

    printers.reserve(static_cast<qsizetype>(result.printers.size()));
    for (const auto& p : result.printers) {
        printers.append(printerInfoToMap(p));
    }
    return printers;
}

QVariantMap CloudBridge::fetchQuotaWithRetry(QString& message, bool& ok) const {
    ok = false;
    QVariantMap out;
    const usecases::cloud::LoadCloudQuotaUseCase useCase;
    const cloud::CloudQuotaResult q = useCase.execute();

    message = QString::fromStdString(q.message);
    ok = q.ok;
    if (!q.ok) {
        finalizeUiMessage(out);
        return out;
    }

    out.insert("totalDisplay", QString::fromStdString(q.totalDisplay));
    out.insert("usedDisplay", QString::fromStdString(q.usedDisplay));
    out.insert("totalBytes", static_cast<qulonglong>(q.totalBytes));
    out.insert("usedBytes", static_cast<qulonglong>(q.usedBytes));
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::loadCachedFiles(int page, int limit) const {
    UiPerfTrace perf("cloud_bridge.load_cached_files");
    perf.setField("page", std::to_string(page));
    perf.setField("limit", std::to_string(limit));
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    out.insert("messageKey", QStringLiteral("cache.files.unavailable"));
    out.insert("fallbackMessage", QStringLiteral("Local file cache is unavailable."));
    out.insert("files", QVariantList{});
    out.insert("total", 0);

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    // Keep startup path fast while ensuring QML only receives validated local images.
    QVariantList files = m_cache->loadFiles(page, limit);
    for (QVariant& file : files) {
        QVariantMap map = file.toMap();
        resolveThumbnailInMap(map, false);
        file = map;
    }
    perf.setCount("items", files.size());
    out.insert("ok", true);
    out.insert("message", files.isEmpty()
                             ? QStringLiteral("Aucune donnée cache.")
                             : QStringLiteral("Fichiers chargés depuis le cache local."));
    out.insert("messageKey", files.isEmpty()
                             ? QStringLiteral("cache.files.empty")
                             : QStringLiteral("cache.files.loaded"));
    out.insert("fallbackMessage", files.isEmpty()
                                  ? QStringLiteral("No file available in local cache.")
                                  : QStringLiteral("Files loaded from local cache."));
    out.insert("params", QVariantMap{
        {QStringLiteral("count"), files.size()},
        {QStringLiteral("page"), page},
        {QStringLiteral("limit"), limit}
    });
    out.insert("files", files);
    out.insert("total", files.size());
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::loadCachedFilesAsync(int page, int limit) {
    if (m_shuttingDown.load()) {
        return;
    }
    launchBackgroundTask([this, page, limit]() {
        const QVariantMap result = loadCachedFiles(page, limit);
        QMetaObject::invokeMethod(this, [this, result]() {
            emit cachedFilesLoaded(result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::loadCachedPrinters() const {
    UiPerfTrace perf("cloud_bridge.load_cached_printers");
    QVariantMap out;
    if constexpr (kDebugBuildEnabled) {
        out.insert("endpoint", QStringLiteral(
            "/p/p/workbench/api/work/printer/getPrinters + "
            "/p/p/workbench/api/work/project/getProjects?printer_id=<id>&print_status=1"));
    }
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    out.insert("messageKey", QStringLiteral("cache.printers.unavailable"));
    out.insert("fallbackMessage", QStringLiteral("Local printer cache is unavailable."));
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString{});
    }
    out.insert("printers", QVariantList{});

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    const QVariantList cachedPrinters = m_cache->loadPrinters();
    perf.setCount("cached_printers", cachedPrinters.size());
    QStringList printerIds;
    printerIds.reserve(cachedPrinters.size());
    for (const QVariant& item : cachedPrinters) {
        const QString printerId = item.toMap().value(QStringLiteral("id")).toString().trimmed();
        if (!printerId.isEmpty() && !printerIds.contains(printerId)) {
            printerIds.push_back(printerId);
        }
    }
    const QVariantMap cachedProjectsByPrinter = m_cache->loadRecentJobsForPrinters(printerIds, 20);
    perf.setCount("cached_jobs_printers", cachedProjectsByPrinter.size());
    const auto realtimeSnapshots = accloud::realtime::PrinterRealtimeStore::instance().snapshotAll();
    QVariantList printers;
    printers.reserve(cachedPrinters.size());
    for (const QVariant& item : cachedPrinters) {
        QVariantMap printer = item.toMap();
        const QString printerId = printer.value(QStringLiteral("id")).toString();
        printer.insert(QStringLiteral("progress"), -1);
        printer.insert(QStringLiteral("elapsedSec"), -1);
        printer.insert(QStringLiteral("remainingSec"), -1);
        printer.insert(QStringLiteral("currentLayer"), -1);
        printer.insert(QStringLiteral("totalLayers"), -1);
        if (!printer.contains(QStringLiteral("details"))) {
            printer.insert(QStringLiteral("details"), QVariantMap{});
        }
        const QVariantList cachedProjects = cachedProjectsByPrinter.value(printerId).toList();
        if (!cachedProjects.isEmpty()) {
            const QVariantMap firstProject = cachedProjects.first().toMap();
            const QString firstName = firstProject.value(QStringLiteral("currentFile")).toString().trimmed().isEmpty()
                    ? firstProject.value(QStringLiteral("gcodeName")).toString()
                    : firstProject.value(QStringLiteral("currentFile")).toString();
            if (!firstName.trimmed().isEmpty()) {
                printer.insert(QStringLiteral("currentFile"), firstName);
            }
            const int firstProgress = firstProject.value(QStringLiteral("progress"), -1).toInt();
            if (firstProgress >= 0) {
                printer.insert(QStringLiteral("progress"), firstProgress);
            }
            const int firstElapsedSec = firstProject.value(QStringLiteral("elapsedSec"), -1).toInt();
            if (firstElapsedSec >= 0) {
                printer.insert(QStringLiteral("elapsedSec"), firstElapsedSec);
            }
            const int firstRemainingSec = firstProject.value(QStringLiteral("remainingSec"), -1).toInt();
            if (firstRemainingSec >= 0) {
                printer.insert(QStringLiteral("remainingSec"), firstRemainingSec);
            }
            const int firstCurrentLayer = firstProject.value(QStringLiteral("currentLayer"), -1).toInt();
            if (firstCurrentLayer >= 0) {
                printer.insert(QStringLiteral("currentLayer"), firstCurrentLayer);
            }
            const int firstTotalLayers = firstProject.value(QStringLiteral("totalLayers"), -1).toInt();
            if (firstTotalLayers >= 0) {
                printer.insert(QStringLiteral("totalLayers"), firstTotalLayers);
            }
        }
        if constexpr (kDebugBuildEnabled) {
            printer.insert(QStringLiteral("detailsRawJson"), QString{});
            printer.insert(QStringLiteral("projectsRawJson"), QString{});
        }
        printer.insert(QStringLiteral("projects"), cachedProjects);
        applyRealtimeOverlayToPrinterMap(printer, realtimeSnapshots);
        printers.append(printer);
    }
    perf.setCount("items", printers.size());
    out.insert("ok", true);
    out.insert("message", printers.isEmpty()
                             ? QStringLiteral("Aucune imprimante en cache.")
                             : QStringLiteral("Imprimantes chargées depuis le cache local."));
    out.insert("messageKey", printers.isEmpty()
                             ? QStringLiteral("cache.printers.empty")
                             : QStringLiteral("cache.printers.loaded"));
    out.insert("fallbackMessage", printers.isEmpty()
                                  ? QStringLiteral("No printer available in local cache.")
                                  : QStringLiteral("Printers loaded from local cache."));
    out.insert("params", QVariantMap{{QStringLiteral("count"), printers.size()}});
    out.insert("printers", printers);
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::loadCachedPrintersAsync() {
    if (m_shuttingDown.load()) {
        return;
    }
    launchBackgroundTask([this]() {
        const QVariantMap result = loadCachedPrinters();
        QMetaObject::invokeMethod(this, [this, result]() {
            emit cachedPrintersLoaded(result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::loadCachedQuota() const {
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache quota indisponible."));
    out.insert("messageKey", QStringLiteral("cache.quota.unavailable"));
    out.insert("fallbackMessage", QStringLiteral("Local quota cache is unavailable."));
    out.insert("totalDisplay", QString{});
    out.insert("usedDisplay", QString{});
    out.insert("totalBytes", static_cast<qulonglong>(0));
    out.insert("usedBytes", static_cast<qulonglong>(0));

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    const QVariantMap quota = m_cache->loadQuota();
    if (quota.isEmpty()) {
        out.insert("ok", true);
        out.insert("message", QStringLiteral("Aucun quota en cache."));
        out.insert("messageKey", QStringLiteral("cache.quota.empty"));
        out.insert("fallbackMessage", QStringLiteral("No cached quota available."));
        finalizeUiMessage(out);
        return out;
    }

    out.insert("ok", true);
    out.insert("message", QStringLiteral("Quota chargé depuis le cache local."));
    out.insert("messageKey", QStringLiteral("cache.quota.loaded"));
    out.insert("fallbackMessage", QStringLiteral("Quota loaded from local cache."));
    out.insert("totalDisplay", quota.value("totalDisplay"));
    out.insert("usedDisplay", quota.value("usedDisplay"));
    out.insert("totalBytes", quota.value("totalBytes"));
    out.insert("usedBytes", quota.value("usedBytes"));
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::loadCachedQuotaAsync() {
    if (m_shuttingDown.load()) {
        return;
    }
    launchBackgroundTask([this]() {
        const QVariantMap result = loadCachedQuota();
        QMetaObject::invokeMethod(this, [this, result]() {
            emit cachedQuotaLoaded(result);
        }, Qt::QueuedConnection);
    });
}

void CloudBridge::refreshFilesAsync(int page, int limit, bool force) {
    refreshFilesAsyncWithPolicy(page, limit, force, false);
}

void CloudBridge::refreshFilesAndThumbnailsAsync(int page, int limit, bool force) {
    refreshFilesAsyncWithPolicy(page, limit, force, true);
}

void CloudBridge::refreshFilesAsyncWithPolicy(int page,
                                              int limit,
                                              bool force,
                                              bool forceThumbnails) {
    if (m_shuttingDown.load()) {
        return;
    }
    if (m_refreshFilesRunning.exchange(true)) {
        return;
    }

    launchBackgroundTask([this, page, limit, force, forceThumbnails]() {
        if (m_shuttingDown.load()) {
            m_refreshFilesRunning.store(false);
            return;
        }

        if (!shouldRefresh(QStringLiteral("files"), 120, force)) {
            m_refreshFilesRunning.store(false);
            return;
        }

        const int scanPageSize = std::max(100, std::min(limit, 500));
        QString inventoryMessage;
        bool inventoryComplete = false;
        QVariantList files = fetchAllFilesWithRetry(
            scanPageSize, inventoryMessage, inventoryComplete, true, forceThumbnails);

        QString message = inventoryMessage;
        bool ok = inventoryComplete;
        if (!inventoryComplete) {
            QString fallbackMessage;
            bool fallbackOk = false;
            files = fetchFilesWithRetry(
                page, limit, fallbackMessage, fallbackOk, true, forceThumbnails);
            message = fallbackMessage;
            ok = fallbackOk;
        }

        if (m_cache != nullptr) {
            m_cache->updateSyncState(QStringLiteral("files"), ok, message);
        }

        if (ok) {
            if (m_cache != nullptr) {
                m_cache->replaceFiles(files);
            }

            QVariantList updateCandidates;
            qulonglong updateBytes = 0;
            if (inventoryComplete) {
                for (const QVariant& file : files) {
                    const QVariantMap map = file.toMap();
                    if (!map.value(QStringLiteral("thumbnailUpdateCandidate"), false).toBool()) {
                        continue;
                    }
                    QVariantMap candidate;
                    candidate.insert(QStringLiteral("fileId"), map.value(QStringLiteral("fileId")));
                    candidate.insert(QStringLiteral("fileName"), map.value(QStringLiteral("fileName")));
                    candidate.insert(QStringLiteral("sizeBytes"), map.value(QStringLiteral("sizeBytes")));
                    candidate.insert(QStringLiteral("time"), map.value(QStringLiteral("createTimeEpoch")));
                    updateCandidates.append(candidate);
                    updateBytes += map.value(QStringLiteral("sizeBytes")).toULongLong();
                }
            }

            QMetaObject::invokeMethod(this,
                                      [this, files, updateCandidates, updateBytes,
                                       inventoryComplete, inventoryMessage]() {
                emit filesUpdatedFromCloud(files, QStringLiteral("Cloud refresh terminé."));
                if (inventoryComplete && !updateCandidates.isEmpty()) {
                    emit pwszCloudPreviewUpdateSuggested(updateCandidates, updateBytes);
                }
                if (!inventoryComplete) {
                    emit syncFailed(QStringLiteral("pwsz_preview_candidates"),
                                    inventoryMessage);
                }
            },
                                      Qt::QueuedConnection);

            QString quotaMessage;
            bool quotaOk = false;
            QVariantMap quota = fetchQuotaWithRetry(quotaMessage, quotaOk);
            if (m_cache != nullptr) {
                m_cache->updateSyncState(QStringLiteral("quota"), quotaOk, quotaMessage);
            }
            if (quotaOk) {
                if (m_cache != nullptr) {
                    m_cache->saveQuota(quota);
                }
                QMetaObject::invokeMethod(this, [this, quota]() {
                    emit quotaUpdatedFromCloud(quota, QStringLiteral("Quota rafraîchi depuis le cloud."));
                }, Qt::QueuedConnection);
            } else {
                QMetaObject::invokeMethod(this, [this, quotaMessage]() {
                    emit syncFailed(QStringLiteral("quota"), quotaMessage);
                }, Qt::QueuedConnection);
            }
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("files"), message);
            }, Qt::QueuedConnection);
        }

        m_refreshFilesRunning.store(false);
    });
}

void CloudBridge::refreshPrintersAsync(bool force) {
    if (m_shuttingDown.load()) {
        return;
    }
    if (m_refreshPrintersRunning.exchange(true)) {
        return;
    }

    launchBackgroundTask([this, force]() {
        if (m_shuttingDown.load()) {
            m_refreshPrintersRunning.store(false);
            return;
        }
        QString message;
        QString rawJson;
        bool ok = false;

        if (!shouldRefresh(QStringLiteral("printers"), 30, force)) {
            m_refreshPrintersRunning.store(false);
            return;
        }

        const QVariantList printers = fetchPrintersWithRetry(message, ok, rawJson);
        if (m_cache != nullptr) {
            m_cache->updateSyncState(QStringLiteral("printers"), ok, message);
        }

        if (ok && m_cache != nullptr) {
            m_cache->replacePrinters(printers);
            QMetaObject::invokeMethod(this, [this, printers]() {
                emit printersUpdatedFromCloud(printers, QStringLiteral("Cloud refresh imprimantes terminé."));
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("printers"), message);
            }, Qt::QueuedConnection);
        }

        m_refreshPrintersRunning.store(false);
    });
}

void CloudBridge::refreshReasonCatalogAsync(bool force) {
    if (m_shuttingDown.load()) {
        return;
    }
    if (m_refreshReasonCatalogRunning.exchange(true)) {
        return;
    }

    launchBackgroundTask([this, force]() {
        if (m_shuttingDown.load()) {
            m_refreshReasonCatalogRunning.store(false);
            return;
        }
        const QString scope = QStringLiteral("reason_catalog");
        if (!shouldRefresh(scope, 3600, force)) {
            m_refreshReasonCatalogRunning.store(false);
            return;
        }

        const usecases::cloud::FetchReasonCatalogUseCase useCase;
        const auto r = useCase.execute();
        const bool ok = r.ok;
        const QString message = QString::fromStdString(r.message);

        if (m_cache != nullptr) {
            m_cache->updateSyncState(scope, ok, message);
        }

        if (ok) {
            QVariantList reasons;
            reasons.reserve(static_cast<qsizetype>(r.reasons.size()));
            for (const auto& item : r.reasons) {
                reasons.append(reasonCatalogItemToMap(item));
            }
            QMetaObject::invokeMethod(this, [this, reasons, message]() {
                emit reasonCatalogUpdatedFromCloud(reasons, message);
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("reason_catalog"), message);
            }, Qt::QueuedConnection);
        }

        m_refreshReasonCatalogRunning.store(false);
    });
}

void CloudBridge::refreshPrinterInsightsAsync(const QString& printerId, int page, int limit, bool force) {
    if (m_shuttingDown.load()) {
        return;
    }
    launchBackgroundTask([this, printerId, page, limit, force]() {
        if (m_shuttingDown.load()) {
            return;
        }
        const QString normalizedPrinterId = printerId.trimmed();
        if (normalizedPrinterId.isEmpty()) {
            return;
        }

        const QString scope = QStringLiteral("printer_insights_%1").arg(normalizedPrinterId);
        if (!shouldRefresh(scope, 15, force)) {
            return;
        }

        const usecases::cloud::FetchPrinterDetailsUseCase detailsUseCase;
        const auto detailsResult = detailsUseCase.execute(normalizedPrinterId.toStdString());
        const usecases::cloud::FetchPrinterProjectsUseCase projectsUseCase;
        const auto projectsResult = projectsUseCase.execute(normalizedPrinterId.toStdString(), page, limit);

        QVariantMap detailsMap;
        QString detailsRawJson;
        if constexpr (kDebugBuildEnabled) {
            detailsRawJson = QString::fromStdString(detailsResult.rawJson);
        }
        if (detailsResult.ok) {
            detailsMap = printerDetailsToMap(detailsResult);
            if (m_cache != nullptr) {
                m_cache->savePrinterDetails(normalizedPrinterId, detailsMap);
            }
        }

        QVariantList projects;
        QString projectsRawJson;
        if constexpr (kDebugBuildEnabled) {
            projectsRawJson = QString::fromStdString(projectsResult.rawJson);
        }
        if (projectsResult.ok) {
            projects.reserve(static_cast<qsizetype>(projectsResult.items.size()));
            for (const auto& item : projectsResult.items) {
                QVariantMap projectMap = printerProjectToMap(item);
                QString currentImage = projectMap.value(QStringLiteral("img")).toString();
                if (!localImageIsVisuallyUsable(currentImage)) {
                    const QString fallbackImage = resolveProjectImageFromFilesCache(
                        m_cache,
                        projectMap.value(QStringLiteral("currentFile")).toString(),
                        projectMap.value(QStringLiteral("gcodeName")).toString());
                    if (!fallbackImage.isEmpty()) {
                        projectMap.insert(QStringLiteral("img"), fallbackImage);
                    }
                }
                projects.append(projectMap);
            }
            if (m_cache != nullptr) {
                m_cache->upsertJobsForPrinter(normalizedPrinterId, projects);
                projects = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
            }
        } else if (m_cache != nullptr) {
            projects = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
            for (int i = 0; i < projects.size(); ++i) {
                QVariantMap projectMap = projects[i].toMap();
                const QString currentImage = projectMap.value(QStringLiteral("img")).toString();
                if (localImageIsVisuallyUsable(currentImage)) {
                    continue;
                }
                const QString fallbackImage = resolveProjectImageFromFilesCache(
                    m_cache,
                    projectMap.value(QStringLiteral("currentFile")).toString(),
                    projectMap.value(QStringLiteral("gcodeName")).toString());
                if (!fallbackImage.isEmpty()) {
                    projectMap.insert(QStringLiteral("img"), fallbackImage);
                    projects[i] = projectMap;
                }
            }
        }

        const bool ok = detailsResult.ok || projectsResult.ok || !projects.isEmpty();
        QString message;
        if (detailsResult.ok && projectsResult.ok) {
            message = QStringLiteral("Printer insights refreshed from cloud.");
        } else if (ok) {
            message = detailsResult.ok
                    ? QStringLiteral("Printer details refreshed; projects loaded from cache.")
                    : QStringLiteral("Printer projects refreshed; details unavailable.");
        } else {
            message = QString::fromStdString(!projectsResult.message.empty()
                    ? projectsResult.message
                    : detailsResult.message);
        }

        if (m_cache != nullptr) {
            m_cache->updateSyncState(scope, ok, message);
        }

        if (ok) {
            QMetaObject::invokeMethod(this, [this,
                                             normalizedPrinterId,
                                             detailsMap,
                                             projects,
                                             detailsRawJson,
                                             projectsRawJson,
                                             message]() {
                emit printerInsightsUpdatedFromCloud(normalizedPrinterId,
                                                     detailsMap,
                                                     projects,
                                                     detailsRawJson,
                                                     projectsRawJson,
                                                     message);
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("printer_insights"), message);
            }, Qt::QueuedConnection);
        }
    });
}

// ── fetchFiles ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchFiles(int page, int limit) const {
    QVariantMap out;
    QString message;
    bool ok = false;
    const QVariantList files = fetchFilesWithRetry(page, limit, message, ok, false);

    out.insert("ok", ok);
    out.insert("message", message);
    out.insert("total", files.size());
    out.insert("files", files);

    if (m_cache != nullptr) {
        m_cache->updateSyncState(QStringLiteral("files"), ok, message);
        if (ok) {
            m_cache->replaceFiles(files);
        }
    }
    finalizeUiMessage(out);
    return out;
}

// ── fetchQuota ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchQuota() const {
    QVariantMap out;
    QString message;
    bool ok = false;
    const QVariantMap quota = fetchQuotaWithRetry(message, ok);

    out.insert("ok", ok);
    out.insert("message", message);
    out.insert("totalDisplay", quota.value("totalDisplay"));
    out.insert("usedDisplay", quota.value("usedDisplay"));
    out.insert("totalBytes", quota.value("totalBytes", static_cast<qulonglong>(0)));
    out.insert("usedBytes", quota.value("usedBytes", static_cast<qulonglong>(0)));

    if (m_cache != nullptr) {
        m_cache->updateSyncState(QStringLiteral("quota"), ok, message);
        if (ok) {
            m_cache->saveQuota(quota);
        }
    }
    finalizeUiMessage(out);
    return out;
}

// ── deleteFile ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::deleteFile(const QString& fileId) const {
    QVariantMap out;
    logging::info("app", "cloud_bridge", "delete_file_start", "Suppression fichier",
                  {{"file_id", fileId.toStdString()}});
    const usecases::cloud::DeleteCloudFileUseCase useCase;
    const auto r = useCase.execute(fileId.toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.file.delete.ok")
                             : QStringLiteral("cloud.file.delete.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("Cloud file deleted.")
                                  : QStringLiteral("Failed to delete cloud file."));
    out.insert("params", QVariantMap{{QStringLiteral("fileId"), fileId}});
    if (r.ok && m_cache != nullptr) {
        m_cache->removeFile(fileId);
        m_cache->invalidateScope(QStringLiteral("files"));
    }
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::deleteFileAsync(const QString& fileId) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedFileId = fileId.trimmed();
    launchBackgroundTask([this, normalizedFileId]() {
        const QVariantMap result = deleteFile(normalizedFileId);
        QMetaObject::invokeMethod(this, [this, normalizedFileId, result]() {
            emit deleteFileFinished(normalizedFileId, result);
        }, Qt::QueuedConnection);
    });
}

// ── getDownloadUrl ────────────────────────────────────────────────────────

QVariantMap CloudBridge::getDownloadUrl(const QString& fileId) const {
    QVariantMap out;
    const usecases::cloud::GetDownloadUrlUseCase useCase;
    const auto r = useCase.execute(fileId.toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.file.download_url.ok")
                             : QStringLiteral("cloud.file.download_url.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("Download URL retrieved.")
                                  : QStringLiteral("Unable to retrieve download URL."));
    out.insert("params", QVariantMap{{QStringLiteral("fileId"), fileId}});
    if (r.ok)
        out.insert("url", QString::fromStdString(r.url));
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::getDownloadUrlAsync(const QString& fileId) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedFileId = fileId.trimmed();
    launchBackgroundTask([this, normalizedFileId]() {
        const QVariantMap result = getDownloadUrl(normalizedFileId);
        QMetaObject::invokeMethod(this, [this, normalizedFileId, result]() {
            emit downloadUrlReady(normalizedFileId, result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::inspectPwszPreview(const QString& localPath) const {
    return m_uploadController->inspectPwszPreview(localPath);
}

QVariantMap CloudBridge::uploadLocalFile(const QString& localPath, bool completePwszPreview2) const {
    return m_uploadController->uploadLocalFile(localPath, completePwszPreview2);
}

void CloudBridge::startUploadLocalFile(const QString& localPath,
                                           bool completePwszPreview2,
                                           const QString& requestContext) {
    m_uploadController->startUploadLocalFile(localPath, completePwszPreview2, requestContext);
}

void CloudBridge::startPwszCloudPreviewUpdate(const QVariantList& files) {
    m_uploadController->startPwszCloudPreviewUpdate(files);
}

void CloudBridge::cancelPwszCloudPreviewUpdate() {
    m_uploadController->cancelPwszCloudPreviewUpdate();
}

// ── fetchPrinters ─────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchPrinters() const {
    QVariantMap out;
    if constexpr (kDebugBuildEnabled) {
        out.insert("endpoint", QStringLiteral(
            "/p/p/workbench/api/work/printer/getPrinters + "
            "/p/p/workbench/api/work/project/getProjects?printer_id=<id>&print_status=1"));
    }
    QString message;
    QString rawJson;
    bool ok = false;
    const QVariantList printers = fetchPrintersWithRetry(message, ok, rawJson);
    out.insert("ok", ok);
    out.insert("message", message);
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", rawJson);
    }
    out.insert("printers", printers);

    if (m_cache != nullptr) {
        m_cache->updateSyncState(QStringLiteral("printers"), ok, message);
        if (ok) {
            m_cache->replacePrinters(printers);
        }
    }
    finalizeUiMessage(out);
    return out;
}

// ── fetchCompatiblePrintersByExt ─────────────────────────────────────────

QVariantMap CloudBridge::fetchCompatiblePrintersByExt(const QString& fileExt) const {
    QVariantMap out;
    const usecases::cloud::FetchPrinterCompatibilityByExtUseCase useCase;
    const auto r = useCase.execute(fileExt.trimmed().toLower().toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok) {
        QVariantList printers;
        printers.reserve(static_cast<qsizetype>(r.printers.size()));
        for (const auto& p : r.printers)
            printers.append(printerCompatToMap(p));
        out.insert("printers", printers);
    }
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::fetchCompatiblePrintersByExtAsync(const QString& fileExt,
                                                    const QString& requestId) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedExt = fileExt.trimmed().toLower();
    const QString normalizedRequestId = requestId.trimmed();
    launchBackgroundTask([this, normalizedExt, normalizedRequestId]() {
        const QVariantMap result = fetchCompatiblePrintersByExt(normalizedExt);
        QMetaObject::invokeMethod(this, [this, normalizedRequestId, normalizedExt, result]() {
            emit compatiblePrintersByExtReady(normalizedRequestId, normalizedExt, result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::fetchCompatiblePrintersByFileId(const QString& fileId) const {
    QVariantMap out;
    const usecases::cloud::FetchPrinterCompatibilityByFileIdUseCase useCase;
    const auto r = useCase.execute(fileId.trimmed().toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok) {
        QVariantList printers;
        printers.reserve(static_cast<qsizetype>(r.printers.size()));
        for (const auto& p : r.printers)
            printers.append(printerCompatToMap(p));
        out.insert("printers", printers);
    }
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::fetchCompatiblePrintersByFileIdAsync(const QString& fileId,
                                                       const QString& requestId) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedFileId = fileId.trimmed();
    const QString normalizedRequestId = requestId.trimmed();
    launchBackgroundTask([this, normalizedFileId, normalizedRequestId]() {
        const QVariantMap result = fetchCompatiblePrintersByFileId(normalizedFileId);
        QMetaObject::invokeMethod(this, [this, normalizedRequestId, normalizedFileId, result]() {
            emit compatiblePrintersByFileIdReady(normalizedRequestId, normalizedFileId, result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::evaluateLocalPrinterFileCompatibility(const QVariantMap& printer,
                                                               const QVariantMap& file) const {
    return printing::printerFileCompatibilityToVariantMap(
        printing::evaluatePrinterFileCompatibility(printer, file));
}

QVariantMap CloudBridge::fetchPrinterDetails(const QString& printerId) const {
    QVariantMap out;
    const usecases::cloud::FetchPrinterDetailsUseCase useCase;
    const auto r = useCase.execute(printerId.trimmed().toStdString());
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString::fromStdString(r.rawJson));
    }
    if (r.ok)
        out.insert("details", printerDetailsToMap(r));
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::fetchReasonCatalog() const {
    QVariantMap out;
    const usecases::cloud::FetchReasonCatalogUseCase useCase;
    const auto r = useCase.execute();
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok) {
        QVariantList reasons;
        reasons.reserve(static_cast<qsizetype>(r.reasons.size()));
        for (const auto& item : r.reasons)
            reasons.append(reasonCatalogItemToMap(item));
        out.insert("reasons", reasons);
    }
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::fetchPrinterProjects(const QString& printerId, int page, int limit) const {
    QVariantMap out;
    const QString normalizedPrinterId = printerId.trimmed();
    const usecases::cloud::FetchPrinterProjectsUseCase useCase;
    const auto r = useCase.execute(normalizedPrinterId.toStdString(), page, limit);
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString::fromStdString(r.rawJson));
    }
    if (r.ok) {
        QVariantList projects;
        projects.reserve(static_cast<qsizetype>(r.items.size()));
        for (const auto& item : r.items) {
            QVariantMap projectMap = printerProjectToMap(item);
            const QString currentImage = projectMap.value(QStringLiteral("img")).toString();
            if (!localImageIsVisuallyUsable(currentImage)) {
                const QString fallbackImage = resolveProjectImageFromFilesCache(
                    m_cache,
                    projectMap.value(QStringLiteral("currentFile")).toString(),
                    projectMap.value(QStringLiteral("gcodeName")).toString());
                if (!fallbackImage.isEmpty()) {
                    projectMap.insert(QStringLiteral("img"), fallbackImage);
                }
            }
            projects.append(projectMap);
        }
        out.insert("projects", projects);
        if (m_cache != nullptr && !normalizedPrinterId.isEmpty()) {
            m_cache->upsertJobsForPrinter(normalizedPrinterId, projects);
            out.insert("projects", m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit));
        }
    } else if (m_cache != nullptr && !normalizedPrinterId.isEmpty()) {
        const QVariantList cached = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
        if (!cached.isEmpty()) {
            QVariantList enriched = cached;
            for (int i = 0; i < enriched.size(); ++i) {
                QVariantMap projectMap = enriched[i].toMap();
                const QString currentImage = projectMap.value(QStringLiteral("img")).toString();
                if (localImageIsVisuallyUsable(currentImage)) {
                    continue;
                }
                const QString fallbackImage = resolveProjectImageFromFilesCache(
                    m_cache,
                    projectMap.value(QStringLiteral("currentFile")).toString(),
                    projectMap.value(QStringLiteral("gcodeName")).toString());
                if (!fallbackImage.isEmpty()) {
                    projectMap.insert(QStringLiteral("img"), fallbackImage);
                    enriched[i] = projectMap;
                }
            }
            out.insert("ok", true);
            out.insert("message", QStringLiteral("Jobs chargés depuis le cache local."));
            out.insert("projects", enriched);
        }
    }
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::loadCachedPrinterProjects(const QString& printerId, int page, int limit) const {
    UiPerfTrace perf("cloud_bridge.load_cached_printer_projects");
    perf.setField("page", std::to_string(page));
    perf.setField("limit", std::to_string(limit));
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    out.insert("messageKey", QStringLiteral("cache.printer_projects.unavailable"));
    out.insert("fallbackMessage", QStringLiteral("Local printer jobs cache is unavailable."));
    out.insert("projects", QVariantList{});
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString{});
    }

    const QString normalizedPrinterId = printerId.trimmed();
    perf.setField("printer_id_present", normalizedPrinterId.isEmpty() ? "0" : "1");
    if (normalizedPrinterId.isEmpty()) {
        out.insert("message", QStringLiteral("printer_id requis."));
        out.insert("messageKey", QStringLiteral("cloud.printer_id.required"));
        out.insert("fallbackMessage", QStringLiteral("Printer identifier is required."));
        finalizeUiMessage(out);
        return out;
    }

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    const QVariantList projects = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
    perf.setCount("items", projects.size());
    out.insert("ok", true);
    out.insert("message", projects.isEmpty()
                             ? QStringLiteral("Aucun job en cache pour cette imprimante.")
                             : QStringLiteral("Jobs chargés depuis le cache local."));
    out.insert("messageKey", projects.isEmpty()
                             ? QStringLiteral("cache.printer_projects.empty")
                             : QStringLiteral("cache.printer_projects.loaded"));
    out.insert("fallbackMessage", projects.isEmpty()
                                  ? QStringLiteral("No cached print job for this printer.")
                                  : QStringLiteral("Printer jobs loaded from local cache."));
    out.insert("params", QVariantMap{
        {QStringLiteral("printerId"), normalizedPrinterId},
        {QStringLiteral("count"), projects.size()},
        {QStringLiteral("page"), page},
        {QStringLiteral("limit"), limit}
    });
    out.insert("projects", projects);
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::loadCachedPrinterProjectsAsync(const QString& printerId, int page, int limit) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedPrinterId = printerId.trimmed();
    launchBackgroundTask([this, normalizedPrinterId, page, limit]() {
        const QVariantMap result = loadCachedPrinterProjects(normalizedPrinterId, page, limit);
        QMetaObject::invokeMethod(this, [this, normalizedPrinterId, result]() {
            emit cachedPrinterProjectsLoaded(normalizedPrinterId, result);
        }, Qt::QueuedConnection);
    });
}


// ── sendPrintOrder ────────────────────────────────────────────────────────

QVariantMap CloudBridge::sendPrintOrder(const QString& printerId,
                                        const QString& fileId,
                                        bool deleteAfterPrint,
                                        bool dryRun) const {
    QVariantMap out;
    if (dryRun) {
        out.insert("ok", true);
        out.insert("message", QString("Dry-run: print order payload generated."));
        out.insert("messageKey", QStringLiteral("cloud.print_order.dry_run"));
        out.insert("fallbackMessage", QStringLiteral("Print order payload generated (dry-run)."));
        out.insert("params", QVariantMap{
            {QStringLiteral("printerId"), printerId},
            {QStringLiteral("fileId"), fileId}
        });
        out.insert("taskId", QString());
        out.insert("msgId", QString());
        out.insert("correlationTicket", QString());
        out.insert("correlationStatus", QStringLiteral("Pending"));
        finalizeUiMessage(out);
        return out;
    }

    const usecases::cloud::SendPrintOrderUseCase useCase;
    const auto r = useCase.execute(
        printerId.toStdString(), fileId.toStdString(), deleteAfterPrint);
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.print_order.sent")
                             : QStringLiteral("cloud.print_order.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("Print order sent.")
                                  : QStringLiteral("Failed to send print order."));
    out.insert("params", QVariantMap{
        {QStringLiteral("printerId"), printerId},
        {QStringLiteral("fileId"), fileId}
    });
    out.insert("taskId",  QString::fromStdString(r.taskId));
    out.insert("msgId", QString::fromStdString(r.msgId));
    out.insert("correlationTicket", QString::fromStdString(r.correlationTicket));
    out.insert("correlationStatus", QString::fromStdString(r.correlationStatus));
    if (r.ok && m_cache != nullptr) {
        m_cache->invalidateScope(QStringLiteral("printers"));
    }
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::sendPrintOrderAsync(const QString& printerId,
                                      const QString& fileId,
                                      bool deleteAfterPrint,
                                      bool dryRun) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedPrinterId = printerId.trimmed();
    const QString normalizedFileId = fileId.trimmed();
    launchBackgroundTask([this, normalizedPrinterId, normalizedFileId, deleteAfterPrint, dryRun]() {
        const QVariantMap result = sendPrintOrder(normalizedPrinterId,
                                                  normalizedFileId,
                                                  deleteAfterPrint,
                                                  dryRun);
        QMetaObject::invokeMethod(this, [this, normalizedPrinterId, normalizedFileId, result]() {
            emit printOrderFinished(normalizedPrinterId, normalizedFileId, result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::sendPrinterOrder(const QString& printerId,
                                          int orderId,
                                          const QVariantMap& data,
                                          const QString& projectId) const {
    QVariantMap out;
    const QString normalizedPrinterId = printerId.trimmed();
    if (normalizedPrinterId.isEmpty()) {
        out.insert("ok", false);
        out.insert("message", QStringLiteral("printer_id requis."));
        out.insert("messageKey", QStringLiteral("cloud.printer_id.required"));
        out.insert("fallbackMessage", QStringLiteral("Printer identifier is required."));
        finalizeUiMessage(out);
        return out;
    }
    if (orderId <= 0) {
        out.insert("ok", false);
        out.insert("message", QStringLiteral("order_id invalide."));
        out.insert("messageKey", QStringLiteral("cloud.order_id.invalid"));
        out.insert("fallbackMessage", QStringLiteral("Order identifier is invalid."));
        out.insert("params", QVariantMap{{QStringLiteral("orderId"), orderId}});
        finalizeUiMessage(out);
        return out;
    }

    const usecases::cloud::SendPrinterOrderUseCase useCase;
    const std::string dataJson = compactJsonFromVariantMap(data);
    const auto r = useCase.execute(normalizedPrinterId.toStdString(),
                                   orderId,
                                   projectId.trimmed().toStdString(),
                                   dataJson);
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.printer_order.sent")
                             : QStringLiteral("cloud.printer_order.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("Printer order sent.")
                                  : QStringLiteral("Failed to send printer order."));
    out.insert("params", QVariantMap{
        {QStringLiteral("printerId"), normalizedPrinterId},
        {QStringLiteral("orderId"), orderId},
        {QStringLiteral("projectId"), projectId.trimmed()}
    });
    out.insert("taskId", QString::fromStdString(r.taskId));
    out.insert("msgId", QString::fromStdString(r.msgId));
    if (r.ok && m_cache != nullptr) {
        m_cache->invalidateScope(QStringLiteral("printers"));
    }
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::sendPrinterOrderAsync(const QString& printerId,
                                        int orderId,
                                        const QVariantMap& data,
                                        const QString& projectId,
                                        const QVariantMap& context) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedPrinterId = printerId.trimmed();
    const QString normalizedProjectId = projectId.trimmed();
    const QVariantMap structuredContext = context;
    launchBackgroundTask([this, normalizedPrinterId, orderId, data, normalizedProjectId, structuredContext]() {
        const QVariantMap result = sendPrinterOrder(normalizedPrinterId,
                                                    orderId,
                                                    data,
                                                    normalizedProjectId);
        QMetaObject::invokeMethod(this, [this, structuredContext, normalizedPrinterId, orderId, result]() {
            emit printerOrderFinished(structuredContext, normalizedPrinterId, orderId, result);
        }, Qt::QueuedConnection);
    });
}

// ── startDownload (async) ─────────────────────────────────────────────────

void CloudBridge::startDownload(const QString& signedUrl, const QString& savePath) {
    m_downloadController->start(signedUrl, savePath);
}

// ── cancelDownload ────────────────────────────────────────────────────────

void CloudBridge::cancelDownload() {
    m_downloadController->cancel();
}

} // namespace accloud
