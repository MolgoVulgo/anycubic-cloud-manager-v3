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
#include <QFile>
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

namespace accloud::usecases::cloud {
namespace {

#ifdef ACCLOUD_WITH_QT

struct DownloadResult {
    bool ok{false};
    std::string message;
};

struct ThumbnailProbeResult {
    bool valid{false};
    bool tooSmall{false};
    std::string message;
};

constexpr int kDownloadTimeoutMs = 300000;
constexpr int kThumbnailTimeoutMs = 15000;

DownloadResult downloadToFile(const std::string& signedUrl,
                              const std::filesystem::path& destination) {
    if (signedUrl.empty() || destination.empty()) {
        return {false, "URL ou destination vide"};
    }

    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(QString::fromStdString(signedUrl))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kDownloadTimeoutMs);

    QEventLoop loop;
    QNetworkReply* reply = nam.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError
                 && status >= 200 && status < 300
                 && !body.isEmpty();
    const std::string error = reply->errorString().toStdString();
    reply->deleteLater();
    if (!ok) {
        return {false, error.empty() ? "Téléchargement cloud échoué" : error};
    }

    QSaveFile output(QString::fromStdString(destination.string()));
    if (!output.open(QIODevice::WriteOnly)) {
        return {false, "Impossible de créer le fichier téléchargé"};
    }
    if (output.write(body) != body.size() || !output.commit()) {
        output.cancelWriting();
        return {false, "Écriture du fichier téléchargé échouée"};
    }
    return {true, "Téléchargement terminé"};
}

ThumbnailProbeResult probeThumbnail(const std::string& url) {
    if (url.empty()) {
        return {false, false, "URL de miniature vide"};
    }

    QNetworkAccessManager nam;
    QNetworkRequest request{QUrl(QString::fromStdString(url))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kThumbnailTimeoutMs);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    QNetworkReply* reply = nam.get(request);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(kThumbnailTimeoutMs);
    loop.exec();
    timeout.stop();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const bool networkOk = !timedOut && reply->error() == QNetworkReply::NoError
                         && status >= 200 && status < 300;
    const std::string error = reply->errorString().toStdString();
    reply->deleteLater();
    if (!networkOk) {
        return {false, false, error.empty() ? "Téléchargement miniature échoué" : error};
    }

    const auto validation = accloud::cloud::thumbnail::validateThumbnailBytes(body);
    return {validation.valid,
            validation.tooSmall,
            validation.valid ? "Miniature valide" : validation.error.toStdString()};
}

bool uploadIsReady(int status, const std::string& gcodeId) {
    return status == 1 || (!gcodeId.empty() && gcodeId != "0");
}

#endif

} // namespace

CloudPwszPreviewUpdateResult UpdateCloudPwszPreviewsUseCase::execute(
    const std::vector<CloudPwszPreviewUpdateItem>& items,
    const ProgressCallback& onProgress) const {
    CloudPwszPreviewUpdateResult summary;
    const auto orderedItems = orderCloudPwszPreviewUpdateItemsOldestFirst(items);
    summary.items.reserve(orderedItems.size());

#ifndef ACCLOUD_WITH_QT
    (void)onProgress;
    for (const auto& item : orderedItems) {
        summary.items.push_back({item.fileId, {}, item.fileName, "failed", "Qt non disponible"});
        ++summary.failed;
    }
    return summary;
#else
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
        const auto& item = orderedItems[static_cast<std::size_t>(index)];
        const int current = index + 1;
        const auto report = [&](const std::string& phase) {
            if (onProgress) {
                onProgress(current, total, item.fileName, phase);
            }
        };

        CloudPwszPreviewUpdateItemResult result;
        result.originalFileId = item.fileId;
        result.fileName = item.fileName;
        result.status = "failed";

        report("Téléchargement du fichier cloud");
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
        const auto download = downloadToFile(signedUrl.url, originalPath);
        if (!download.ok) {
            result.message = download.message;
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }

        report("Modification du fichier PWSZ");
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

        report("Envoi de la version modifiée");
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
            return uploadsApi.unlockStorageSpace(context.accessToken,
                                                 context.xxToken,
                                                 lock.lockId,
                                                 deleteCos);
        };

        const auto put = uploadsApi.putPresigned(lock.preSignUrl, prepared.preparedPath);
        if (!put.ok) {
            unlock(true);
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            result.message = put.message;
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }

        const auto registered = uploadsApi.registerUploadedFile(context.accessToken,
                                                                context.xxToken,
                                                                lock.lockId);
        if (!registered.ok || registered.fileId.empty()) {
            unlock(true);
            accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);
            result.message = registered.message.empty()
                                 ? "Enregistrement de la version modifiée échoué"
                                 : registered.message;
            ++summary.failed;
            summary.items.push_back(std::move(result));
            continue;
        }
        result.newFileId = registered.fileId;
        unlock(false);
        accloud::cloud::archive::discardPreparedFile(prepared.preparedPath);

        report("Traitement cloud de la version modifiée");
        bool ready = false;
        static constexpr std::array<std::chrono::milliseconds, 8> delays = {
            std::chrono::milliseconds(500),
            std::chrono::milliseconds(1000),
            std::chrono::milliseconds(2000),
            std::chrono::milliseconds(4000),
            std::chrono::milliseconds(8000),
            std::chrono::milliseconds(12000),
            std::chrono::milliseconds(15000),
            std::chrono::milliseconds(20000)
        };
        for (const auto delay : delays) {
            std::this_thread::sleep_for(delay);
            const auto status = uploadsApi.getUploadStatus(context.accessToken,
                                                           context.xxToken,
                                                           registered.fileId);
            if (status.ok && uploadIsReady(status.status, status.gcodeId)) {
                ready = true;
                break;
            }
        }
        if (!ready) {
            const auto cleanup = filesApi.remove(context.accessToken,
                                                 context.xxToken,
                                                 registered.fileId);
            if (cleanup.ok) {
                result.message = "Traitement cloud de la version modifiée non confirmé";
                ++summary.failed;
            } else {
                result.status = "partial";
                result.message = "Traitement cloud non confirmé et nouvelle version impossible à supprimer; l'original est conservé";
                ++summary.partial;
            }
            summary.items.push_back(std::move(result));
            continue;
        }

        report("Validation de la nouvelle miniature");
        bool thumbnailValid = false;
        for (int attempt = 0; attempt < 8 && !thumbnailValid; ++attempt) {
            const auto list = filesApi.list(context.accessToken, context.xxToken, 1, 1500);
            if (list.ok) {
                for (const auto& file : list.files) {
                    if (file.id != registered.fileId) {
                        continue;
                    }
                    for (const auto& candidate : file.thumbnailCandidates) {
                        const auto probe = probeThumbnail(candidate);
                        if (probe.valid) {
                            thumbnailValid = true;
                            break;
                        }
                    }
                    if (!thumbnailValid && !file.thumbnailUrl.empty()) {
                        thumbnailValid = probeThumbnail(file.thumbnailUrl).valid;
                    }
                    break;
                }
            }
            if (!thumbnailValid) {
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }
        }
        if (!thumbnailValid) {
            const auto cleanup = filesApi.remove(context.accessToken,
                                                 context.xxToken,
                                                 registered.fileId);
            if (cleanup.ok) {
                result.message = "La nouvelle miniature est toujours invalide";
                ++summary.failed;
            } else {
                result.status = "partial";
                result.message = "Nouvelle miniature invalide et nouvelle version impossible à supprimer; l'original est conservé";
                ++summary.partial;
            }
            summary.items.push_back(std::move(result));
            continue;
        }

        report("Suppression de l'ancienne version");
        const auto removal = filesApi.remove(context.accessToken,
                                             context.xxToken,
                                             item.fileId);
        if (!removal.ok) {
            result.status = "partial";
            result.message = "Nouvelle version valide, mais suppression de l'ancienne version impossible";
            ++summary.partial;
            summary.items.push_back(std::move(result));
            continue;
        }

        result.status = "modified";
        result.message = "Fichier modifié";
        ++summary.modified;
        summary.items.push_back(std::move(result));
        logging::info("cloud", "pwsz_cloud_update", "file_modified",
                      "Cloud PWSZ preview update completed",
                      {{"old_file_id", item.fileId},
                       {"new_file_id", registered.fileId},
                       {"file_name", item.fileName}});
    }

    summary.ok = summary.failed == 0 && summary.partial == 0;
    return summary;
#endif
}

} // namespace accloud::usecases::cloud
