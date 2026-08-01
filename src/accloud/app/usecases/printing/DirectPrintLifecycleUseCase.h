#pragma once

#include "domain/printing/DirectPrintOperation.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace accloud::usecases::printing {

struct DirectPrintProjectSnapshot {
    std::string printerId;
    std::string taskId;
    std::string cloudFileId;
    std::string currentFile;
    std::string gcodeName;
    int printStatus{0};
};

enum class DirectPrintCompletionKind {
    None,
    Success,
    Failure,
};

enum class DirectPrintLifecycleEffectKind {
    PersistOperation,
    RemoveOperation,
    RequestLocalDelete,
    RequestCloudDelete,
};

struct DirectPrintLifecycleEffect {
    DirectPrintLifecycleEffectKind kind{DirectPrintLifecycleEffectKind::PersistOperation};
    DirectPrintCompletionKind completionKind{DirectPrintCompletionKind::None};
};

struct DirectPrintLifecycleContext {
    bool localDeleteInFlight{false};
    bool cloudDeleteInFlight{false};
};

struct DirectPrintLifecycleTransition {
    accloud::printing::DirectPrintOperation operation;
    std::vector<DirectPrintLifecycleEffect> effects;
    bool matchedProject{false};
};

struct DirectPrintLocalDeleteDispatchResult {
    bool accepted{false};
    bool confirmationPending{false};
};

[[nodiscard]] std::optional<std::size_t> findMatchingDirectPrintProject(
    const accloud::printing::DirectPrintOperation& operation,
    const std::vector<DirectPrintProjectSnapshot>& projects);

[[nodiscard]] DirectPrintLifecycleTransition reconcileDirectPrintOperation(
    accloud::printing::DirectPrintOperation operation,
    const std::vector<DirectPrintProjectSnapshot>& projects,
    DirectPrintLifecycleContext context = {});

[[nodiscard]] DirectPrintLifecycleTransition beginDirectPrintLocalDelete(
    accloud::printing::DirectPrintOperation operation,
    DirectPrintCompletionKind completionKind,
    bool localDeleteInFlight = false);

[[nodiscard]] DirectPrintLifecycleTransition beginDirectPrintCloudDelete(
    accloud::printing::DirectPrintOperation operation,
    bool cloudDeleteInFlight = false);

[[nodiscard]] DirectPrintLifecycleTransition handleDirectPrintLocalDeleteDispatch(
    accloud::printing::DirectPrintOperation operation,
    DirectPrintCompletionKind completionKind,
    DirectPrintLocalDeleteDispatchResult result);

[[nodiscard]] DirectPrintLifecycleTransition handleDirectPrintLocalDeleteConfirmation(
    accloud::printing::DirectPrintOperation operation,
    DirectPrintCompletionKind completionKind,
    bool success,
    bool cloudDeleteInFlight = false);

[[nodiscard]] DirectPrintLifecycleTransition handleDirectPrintCloudDeleteResult(
    accloud::printing::DirectPrintOperation operation,
    bool success);

} // namespace accloud::usecases::printing
