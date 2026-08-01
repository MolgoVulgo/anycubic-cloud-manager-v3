#include "DirectPrintLifecycleUseCase.h"

#include <utility>

namespace accloud::usecases::printing {
namespace {

using accloud::printing::DirectPrintOperation;
using accloud::printing::DirectPrintState;

void addEffect(DirectPrintLifecycleTransition& transition,
               DirectPrintLifecycleEffectKind kind,
               DirectPrintCompletionKind completionKind = DirectPrintCompletionKind::None) {
    transition.effects.push_back({kind, completionKind});
}

DirectPrintLifecycleTransition persistThenRequestLocalDelete(
    DirectPrintOperation operation,
    DirectPrintCompletionKind completionKind,
    bool localDeleteInFlight) {
    DirectPrintLifecycleTransition transition{std::move(operation)};
    if (localDeleteInFlight) {
        return transition;
    }

    transition.operation.state = completionKind == DirectPrintCompletionKind::Success
        ? DirectPrintState::SuccessLocalDeletePending
        : DirectPrintState::FailureLocalDeletePending;
    addEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
    addEffect(transition, DirectPrintLifecycleEffectKind::RequestLocalDelete, completionKind);
    return transition;
}

DirectPrintLifecycleTransition localCleanupError(
    DirectPrintOperation operation,
    DirectPrintCompletionKind completionKind) {
    DirectPrintLifecycleTransition transition{std::move(operation)};
    transition.operation.state = completionKind == DirectPrintCompletionKind::Success
        ? DirectPrintState::SuccessLocalCleanupError
        : DirectPrintState::FailedLocalCleanupError;
    addEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
    return transition;
}

} // namespace

std::optional<std::size_t> findMatchingDirectPrintProject(
    const DirectPrintOperation& operation,
    const std::vector<DirectPrintProjectSnapshot>& projects) {
    std::optional<std::size_t> fallback;
    for (std::size_t index = 0; index < projects.size(); ++index) {
        const auto& project = projects[index];
        const std::string& projectPrinterId = project.printerId.empty()
            ? operation.printerId
            : project.printerId;
        if (projectPrinterId != operation.printerId) {
            continue;
        }
        if (!operation.printTaskId.empty() && project.taskId == operation.printTaskId) {
            return index;
        }
        if (!operation.cloudFileId.empty() && project.cloudFileId == operation.cloudFileId) {
            fallback = index;
        }
    }
    return fallback;
}

DirectPrintLifecycleTransition beginDirectPrintLocalDelete(
    DirectPrintOperation operation,
    DirectPrintCompletionKind completionKind,
    bool localDeleteInFlight) {
    if (completionKind == DirectPrintCompletionKind::None) {
        return {std::move(operation)};
    }
    return persistThenRequestLocalDelete(std::move(operation), completionKind, localDeleteInFlight);
}

DirectPrintLifecycleTransition beginDirectPrintCloudDelete(
    DirectPrintOperation operation,
    bool cloudDeleteInFlight) {
    DirectPrintLifecycleTransition transition{std::move(operation)};
    if (transition.operation.cloudFileId.empty() || cloudDeleteInFlight) {
        return transition;
    }

    transition.operation.state = DirectPrintState::CloudDeletePending;
    addEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
    addEffect(transition, DirectPrintLifecycleEffectKind::RequestCloudDelete);
    return transition;
}

DirectPrintLifecycleTransition reconcileDirectPrintOperation(
    DirectPrintOperation operation,
    const std::vector<DirectPrintProjectSnapshot>& projects,
    DirectPrintLifecycleContext context) {
    DirectPrintLifecycleTransition transition{std::move(operation)};
    const auto match = findMatchingDirectPrintProject(transition.operation, projects);
    if (!match.has_value()) {
        return transition;
    }
    transition.matchedProject = true;

    const auto& project = projects[*match];
    const std::string currentFile = !project.currentFile.empty()
        ? project.currentFile
        : project.gcodeName;
    if (!currentFile.empty()) {
        transition.operation.printerLocalFilename = currentFile;
        transition.operation.printerLocalPath = "/";
    }

    if (transition.operation.state == DirectPrintState::CloudDeletePending
        || transition.operation.state == DirectPrintState::CloudDeleteError) {
        auto cloudTransition = beginDirectPrintCloudDelete(
            std::move(transition.operation), context.cloudDeleteInFlight);
        cloudTransition.matchedProject = true;
        return cloudTransition;
    }

    if (accloud::printing::isDirectPrintCleanupError(transition.operation.state)) {
        return transition;
    }

    if (project.printStatus == 1) {
        transition.operation.observedActive = true;
        transition.operation.state = DirectPrintState::Printing;
        addEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
        return transition;
    }

    if (project.printStatus == 2 && transition.operation.observedActive) {
        if (!transition.operation.deleteAfterSuccess) {
            addEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
            return transition;
        }
        auto cleanup = persistThenRequestLocalDelete(
            std::move(transition.operation),
            DirectPrintCompletionKind::Success,
            context.localDeleteInFlight);
        cleanup.matchedProject = true;
        return cleanup;
    }

    if ((project.printStatus == 3 || project.printStatus == 4)
        && transition.operation.observedActive) {
        const bool canDeleteLocal = transition.operation.deleteAfterSuccess
            && transition.operation.deleteLocalOnFailure
            && !transition.operation.printerLocalFilename.empty();
        if (!canDeleteLocal) {
            addEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
            return transition;
        }
        auto cleanup = persistThenRequestLocalDelete(
            std::move(transition.operation),
            DirectPrintCompletionKind::Failure,
            context.localDeleteInFlight);
        cleanup.matchedProject = true;
        return cleanup;
    }

    return transition;
}

DirectPrintLifecycleTransition handleDirectPrintLocalDeleteDispatch(
    DirectPrintOperation operation,
    DirectPrintCompletionKind completionKind,
    DirectPrintLocalDeleteDispatchResult result) {
    if (completionKind == DirectPrintCompletionKind::None) {
        return {std::move(operation)};
    }
    if (!result.accepted) {
        return localCleanupError(std::move(operation), completionKind);
    }
    if (result.confirmationPending) {
        return {std::move(operation)};
    }
    if (completionKind == DirectPrintCompletionKind::Success) {
        return beginDirectPrintCloudDelete(std::move(operation));
    }

    DirectPrintLifecycleTransition transition{std::move(operation)};
    addEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
    return transition;
}

DirectPrintLifecycleTransition handleDirectPrintLocalDeleteConfirmation(
    DirectPrintOperation operation,
    DirectPrintCompletionKind completionKind,
    bool success,
    bool cloudDeleteInFlight) {
    if (completionKind == DirectPrintCompletionKind::None) {
        return {std::move(operation)};
    }
    if (!success) {
        return localCleanupError(std::move(operation), completionKind);
    }
    if (completionKind == DirectPrintCompletionKind::Success) {
        return beginDirectPrintCloudDelete(std::move(operation), cloudDeleteInFlight);
    }

    DirectPrintLifecycleTransition transition{std::move(operation)};
    addEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
    return transition;
}

DirectPrintLifecycleTransition handleDirectPrintCloudDeleteResult(
    DirectPrintOperation operation,
    bool success) {
    DirectPrintLifecycleTransition transition{std::move(operation)};
    if (success) {
        addEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
        return transition;
    }

    transition.operation.state = DirectPrintState::CloudDeleteError;
    addEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
    return transition;
}

} // namespace accloud::usecases::printing
