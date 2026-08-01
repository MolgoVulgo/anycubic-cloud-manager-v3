#include "CloudUploadController.h"

#include "CloudBridgeSupport.h"
#include "app/LocalCacheStore.h"
#include "ThumbnailService.h"
#include "app/usecases/cloud/UpdateCloudPwszPreviewsUseCase.h"
#include "app/usecases/cloud/UploadLocalFileUseCase.h"
#include "infra/cloud/archive/PwszPreviewArchive.h"
#include "infra/logging/JsonlLogger.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <utility>

namespace accloud {

CloudUploadController::CloudUploadController(LocalCacheStore* cache, QObject* parent)
    : QObject(parent)
    , m_cache(cache) {}

CloudUploadController::~CloudUploadController() {
    shutdown();
}

void CloudUploadController::setRefreshFilesCallback(std::function<void()> callback) {
    m_refreshFilesCallback = std::move(callback);
}

void CloudUploadController::reapFinishedBackgroundTasksLocked() {
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

void CloudUploadController::launchBackgroundTask(std::function<void()> task) {
    if (m_shuttingDown.load()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_backgroundTasksMutex);
    reapFinishedBackgroundTasksLocked();
    m_backgroundTasks.emplace_back(std::async(std::launch::async, [this, task = std::move(task)]() mutable {
        if (!m_shuttingDown.load()) {
            task();
        }
    }));
}

void CloudUploadController::waitBackgroundTasks() {
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

void CloudUploadController::shutdown() {
    if (m_shuttingDown.exchange(true)) {
        return;
    }
    m_pwszCloudUpdateCancelRequested.store(true);
    waitBackgroundTasks();
}

void CloudUploadController::invalidateFilesAndQuota() {
    if (m_cache == nullptr) {
        return;
    }
    m_cache->invalidateScope(QStringLiteral("files"));
    m_cache->invalidateScope(QStringLiteral("quota"));
}

QVariantMap CloudUploadController::inspectPwszPreview(const QString& localPath) const {
    QVariantMap out;
    const QString normalizedPath = cloud_bridge_support::normalizeUploadLocalPath(localPath);
    const QFileInfo info(normalizedPath);
    if (info.suffix().compare(QStringLiteral("pwsz"), Qt::CaseInsensitive) != 0) {
        out.insert("ok", true);
        out.insert("isPwsz", false);
        out.insert("needsCompletion", false);
        return out;
    }
    const auto inspection = cloud::archive::inspectPwszPreviewArchive(
        std::filesystem::path(normalizedPath.toStdString()));
    out.insert("ok", inspection.ok);
    out.insert("isPwsz", true);
    out.insert("hasPreview1", inspection.hasPreview1);
    out.insert("hasPreview2", inspection.hasPreview2);
    out.insert("needsCompletion", inspection.needsCompletion);
    out.insert("message", QString::fromStdString(inspection.message));
    return out;
}

QVariantMap CloudUploadController::uploadLocalFile(const QString& localPath,
                                                   bool completePwszPreview2) const {
    QVariantMap out;
    const QString normalizedPath = cloud_bridge_support::normalizeUploadLocalPath(localPath);
    const QFileInfo localFileInfo(normalizedPath);
    const QString localFileName = localFileInfo.fileName().trimmed().isEmpty()
                                      ? normalizedPath
                                      : localFileInfo.fileName();
    logging::info("app", "cloud_upload", "upload_local_file_start",
                  "uploadLocalFile called",
                  {{"file_name", localFileName.toStdString()}});
    if (normalizedPath.isEmpty()) {
        out.insert("ok", false);
        out.insert("message", QStringLiteral("Chemin fichier vide."));
        out.insert("messageKey", QStringLiteral("cloud.upload.path_required"));
        out.insert("fallbackMessage", QStringLiteral("Local file path is required."));
        logging::warn("app", "cloud_upload", "upload_local_file_invalid_path",
                      "uploadLocalFile aborted: empty path");
        cloud_bridge_support::finalizeUiMessage(out);
        return out;
    }

    const usecases::cloud::UploadLocalFileUseCase useCase;
    const auto result = useCase.execute(normalizedPath.toStdString(), completePwszPreview2);
    out.insert("ok", result.ok);
    out.insert("message", QString::fromStdString(result.message));
    out.insert("messageKey", result.ok
                             ? QStringLiteral("cloud.upload.ok")
                             : QStringLiteral("cloud.upload.failed"));
    out.insert("fallbackMessage", result.ok
                                  ? QStringLiteral("File uploaded to cloud.")
                                  : QStringLiteral("Failed to upload local file."));
    out.insert("params", QVariantMap{
        {QStringLiteral("fileName"), localFileName},
        {QStringLiteral("uploadStatus"), result.uploadStatus},
        {QStringLiteral("unlockOk"), result.unlockOk}
    });
    out.insert("fileId", QString::fromStdString(result.fileId));
    out.insert("gcodeId", QString::fromStdString(result.gcodeId));
    out.insert("uploadStatus", result.uploadStatus);
    out.insert("unlockOk", result.unlockOk);
    out.insert("previewAdded", result.previewAdded);
    out.insert("localFileSynchronized", result.localFileSynchronized);
    out.insert("recoveryPath", QString::fromStdString(result.recoveryPath));

    logging::info("app", "cloud_upload", "upload_local_file_result",
                  "uploadLocalFile finished",
                  {{"ok", result.ok ? "1" : "0"},
                   {"file_name", localFileName.toStdString()},
                   {"file_id", result.fileId},
                   {"gcode_id", result.gcodeId.empty() ? "0" : result.gcodeId},
                   {"status", std::to_string(result.uploadStatus)},
                   {"unlock_ok", result.unlockOk ? "1" : "0"}});

    if (result.ok && m_cache != nullptr) {
        m_cache->invalidateScope(QStringLiteral("files"));
        m_cache->invalidateScope(QStringLiteral("quota"));
    }

    cloud_bridge_support::finalizeUiMessage(out);
    return out;
}

void CloudUploadController::startUploadLocalFile(const QString& localPath,
                                                 bool completePwszPreview2,
                                                 const QString& requestContext) {
    const QString normalizedContext = requestContext.trimmed();
    const QString normalizedPath = cloud_bridge_support::normalizeUploadLocalPath(localPath);
    if (normalizedPath.isEmpty()) {
        if (normalizedContext.isEmpty()) {
            emit uploadFinished(false,
                                QStringLiteral("Chemin fichier vide."),
                                QString(), QString(), 0, false, true);
        } else {
            QVariantMap failure;
            failure.insert(QStringLiteral("ok"), false);
            failure.insert(QStringLiteral("message"), QStringLiteral("Chemin fichier vide."));
            emit uploadContextFinished(normalizedContext, failure);
        }
        return;
    }

    if (normalizedContext.isEmpty()) {
        emit uploadProgressChanged(0.0, QStringLiteral("Demarrage upload"));
    } else {
        emit uploadContextProgressChanged(normalizedContext, 0.0, QStringLiteral("Demarrage upload"));
    }
    logging::info("app", "cloud_upload", "upload_async_start",
                  "startUploadLocalFile called",
                  {{"file_path", normalizedPath.toStdString()}});

    launchBackgroundTask([this, normalizedPath, completePwszPreview2, normalizedContext]() {
        const usecases::cloud::UploadLocalFileUseCase useCase;
        const auto result = useCase.execute(
            normalizedPath.toStdString(),
            completePwszPreview2,
            [this, normalizedContext](double progress, const std::string& phase) {
                const double clamped = std::clamp(progress, 0.0, 1.0);
                const QString phaseText = QString::fromStdString(phase);
                QMetaObject::invokeMethod(this, [this, normalizedContext, clamped, phaseText]() {
                    if (normalizedContext.isEmpty()) {
                        emit uploadProgressChanged(clamped, phaseText);
                    } else {
                        emit uploadContextProgressChanged(normalizedContext, clamped, phaseText);
                    }
                }, Qt::QueuedConnection);
            });

        QMetaObject::invokeMethod(this, [this, normalizedContext, result]() {
            if (result.ok) {
                invalidateFilesAndQuota();
            }
            logging::info("app", "cloud_upload", "upload_async_result",
                          "startUploadLocalFile finished",
                          {{"ok", result.ok ? "1" : "0"},
                           {"file_id", result.fileId},
                           {"gcode_id", result.gcodeId.empty() ? "0" : result.gcodeId},
                           {"status", std::to_string(result.uploadStatus)},
                           {"unlock_ok", result.unlockOk ? "1" : "0"}});
            const QVariantMap contextResult{
                {QStringLiteral("ok"), result.ok},
                {QStringLiteral("message"), QString::fromStdString(result.message)},
                {QStringLiteral("fileId"), QString::fromStdString(result.fileId)},
                {QStringLiteral("gcodeId"), QString::fromStdString(result.gcodeId)},
                {QStringLiteral("uploadStatus"), result.uploadStatus},
                {QStringLiteral("unlockOk"), result.unlockOk},
                {QStringLiteral("localFileSynchronized"), result.localFileSynchronized}
            };
            if (normalizedContext.isEmpty()) {
                emit uploadFinished(result.ok,
                                    QString::fromStdString(result.message),
                                    QString::fromStdString(result.fileId),
                                    QString::fromStdString(result.gcodeId),
                                    result.uploadStatus,
                                    result.unlockOk,
                                    result.localFileSynchronized);
            } else {
                emit uploadContextFinished(normalizedContext, contextResult);
            }

            const bool ready = usecases::cloud::UploadLocalFileUseCase::isUploadReady(
                result.uploadStatus, result.gcodeId);
            if (result.ok && !ready && !m_shuttingDown.load() && m_refreshFilesCallback) {
                QTimer::singleShot(10000, this, [this]() {
                    if (!m_shuttingDown.load() && m_refreshFilesCallback) {
                        m_refreshFilesCallback();
                    }
                });
                QTimer::singleShot(30000, this, [this]() {
                    if (!m_shuttingDown.load() && m_refreshFilesCallback) {
                        m_refreshFilesCallback();
                    }
                });
            }
        }, Qt::QueuedConnection);
    });
}

void CloudUploadController::startPwszCloudPreviewUpdate(const QVariantList& files) {
    if (m_shuttingDown.load() || m_pwszCloudUpdateRunning.exchange(true)) {
        return;
    }
    m_pwszCloudUpdateCancelRequested.store(false);

    std::vector<usecases::cloud::CloudPwszPreviewUpdateItem> items;
    items.reserve(static_cast<std::size_t>(files.size()));
    for (const QVariant& value : files) {
        const QVariantMap map = value.toMap();
        usecases::cloud::CloudPwszPreviewUpdateItem item;
        item.fileId = map.value(QStringLiteral("fileId")).toString().trimmed().toStdString();
        item.fileName = map.value(QStringLiteral("fileName")).toString().trimmed().toStdString();
        item.sizeBytes = map.value(QStringLiteral("sizeBytes")).toULongLong();
        item.createTime = map.value(QStringLiteral("time"),
                                    map.value(QStringLiteral("createTimeEpoch"))).toLongLong();
        if (!item.fileId.empty() && !item.fileName.empty()) {
            items.push_back(std::move(item));
        }
    }

    if (items.empty()) {
        m_pwszCloudUpdateRunning.store(false);
        QVariantMap summary;
        summary.insert(QStringLiteral("ok"), false);
        summary.insert(QStringLiteral("cancelled"), false);
        summary.insert(QStringLiteral("cancelledItems"), 0);
        summary.insert(QStringLiteral("modified"), 0);
        summary.insert(QStringLiteral("skipped"), 0);
        summary.insert(QStringLiteral("failed"), 0);
        summary.insert(QStringLiteral("partial"), 0);
        summary.insert(QStringLiteral("message"), QStringLiteral("Aucun fichier PWSZ sélectionné."));
        emit pwszCloudPreviewUpdateFinished(summary);
        return;
    }

    launchBackgroundTask([this, items = std::move(items)]() {
        const usecases::cloud::UpdateCloudPwszPreviewsUseCase useCase;
        const auto result = useCase.execute(
            items,
            [this](int current, int total, const std::string& fileName, const std::string& phase) {
                if (m_shuttingDown.load()) {
                    return;
                }
                const QString qFileName = QString::fromStdString(fileName);
                const QString qPhase = QString::fromStdString(phase);
                QMetaObject::invokeMethod(this, [this, current, total, qFileName, qPhase]() {
                    if (!m_shuttingDown.load()) {
                        emit pwszCloudPreviewUpdateProgress(current, total, qFileName, qPhase);
                    }
                }, Qt::QueuedConnection);
            },
            [this]() {
                return m_shuttingDown.load() || m_pwszCloudUpdateCancelRequested.load();
            },
            [](const std::string& url,
               const usecases::cloud::CancellationCallback& shouldCancel) {
                const auto resolved = thumbnail_service::resolveThumbnailLocalUrl(
                    QString::fromStdString(url), true, true, shouldCancel);
                usecases::cloud::CloudPwszThumbnailValidationResult validation;
                validation.valid = !resolved.localUrl.isEmpty();
                validation.tooSmall = resolved.tooSmall;
                validation.cancelled = resolved.cancelled;
                validation.message = resolved.failureCategory.toStdString();
                return validation;
            });

        QVariantList itemResults;
        itemResults.reserve(static_cast<qsizetype>(result.items.size()));
        for (const auto& item : result.items) {
            QVariantMap map;
            map.insert(QStringLiteral("originalFileId"), QString::fromStdString(item.originalFileId));
            map.insert(QStringLiteral("newFileId"), QString::fromStdString(item.newFileId));
            map.insert(QStringLiteral("fileName"), QString::fromStdString(item.fileName));
            map.insert(QStringLiteral("status"), QString::fromStdString(item.status));
            map.insert(QStringLiteral("message"), QString::fromStdString(item.message));
            itemResults.append(map);
        }

        QVariantMap summary;
        summary.insert(QStringLiteral("ok"), result.ok);
        summary.insert(QStringLiteral("cancelled"), result.cancelled);
        summary.insert(QStringLiteral("cancelledItems"), result.cancelledItems);
        summary.insert(QStringLiteral("modified"), result.modified);
        summary.insert(QStringLiteral("skipped"), result.skipped);
        summary.insert(QStringLiteral("failed"), result.failed);
        summary.insert(QStringLiteral("partial"), result.partial);
        summary.insert(QStringLiteral("items"), itemResults);

        if (m_shuttingDown.load()) {
            m_pwszCloudUpdateRunning.store(false);
            return;
        }
        QMetaObject::invokeMethod(this, [this, summary]() {
            m_pwszCloudUpdateRunning.store(false);
            m_pwszCloudUpdateCancelRequested.store(false);
            invalidateFilesAndQuota();
            emit pwszCloudPreviewUpdateFinished(summary);
            if (!m_shuttingDown.load() && m_refreshFilesCallback) {
                m_refreshFilesCallback();
            }
        }, Qt::QueuedConnection);
    });
}

void CloudUploadController::cancelPwszCloudPreviewUpdate() {
    if (m_pwszCloudUpdateRunning.load()) {
        m_pwszCloudUpdateCancelRequested.store(true);
    }
}

} // namespace accloud
