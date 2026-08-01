#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace accloud::printing {

enum class DirectPrintState {
    Uploaded,
    PrintCommandSent,
    Printing,
    SuccessLocalDeletePending,
    FailureLocalDeletePending,
    SuccessLocalCleanupError,
    FailedLocalCleanupError,
    CloudDeletePending,
    CloudDeleteError,
    Unknown,
};

[[nodiscard]] constexpr std::string_view directPrintStateKey(DirectPrintState state) noexcept {
    switch (state) {
    case DirectPrintState::Uploaded:
        return "UPLOADED";
    case DirectPrintState::PrintCommandSent:
        return "PRINT_COMMAND_SENT";
    case DirectPrintState::Printing:
        return "PRINTING";
    case DirectPrintState::SuccessLocalDeletePending:
        return "SUCCESS_LOCAL_DELETE_PENDING";
    case DirectPrintState::FailureLocalDeletePending:
        return "FAILURE_LOCAL_DELETE_PENDING";
    case DirectPrintState::SuccessLocalCleanupError:
        return "SUCCESS_LOCAL_CLEANUP_ERROR";
    case DirectPrintState::FailedLocalCleanupError:
        return "FAILED_LOCAL_CLEANUP_ERROR";
    case DirectPrintState::CloudDeletePending:
        return "CLOUD_DELETE_PENDING";
    case DirectPrintState::CloudDeleteError:
        return "CLOUD_DELETE_ERROR";
    case DirectPrintState::Unknown:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

[[nodiscard]] constexpr DirectPrintState directPrintStateFromPersisted(
    std::string_view value) noexcept {
    if (value == "UPLOADED") return DirectPrintState::Uploaded;
    if (value == "PRINT_COMMAND_SENT") return DirectPrintState::PrintCommandSent;
    if (value == "PRINTING") return DirectPrintState::Printing;
    if (value == "SUCCESS_LOCAL_DELETE_PENDING") return DirectPrintState::SuccessLocalDeletePending;
    if (value == "FAILURE_LOCAL_DELETE_PENDING") return DirectPrintState::FailureLocalDeletePending;
    if (value == "SUCCESS_LOCAL_CLEANUP_ERROR") return DirectPrintState::SuccessLocalCleanupError;
    if (value == "FAILED_LOCAL_CLEANUP_ERROR") return DirectPrintState::FailedLocalCleanupError;
    if (value == "CLOUD_DELETE_PENDING") return DirectPrintState::CloudDeletePending;
    if (value == "CLOUD_DELETE_ERROR") return DirectPrintState::CloudDeleteError;
    return DirectPrintState::Unknown;
}

[[nodiscard]] constexpr bool isDirectPrintCleanupError(DirectPrintState state) noexcept {
    return state == DirectPrintState::SuccessLocalCleanupError
        || state == DirectPrintState::FailedLocalCleanupError;
}

struct DirectPrintOperation {
    std::string printerId;
    std::string cloudFileId;
    std::string cloudGcodeId;
    std::string cloudFileName;
    std::uint64_t cloudFileSize{0};

    std::string printTaskId;
    std::string printMsgId;

    std::string printerLocalFilename;
    std::string printerLocalPath{"/"};

    bool deleteAfterSuccess{false};
    bool deleteLocalOnFailure{false};
    bool observedActive{false};

    DirectPrintState state{DirectPrintState::Uploaded};
    std::int64_t createdAt{0};
    std::int64_t updatedAt{0};
};

} // namespace accloud::printing
