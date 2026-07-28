#include "CloudPwszReplacementWorkflow.h"

#include <array>
#include <chrono>
#include <thread>
#include <utility>

namespace accloud::usecases::cloud {
namespace {

bool cancellationRequested(const CloudPwszReplacementOperations& operations) {
    return operations.shouldCancel && operations.shouldCancel();
}

bool waitFor(std::chrono::milliseconds delay,
             const CloudPwszReplacementOperations& operations) {
    if (cancellationRequested(operations)) {
        return false;
    }
    if (operations.wait) {
        return operations.wait(delay) && !cancellationRequested(operations);
    }
    std::this_thread::sleep_for(delay);
    return !cancellationRequested(operations);
}

bool uploadIsReady(const CloudPwszReplacementStatusResult& status) {
    return status.ok
        && (status.status == 1 || (!status.gcodeId.empty() && status.gcodeId != "0"));
}

CloudPwszReplacementOutcome cancelledOutcome(std::string message) {
    return {"partial", std::move(message), true};
}

CloudPwszReplacementOutcome cleanupNewVersion(
    const std::string& newFileId,
    std::string failureMessage,
    std::string cleanupFailureMessage,
    const CloudPwszReplacementOperations& operations) {
    const CloudPwszReplacementActionResult cleanup = operations.removeFile
        ? operations.removeFile(newFileId)
        : CloudPwszReplacementActionResult{false, "Opération de suppression indisponible"};
    if (cleanup.ok) {
        return {"failed", std::move(failureMessage), false};
    }
    if (!cleanup.message.empty()) {
        cleanupFailureMessage += ": " + cleanup.message;
    }
    return {"partial", std::move(cleanupFailureMessage), false};
}

} // namespace

CloudPwszReplacementOutcome finalizeRegisteredCloudPwszReplacement(
    const std::string& originalFileId,
    const std::string& newFileId,
    const CloudPwszReplacementOperations& operations) {
    const CloudPwszReplacementActionResult unlock = operations.unlockRegistered
        ? operations.unlockRegistered()
        : CloudPwszReplacementActionResult{false, "Opération de déverrouillage indisponible"};
    if (!unlock.ok) {
        std::string message =
            "Nouvelle version enregistrée, mais déverrouillage de l'espace cloud non confirmé; "
            "l'original est conservé";
        if (!unlock.message.empty()) {
            message += ": " + unlock.message;
        }
        return {"partial", std::move(message), false};
    }

    if (cancellationRequested(operations)) {
        return cancelledOutcome(
            "Opération annulée après l'enregistrement; la nouvelle version et l'original sont conservés");
    }

    static constexpr std::array<std::chrono::milliseconds, 8> statusDelays = {
        std::chrono::milliseconds(500),
        std::chrono::milliseconds(1000),
        std::chrono::milliseconds(2000),
        std::chrono::milliseconds(4000),
        std::chrono::milliseconds(8000),
        std::chrono::milliseconds(12000),
        std::chrono::milliseconds(15000),
        std::chrono::milliseconds(20000),
    };

    bool ready = false;
    for (const auto delay : statusDelays) {
        if (!waitFor(delay, operations)) {
            return cancelledOutcome(
                "Opération annulée pendant le traitement cloud; la nouvelle version et l'original sont conservés");
        }
        const CloudPwszReplacementStatusResult status = operations.getUploadStatus
            ? operations.getUploadStatus()
            : CloudPwszReplacementStatusResult{};
        if (cancellationRequested(operations)) {
            return cancelledOutcome(
                "Opération annulée pendant le traitement cloud; la nouvelle version et l'original sont conservés");
        }
        if (uploadIsReady(status)) {
            ready = true;
            break;
        }
    }

    if (!ready) {
        return cleanupNewVersion(
            newFileId,
            "Traitement cloud de la version modifiée non confirmé",
            "Traitement cloud non confirmé et nouvelle version impossible à supprimer; l'original est conservé",
            operations);
    }

    bool thumbnailValid = false;
    for (int attempt = 0; attempt < 8 && !thumbnailValid; ++attempt) {
        if (cancellationRequested(operations)) {
            return cancelledOutcome(
                "Opération annulée pendant la validation; la nouvelle version et l'original sont conservés");
        }

        const CloudPwszReplacementListResult list = operations.listFiles
            ? operations.listFiles()
            : CloudPwszReplacementListResult{};
        if (cancellationRequested(operations)) {
            return cancelledOutcome(
                "Opération annulée pendant la validation; la nouvelle version et l'original sont conservés");
        }

        if (list.ok) {
            for (const auto& file : list.files) {
                if (file.fileId != newFileId) {
                    continue;
                }
                for (const auto& candidate : file.thumbnailCandidates) {
                    const CloudPwszThumbnailValidationResult probe = operations.validateThumbnail
                        ? operations.validateThumbnail(candidate)
                        : CloudPwszThumbnailValidationResult{};
                    if (probe.cancelled || cancellationRequested(operations)) {
                        return cancelledOutcome(
                            "Opération annulée pendant la validation; la nouvelle version et l'original sont conservés");
                    }
                    if (probe.valid) {
                        thumbnailValid = true;
                        break;
                    }
                }
                if (!thumbnailValid && !file.thumbnailUrl.empty()) {
                    const CloudPwszThumbnailValidationResult probe = operations.validateThumbnail
                        ? operations.validateThumbnail(file.thumbnailUrl)
                        : CloudPwszThumbnailValidationResult{};
                    if (probe.cancelled || cancellationRequested(operations)) {
                        return cancelledOutcome(
                            "Opération annulée pendant la validation; la nouvelle version et l'original sont conservés");
                    }
                    thumbnailValid = probe.valid;
                }
                break;
            }
        }

        if (!thumbnailValid && attempt + 1 < 8
            && !waitFor(std::chrono::milliseconds(3000), operations)) {
            return cancelledOutcome(
                "Opération annulée pendant la validation; la nouvelle version et l'original sont conservés");
        }
    }

    if (!thumbnailValid) {
        return cleanupNewVersion(
            newFileId,
            "La nouvelle miniature est toujours invalide",
            "Nouvelle miniature invalide et nouvelle version impossible à supprimer; l'original est conservé",
            operations);
    }

    if (cancellationRequested(operations)) {
        return cancelledOutcome(
            "Opération annulée avant la suppression; la nouvelle version et l'original sont conservés");
    }

    const CloudPwszReplacementActionResult removal = operations.removeFile
        ? operations.removeFile(originalFileId)
        : CloudPwszReplacementActionResult{false, "Opération de suppression indisponible"};
    if (!removal.ok) {
        std::string message =
            "Nouvelle version valide, mais suppression de l'ancienne version impossible";
        if (!removal.message.empty()) {
            message += ": " + removal.message;
        }
        return {"partial", std::move(message), false};
    }

    return {"modified", "Fichier modifié", false};
}

} // namespace accloud::usecases::cloud
