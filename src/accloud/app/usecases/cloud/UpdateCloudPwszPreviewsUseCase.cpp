#include "UpdateCloudPwszPreviewsUseCase.h"

#include "infra/cloud/api/DownloadsApi.h"
#include "infra/cloud/api/FilesApi.h"
#include "infra/cloud/api/UploadsApi.h"
#include "infra/cloud/archive/PwszPreviewArchive.h"
#include "infra/cloud/core/SessionProvider.h"
#include "infra/cloud/thumbnail/ThumbnailValidation.h"
#include "infra/logging/JsonlLogger.h"

#ifdef ACCLOUD_WITH_QT
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#endif

#include <array>
#include <chrono>
#include <filesystem>
#include <thread>
#include <utility>

namespace accloud::usecases::cloud {
namespace {

#ifdef ACCLOUD_WITH_QT

struct DownloadResult {
    bool ok{false};
    bool cancelled{false};
    std::string message;
};

constexpr int kDownloadTimeoutMs = 300000;
constexpr int kThumbnailTimeoutMs = 15000;
constexpr int kCancellationPollMs = 100;
constexpr qint64 kDownloadChunkBytes = 64 * 1024;

DownloadResult downloadToFile(const std::string& signedUrl,
                              const std::filesystem::path& destination,
                              const CancellationCallback& shouldCancel) {
    if (signedUrl.empty() || destination.empty()) {
        return {false, false, "URL ou destination vide"};
    }
    if (detail::cancellationRequested(shouldCancel)) {
        return {false, true, "Opération annulée"};
    }

    QSaveFile output(QString::fromStdString(destination.string()));
    if (!output.open(QIODevice::WriteOnly)) {
        return {false, false, "Impossible de créer le fichier téléchargé"};
    }

    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(QString::fromStdString(signedUrl))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kDownloadTimeoutMs);

    QEventLoop loop;
    QTimer cancellationTimer;
    cancellationTimer.setInterval(kCancellationPollMs);

    bool cancelled = false;
    bool writeFailed = false;
    qint64 writtenBytes = 0;
    std::array<char, static_cast<std::size_t>(kDownloadChunkBytes)> buffer{};

    QNetworkReply* reply = nam.get(request);
    const auto drainReply = [&]() {
        while (!writeFailed && reply->bytesAvailable() > 0) {
            const qint64 readBytes = reply->read(buffer.data(), kDownloadChunkBytes);
            if (readBytes < 0) {
                writeFailed = true;
                reply->abort();
                break;
            }
            if (readBytes == 0) {
                break;
            }
            if (output.write(buffer.data(), readBytes) != readBytes) {
                writeFailed = true;
                reply->abort();
                break;
            }
            writtenBytes += readBytes;
        }
    };

    QObject::connect(reply, &QNetworkReply::readyRead, &loop, drainReply);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (shouldCancel) {
        QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
            if (!detail::cancellationRequested(shouldCancel)) {
                return;
            }
            cancelled = true;
            reply->abort();
        });
        cancellationTimer.start();
    }

    loop.exec();
    cancellationTimer.stop();
    drainReply();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool networkOk = reply->error() == QNetworkReply::NoError;
    const std::string error = reply->errorString().toStdString();
    reply->deleteLater();

    if (cancelled || detail::cancellationRequested(shouldCancel)) {
        output.cancelWriting();
        return {false, true, "Opération annulée"};
    }
    if (writeFailed) {
        output.cancelWriting();
        return {false, false, "Écriture du fichier téléchargé échouée"};
    }
    if (!networkOk || status < 200 || status >= 300 || writtenBytes <= 0) {
        output.cancelWriting();
        return {false, false, error.empty() ? "Téléchargement cloud échoué" : error};
    }
    if (!output.commit()) {
        output.cancelWriting();
        return {false, false, "Écriture du fichier téléchargé échouée"};
    }
    return {true, false, "Téléchargement terminé"};
}

CloudPwszThumbnailValidationResult probeThumbnail(const std::string& url,
                                    const CancellationCallback& shouldCancel) {
    if (url.empty()) {
        return {false, false, false, "URL de miniature vide"};
    }
    if (detail::cancellationRequested(shouldCancel)) {
        return {false, false, true, "Opération annulée"};
    }

    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(QString::fromStdString(url))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kThumbnailTimeoutMs);

    QEventLoop loop;
    QTimer timeout;
    QTimer cancellationTimer;
    timeout.setSingleShot(true);
    cancellationTimer.setInterval(kCancellationPollMs);
    bool timedOut = false;
    bool cancelled = false;
    QNetworkReply* reply = nam.get(request);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    if (shouldCancel) {
        QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
            if (!detail::cancellationRequested(shouldCancel)) {
                return;
            }
            cancelled = true;
            reply->abort();
            loop.quit();
        });
        cancellationTimer.start();
    }
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(kThumbnailTimeoutMs);
    loop.exec();
    timeout.stop();
    cancellationTimer.stop();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const bool networkOk = !timedOut && !cancelled
                         && reply->error() == QNetworkReply::NoError
                         && status >= 200 && status < 300;
    const std::string error = reply->errorString().toStdString();
    reply->deleteLater();
    if (cancelled || detail::cancellationRequested(shouldCancel)) {
        return {false, false, true, "Opération annulée"};
    }
    if (!networkOk) {
        return {false, false, false,
                error.empty() ? "Téléchargement miniature échoué" : error};
    }

    const auto validation = accloud::cloud::thumbnail::validateThumbnailBytes(body);
    return {validation.valid,
            validation.tooSmall,
            false,
            validation.valid ? "Miniature valide" : validation.error.toStdString()};
}

accloud::cloud::CloudUploadUnlockResult unlockStorageWithRetry(
    const accloud::cloud::api::UploadsApi& uploadsApi,
    const std::string& accessToken,
    const std::string& xxToken,
    const std::string& lockId,
    bool deleteCos) {
    static constexpr std::array<std::chrono::milliseconds, 3> retryDelays = {
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(250),
        std::chrono::milliseconds(750),
    };

    accloud::cloud::CloudUploadUnlockResult result;
    for (const auto delay : retryDelays) {
        if (delay > std::chrono::milliseconds::zero()) {
            std::this_thread::sleep_for(delay);
        }
        result = uploadsApi.unlockStorageSpace(accessToken, xxToken, lockId, deleteCos);
        if (result.ok) {
            return result;
        }
    }
    return result;
}

#endif

} // namespace

CloudPwszPreviewUpdateResult UpdateCloudPwszPreviewsUseCase::execute(
    const std::vector<CloudPwszPreviewUpdateItem>& items,
    const ProgressCallback& onProgress,
    const CancellationCallback& shouldCancel,
    const ThumbnailValidationCallback& validateThumbnail) const {
    CloudPwszPreviewUpdateResult summary;
    const auto orderedItems = orderCloudPwszPreviewUpdateItemsOldestFirst(items);
    summary.items.reserve(orderedItems.size());

#ifndef ACCLOUD_WITH_QT
    (void)onProgress;
    (void)shouldCancel;
    (void)validateThumbnail;
    for (const auto& item : orderedItems) {
        summary.items.push_back({item.fileId, {}, item.fileName, "failed", "Qt non disponible"});
        ++summary.failed;
    }
    return summary;
#else
    const auto appendCancelled = [&](CloudPwszPreviewUpdateItemResult result,
                                     std::string message) {
        result.status = "cancelled";
        result.message = std::move(message);
        summary.cancelled = true;
        ++summary.cancelledItems;
        summary.items.push_back(std::move(result));
    };
    const auto appendPartialCancellation = [&](CloudPwszPreviewUpdateItemResult result,
                                                std::string message) {
        result.status = "partial";
        result.message = std::move(message);
        summary.cancelled = true;
        ++summary.partial;
        summary.items.push_back(std::move(result));
    };

    if (detail::cancellationRequested(shouldCancel)) {
        summary.cancelled = true;
        return summary;
    }

    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        for (const auto& item : orderedItems) {
            summary.items.push_back({item.fileId, {}, item.fileName, "failed",
                                     "Session invalide ou introuvable"});
            ++summary.failed;
        }
        return summary;
    }

    const auto& context = contextResult.context;
    const accloud::cloud::api::DownloadsApi downloadsApi;
    const accloud::cloud::api::UploadsApi uploadsApi;
    const accloud::cloud::api::FilesApi filesApi;
    const int total = static_cast<int>(orderedItems.size());

    for (int index = 0; index < total; ++index) {
        if (detail::cancellationRequested(shouldCancel)) {
            summary.cancelled = true;
            break;
        }

        const auto& item = orderedItems[static_cast<std::size_t>(index)];
        const int current = index + 1;
        const auto report = [&](const std::string& phase) {
            if (onProgress && !detail::cancellationRequested(shouldCancel)) {
                onProgress(current, total, item.fileName, phase);
            }
        };
        const auto validateThumbnailUrl = [&](const std::string& url) {
            if (validateThumbnail) {
                return validateThumbnail(url, shouldCancel);
            }
            return probeThumbnail(url, shouldCancel);
        };

        CloudPwszPreviewUpdateItemResult result;
        result.originalFileId = item.fileId;
        result.fileName = item.fileName;
        result.status = "failed";

        report(std::string(phase::kDownload));
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            result.message = "Impossible de créer le répertoire temporaire";
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }

        const auto signedUrl = downloadsApi.getSignedUrl(context.accessToken,
                                                         context.xxToken,
                                                         item.fileId);
        if (detail::cancellationRequested(shouldCancel)) {
            appendCancelled(std::move(result), "Opération annulée avant le téléchargement");
            break;
        }
        if (!signedUrl.ok || signedUrl.url.empty()) {
            result.message = signedUrl.message.empty()
                                 ? "URL de téléchargement indisponible"
                                 : signedUrl.message;
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }

        std::filesystem::path originalPath =
            std::filesystem::path(tempDir.path().toStdString()) /
            std::filesystem::path(item.fileName).filename();
        const auto download = downloadToFile(signedUrl.url, originalPath, shouldCancel);
        if (!download.ok) {
            if (download.cancelled) {
                appendCancelled(std::move(result), download.message);
                break;
            }
            result.message = download.message;
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }

        if (detail::cancellationRequested(shouldCancel)) {
            appendCancelled(std::move(result), "Opération annulée après le téléchargement");
            break;
        }

        report(std::string(phase::kPrepare));
        const auto inspection = accloud::cloud::archive::inspectPwszPreviewArchive(originalPath);
        if (!inspection.ok) {
            result.message = inspection.message;
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }
        if (inspection.hasPreview2) {
            result.status = "skipped";
            result.message = "preview_images/preview_2.png est déjà présent";
            ++summary.skipped;
            summary.items.push_back(std::move(result));
            continue;
        }
        if (!inspection.hasPreview1) {
            result.message = "preview_images/preview_1.png est absent";
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }

        const auto prepared = accloud::cloud::archive::preparePwszPreview2Copy(originalPath);
        if (!prepared.ok || !prepared.changed) {
            result.message = prepared.message.empty()
                                 ? "Modification PWSZ impossible"
                                 : prepared.message;
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }

        std::error_code ec;
        const std::uint64_t preparedSize =
            static_cast<std::uint64_t>(std::filesystem::file_size(prepared.preparedPath, ec));
        if (ec || preparedSize == 0) {
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            result.message = "Taille de la version modifiée invalide";
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }
        if (detail::cancellationRequested(shouldCancel)) {
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            appendCancelled(std::move(result), "Opération annulée avant l'envoi cloud");
            break;
        }

        report(std::string(phase::kUpload));
        const auto lock = uploadsApi.lockStorageSpace(context.accessToken,
                                                      context.xxToken,
                                                      item.fileName,
                                                      preparedSize,
                                                      false);
        if (!lock.ok) {
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            result.message = lock.message;
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }

        const auto unlock = [&](bool deleteCos) {
            return unlockStorageWithRetry(uploadsApi,
                                          context.accessToken,
                                          context.xxToken,
                                          lock.lockId,
                                          deleteCos);
        };
        const auto cancelBeforeRegistration = [&](std::string message) {
            const auto unlockResult = unlock(true);
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            if (unlockResult.ok) {
                appendCancelled(std::move(result), std::move(message));
            } else {
                appendPartialCancellation(
                    std::move(result),
                    std::move(message)
                        + "; déverrouillage de l'espace cloud non confirmé: "
                        + unlockResult.message);
            }
        };

        if (detail::cancellationRequested(shouldCancel)) {
            cancelBeforeRegistration("Opération annulée après la réservation cloud");
            break;
        }

        const auto put = uploadsApi.putPresigned(lock.preSignUrl, prepared.preparedPath, shouldCancel);
        if (put.cancelled || detail::cancellationRequested(shouldCancel)) {
            cancelBeforeRegistration("Opération annulée pendant l'envoi binaire");
            break;
        }
        if (!put.ok) {
            const auto unlockResult = unlock(true);
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            result.message = put.message;
            if (unlockResult.ok) {
                ++summary.failed;
            } else {
                result.status = "partial";
                result.message += "; déverrouillage de l'espace cloud non confirmé: "
                                + unlockResult.message;
                ++summary.partial;
            }
            summary.items.push_back(std::move(result));
            if (!unlockResult.ok) {
                break;
            }
            continue;
        }
        const auto registered = uploadsApi.registerUploadedFile(context.accessToken,
                                                                context.xxToken,
                                                                lock.lockId);
        if (!registered.ok || registered.fileId.empty()) {
            const auto unlockResult = unlock(true);
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            result.message = registered.message.empty()
                                 ? "Enregistrement de la version modifiée échoué"
                                 : registered.message;
            if (unlockResult.ok) {
                ++summary.failed;
            } else {
                result.status = "partial";
                result.message += "; déverrouillage de l'espace cloud non confirmé: "
                                + unlockResult.message;
                ++summary.partial;
            }
            summary.items.push_back(std::move(result));
            if (!unlockResult.ok) {
                break;
            }
            continue;
        }
        result.newFileId = registered.fileId;

        bool cloudProcessingReported = false;
        bool thumbnailValidationReported = false;
        CloudPwszReplacementOperations operations;
        operations.unlockRegistered = [&]() {
            const auto value = unlock(false);
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            return CloudPwszReplacementActionResult{value.ok, value.message};
        };
        operations.getUploadStatus = [&]() {
            if (!cloudProcessingReported) {
                report(std::string(phase::kCloudProcessing));
                cloudProcessingReported = true;
            }
            const auto value = uploadsApi.getUploadStatus(context.accessToken,
                                                          context.xxToken,
                                                          registered.fileId);
            return CloudPwszReplacementStatusResult{value.ok, value.status, value.gcodeId};
        };
        operations.listFiles = [&]() {
            if (!thumbnailValidationReported) {
                report(std::string(phase::kValidateThumbnail));
                thumbnailValidationReported = true;
            }
            const auto value = filesApi.list(context.accessToken, context.xxToken, 1, 1500);
            CloudPwszReplacementListResult mapped;
            mapped.ok = value.ok;
            mapped.files.reserve(value.files.size());
            for (const auto& file : value.files) {
                mapped.files.push_back({file.id, file.thumbnailUrl, file.thumbnailCandidates});
            }
            return mapped;
        };
        operations.validateThumbnail = validateThumbnailUrl;
        operations.removeFile = [&](const std::string& fileId) {
            if (fileId == item.fileId) {
                report(std::string(phase::kDeleteOriginal));
            }
            const auto value = filesApi.remove(context.accessToken, context.xxToken, fileId);
            return CloudPwszReplacementActionResult{value.ok, value.message};
        };
        operations.wait = [&](std::chrono::milliseconds delay) {
            return detail::waitInterruptibly(delay, shouldCancel);
        };
        operations.shouldCancel = shouldCancel;

        const auto outcome = finalizeRegisteredCloudPwszReplacement(
            item.fileId, registered.fileId, operations);
        result.status = outcome.status;
        result.message = outcome.message;
        summary.cancelled = summary.cancelled || outcome.cancelled;

        if (outcome.status == "modified") {
            ++summary.modified;
            logging::info("cloud", "pwsz_cloud_update", "file_modified",
                          "Cloud PWSZ preview update completed",
                          {{"old_file_id", item.fileId},
                           {"new_file_id", registered.fileId},
                           {"file_name", item.fileName}});
        } else if (outcome.status == "partial") {
            ++summary.partial;
        } else {
            ++summary.failed;
        }
        summary.items.push_back(std::move(result));

        if (outcome.cancelled) {
            break;
        }
    }

    summary.ok = summary.failed == 0 && summary.partial == 0 && !summary.cancelled;
    return summary;
#endif
}

} // namespace accloud::usecases::cloud
