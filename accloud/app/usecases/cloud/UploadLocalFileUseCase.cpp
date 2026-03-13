#include "UploadLocalFileUseCase.h"

#include "infra/cloud/api/UploadsApi.h"
#include "infra/cloud/core/SessionProvider.h"
#include "infra/logging/JsonlLogger.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <thread>
#include <utility>

namespace accloud::usecases::cloud {

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
    reportProgress(0.74, "Verification statut upload");

    accloud::cloud::CloudUploadStatusResult statusResult;
    bool statusReceived = false;
    static constexpr std::array<std::chrono::milliseconds, 3> kStatusPollDelays = {
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(250),
        std::chrono::milliseconds(900),
    };
    std::size_t statusPollIndex = 0;
    for (const auto& delay : kStatusPollDelays) {
        if (delay.count() > 0) {
            std::this_thread::sleep_for(delay);
        }
        reportProgress(0.74 + (0.14 * (statusPollIndex + 1)), "Verification statut upload");
        statusResult = uploadsApi.getUploadStatus(contextResult.context.accessToken,
                                                  contextResult.context.xxToken,
                                                  registerResult.fileId);
        if (statusResult.ok) {
            statusReceived = true;
            if (!statusResult.gcodeId.empty() || statusResult.status == 1 || statusResult.status == 2) {
                break;
            }
        }
        ++statusPollIndex;
    }

    out.ok = true;
    out.fileId = registerResult.fileId;
    out.uploadStatus = statusReceived ? statusResult.status : 0;
    out.gcodeId = statusReceived ? statusResult.gcodeId : std::string{};
    if (statusReceived) {
        out.message = statusResult.message.empty()
                          ? "Upload terminé."
                          : statusResult.message;
        logging::info("cloud", "upload_local_file", "status_ok",
                      "getUploadStatus completed",
                      {{"file_id", out.fileId},
                       {"status", std::to_string(out.uploadStatus)},
                       {"gcode_id", out.gcodeId.empty() ? "0" : out.gcodeId}});
    } else {
        out.message = "Upload terminé (status en cours).";
        logging::warn("cloud", "upload_local_file", "status_pending",
                      "Upload registered but no confirmed status yet",
                      {{"file_id", out.fileId}});
    }

    const auto unlockResult = finalizeUnlock(false);
    out.unlockOk = unlockResult.ok;
    if (!unlockResult.ok) {
        out.message += " Unlock warning: " + unlockResult.message;
        logging::warn("cloud", "upload_local_file", "unlock_after_success_failed",
                      "unlockStorageSpace failed after successful upload",
                      {{"lock_id", lockResult.lockId},
                       {"file_id", out.fileId}});
    } else {
        logging::info("cloud", "upload_local_file", "unlock_ok",
                      "unlockStorageSpace completed after upload",
                      {{"lock_id", lockResult.lockId},
                       {"file_id", out.fileId}});
    }

    reportProgress(1.0, "Upload termine");

    logging::info("cloud", "upload_local_file", "done",
                  "Upload workflow completed",
                  {{"file_id", out.fileId},
                   {"gcode_id", out.gcodeId.empty() ? "0" : out.gcodeId},
                   {"status", std::to_string(out.uploadStatus)},
                   {"unlock_ok", out.unlockOk ? "1" : "0"}});

    return out;
}

} // namespace accloud::usecases::cloud
