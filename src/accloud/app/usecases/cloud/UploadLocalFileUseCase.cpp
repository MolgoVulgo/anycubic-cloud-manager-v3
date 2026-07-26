#include "UploadLocalFileUseCase.h"

#include "infra/cloud/api/UploadsApi.h"
#include "infra/cloud/core/SessionProvider.h"
#include "infra/logging/JsonlLogger.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <thread>
#include <utility>

namespace accloud::usecases::cloud {
namespace {

std::string trimAscii(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    const auto first = std::find_if(value.begin(), value.end(), notSpace);
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if(value.rbegin(), value.rend(), notSpace).base();
    return std::string(first, last);
}

} // namespace

bool UploadLocalFileUseCase::hasUsableGcodeId(const std::string& gcodeId) {
    const std::string normalized = trimAscii(gcodeId);
    return !normalized.empty() && normalized != "0";
}

bool UploadLocalFileUseCase::isUploadReady(int uploadStatus, const std::string& gcodeId) {
    return uploadStatus == 1 || hasUsableGcodeId(gcodeId);
}

UploadLocalFileResult UploadLocalFileUseCase::execute(const std::string& localPath,
                                                      const ProgressCallback& onProgress) const {
    UploadLocalFileResult out;
    const auto reportProgress = [&](double progress, const std::string& phase) {
        if (onProgress) {
            onProgress(progress, phase);
        }
    };

    reportProgress(0.02, "Validation du fichier local");

    if (localPath.empty()) {
        out.ok = false;
        out.message = "Chemin de fichier vide.";
        reportProgress(1.0, "Echec: chemin vide");
        return out;
    }

    std::error_code ec;
    const std::filesystem::path filePath(localPath);
    if (!std::filesystem::exists(filePath, ec) || ec || !std::filesystem::is_regular_file(filePath, ec)) {
        out.ok = false;
        out.message = "Fichier local introuvable.";
        reportProgress(1.0, "Echec: fichier introuvable");
        return out;
    }

    const auto fileSize = static_cast<std::uint64_t>(std::filesystem::file_size(filePath, ec));
    if (ec || fileSize == 0) {
        out.ok = false;
        out.message = "Taille de fichier invalide.";
        reportProgress(1.0, "Echec: taille invalide");
        return out;
    }

    const std::string fileName = filePath.filename().string();
    if (fileName.empty()) {
        out.ok = false;
        out.message = "Nom de fichier invalide.";
        reportProgress(1.0, "Echec: nom invalide");
        return out;
    }

    logging::info("cloud", "upload_local_file", "start",
                  "Start uploadLocalFile workflow",
                  {{"file_name", fileName},
                   {"file_size", std::to_string(fileSize)}});
    reportProgress(0.08, "Validation de session");

    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        out.ok = false;
        out.message = "Session invalide ou introuvable";
        logging::warn("cloud", "upload_local_file", "session_invalid",
                      "Upload aborted: invalid session");
        reportProgress(1.0, "Echec: session invalide");
        return out;
    }

    const accloud::cloud::api::UploadsApi uploadsApi;
    reportProgress(0.18, "Reservation espace cloud");
    const auto lockResult = uploadsApi.lockStorageSpace(contextResult.context.accessToken,
                                                        contextResult.context.xxToken,
                                                        fileName,
                                                        fileSize,
                                                        false);
    if (!lockResult.ok) {
        out.ok = false;
        out.message = lockResult.message;
        logging::warn("cloud", "upload_local_file", "lock_failed",
                      "Upload aborted: lockStorageSpace failed",
                      {{"file_name", fileName}});
        reportProgress(1.0, "Echec: lock storage");
        return out;
    }
    logging::info("cloud", "upload_local_file", "lock_ok",
                  "lockStorageSpace completed",
                  {{"lock_id", lockResult.lockId}});

    const auto finalizeUnlock = [&](bool deleteCos) {
        return uploadsApi.unlockStorageSpace(contextResult.context.accessToken,
                                             contextResult.context.xxToken,
                                             lockResult.lockId,
                                             deleteCos);
    };

    const auto failWithUnlock = [&](std::string message) {
        UploadLocalFileResult failure;
        failure.ok = false;
        failure.message = std::move(message);
        const auto unlockResult = finalizeUnlock(true);
        failure.unlockOk = unlockResult.ok;
        if (!unlockResult.ok) {
            failure.message += " Unlock failed: " + unlockResult.message;
            logging::warn("cloud", "upload_local_file", "unlock_after_failure_failed",
                          "unlockStorageSpace failed after upload error",
                          {{"lock_id", lockResult.lockId}});
        }
        logging::warn("cloud", "upload_local_file", "failed",
                      "Upload workflow failed",
                      {{"lock_id", lockResult.lockId},
                       {"message", failure.message}});
        reportProgress(1.0, "Echec upload");
        return failure;
    };

    reportProgress(0.30, "Upload binaire");
    const auto putResult = uploadsApi.putPresigned(lockResult.preSignUrl, filePath);
    if (!putResult.ok) {
        return failWithUnlock("Upload binaire échoué: " + putResult.message);
    }
    logging::info("cloud", "upload_local_file", "put_ok",
                  "Binary upload step completed",
                  {{"lock_id", lockResult.lockId},
                   {"http", std::to_string(putResult.httpStatus)}});
    reportProgress(0.62, "Enregistrement fichier cloud");

    const auto registerResult = uploadsApi.registerUploadedFile(contextResult.context.accessToken,
                                                                contextResult.context.xxToken,
                                                                lockResult.lockId);
    if (!registerResult.ok) {
        return failWithUnlock("Enregistrement upload échoué: " + registerResult.message);
    }
    logging::info("cloud", "upload_local_file", "register_ok",
                  "newUploadFile completed",
                  {{"lock_id", lockResult.lockId},
                   {"file_id", registerResult.fileId}});

    out.ok = true;
    out.fileId = registerResult.fileId;

    reportProgress(0.72, "Finalisation espace cloud");
    const auto unlockResult = finalizeUnlock(false);
    out.unlockOk = unlockResult.ok;
    if (!unlockResult.ok) {
        logging::warn("cloud", "upload_local_file", "unlock_after_success_failed",
                      "unlockStorageSpace failed after successful upload",
                      {{"lock_id", lockResult.lockId},
                       {"file_id", out.fileId}});
    } else {
        logging::info("cloud", "upload_local_file", "unlock_ok",
                      "unlockStorageSpace completed before status polling",
                      {{"lock_id", lockResult.lockId},
                       {"file_id", out.fileId}});
    }

    reportProgress(0.78, "Verification statut upload");
    accloud::cloud::CloudUploadStatusResult statusResult;
    bool statusReceived = false;
    static constexpr std::array<std::chrono::milliseconds, 6> kStatusPollDelays = {
        std::chrono::milliseconds(500),
        std::chrono::milliseconds(1000),
        std::chrono::milliseconds(2000),
        std::chrono::milliseconds(4000),
        std::chrono::milliseconds(8000),
        std::chrono::milliseconds(15000),
    };

    for (std::size_t index = 0; index < kStatusPollDelays.size(); ++index) {
        std::this_thread::sleep_for(kStatusPollDelays[index]);
        const double progress = 0.78 + (0.18 * static_cast<double>(index + 1)
                                        / static_cast<double>(kStatusPollDelays.size()));
        reportProgress(progress, "Verification statut upload");
        statusResult = uploadsApi.getUploadStatus(contextResult.context.accessToken,
                                                  contextResult.context.xxToken,
                                                  registerResult.fileId);
        if (!statusResult.ok) {
            continue;
        }
        statusReceived = true;
        if (isUploadReady(statusResult.status, statusResult.gcodeId)) {
            break;
        }
    }

    out.uploadStatus = statusReceived ? statusResult.status : 0;
    out.gcodeId = statusReceived ? statusResult.gcodeId : std::string{};
    const bool uploadReady = statusReceived && isUploadReady(out.uploadStatus, out.gcodeId);
    if (uploadReady) {
        out.message = "Upload terminé.";
        logging::info("cloud", "upload_local_file", "status_ready",
                      "Cloud processing completed after unlock",
                      {{"file_id", out.fileId},
                       {"status", std::to_string(out.uploadStatus)},
                       {"gcode_id", out.gcodeId.empty() ? "0" : out.gcodeId}});
    } else if (statusReceived) {
        out.message = "Upload transfere (traitement cloud en cours).";
        logging::warn("cloud", "upload_local_file", "status_processing",
                      "Upload registered and transferred, cloud processing still pending",
                      {{"file_id", out.fileId},
                       {"status", std::to_string(out.uploadStatus)},
                       {"gcode_id", out.gcodeId.empty() ? "0" : out.gcodeId}});
    } else {
        out.message = "Upload transfere (statut cloud indisponible pour le moment).";
        logging::warn("cloud", "upload_local_file", "status_pending",
                      "Upload registered but no confirmed status yet",
                      {{"file_id", out.fileId}});
    }

    if (!out.unlockOk) {
        out.message += " Unlock warning: " + unlockResult.message;
    }

    reportProgress(1.0, uploadReady ? "Upload termine" : "Traitement cloud en cours");

    logging::info("cloud", "upload_local_file", "done",
                  "Upload workflow completed",
                  {{"file_id", out.fileId},
                   {"gcode_id", out.gcodeId.empty() ? "0" : out.gcodeId},
                   {"status", std::to_string(out.uploadStatus)},
                   {"ready", uploadReady ? "1" : "0"},
                   {"unlock_ok", out.unlockOk ? "1" : "0"}});

    return out;
}

} // namespace accloud::usecases::cloud
