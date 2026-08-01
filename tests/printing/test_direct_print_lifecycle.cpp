#include "app/usecases/printing/DirectPrintLifecycleUseCase.h"
#include "domain/printing/DirectPrintOperation.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using accloud::printing::DirectPrintOperation;
using accloud::printing::DirectPrintState;
using accloud::printing::directPrintStateFromPersisted;
using accloud::printing::directPrintStateKey;
using accloud::usecases::printing::DirectPrintCompletionKind;
using accloud::usecases::printing::DirectPrintLifecycleContext;
using accloud::usecases::printing::DirectPrintLifecycleEffectKind;
using accloud::usecases::printing::DirectPrintLocalDeleteDispatchResult;
using accloud::usecases::printing::DirectPrintProjectSnapshot;
using accloud::usecases::printing::beginDirectPrintCloudDelete;
using accloud::usecases::printing::beginDirectPrintLocalDelete;
using accloud::usecases::printing::findMatchingDirectPrintProject;
using accloud::usecases::printing::handleDirectPrintCloudDeleteResult;
using accloud::usecases::printing::handleDirectPrintLocalDeleteConfirmation;
using accloud::usecases::printing::handleDirectPrintLocalDeleteDispatch;
using accloud::usecases::printing::reconcileDirectPrintOperation;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

DirectPrintOperation makeOperation() {
    DirectPrintOperation operation;
    operation.printerId = "printer-1";
    operation.cloudFileId = "cloud-1";
    operation.cloudGcodeId = "gcode-1";
    operation.cloudFileName = "cube.pwsz";
    operation.cloudFileSize = 117557;
    operation.printTaskId = "task-1";
    operation.printMsgId = "msg-1";
    operation.deleteAfterSuccess = true;
    operation.deleteLocalOnFailure = false;
    operation.state = DirectPrintState::PrintCommandSent;
    operation.createdAt = 1785448826;
    return operation;
}

DirectPrintProjectSnapshot makeProject(int status) {
    DirectPrintProjectSnapshot project;
    project.printerId = "printer-1";
    project.taskId = "task-1";
    project.cloudFileId = "cloud-1";
    project.currentFile = "cube.pwsz";
    project.printStatus = status;
    return project;
}

bool hasEffect(const accloud::usecases::printing::DirectPrintLifecycleTransition& transition,
               DirectPrintLifecycleEffectKind kind,
               DirectPrintCompletionKind completionKind = DirectPrintCompletionKind::None) {
    for (const auto& effect : transition.effects) {
        if (effect.kind == kind && effect.completionKind == completionKind) {
            return true;
        }
    }
    return false;
}

void requireOnlyEffect(const accloud::usecases::printing::DirectPrintLifecycleTransition& transition,
                       DirectPrintLifecycleEffectKind kind) {
    require(transition.effects.size() == 1, "transition should contain exactly one effect");
    require(transition.effects.front().kind == kind, "transition contains an unexpected effect");
}

void testStatePersistenceKeysStayCompatible() {
    const std::vector<DirectPrintState> states{
        DirectPrintState::Uploaded,
        DirectPrintState::PrintCommandSent,
        DirectPrintState::Printing,
        DirectPrintState::SuccessLocalDeletePending,
        DirectPrintState::FailureLocalDeletePending,
        DirectPrintState::SuccessLocalCleanupError,
        DirectPrintState::FailedLocalCleanupError,
        DirectPrintState::CloudDeletePending,
        DirectPrintState::CloudDeleteError,
    };
    for (const auto state : states) {
        require(directPrintStateFromPersisted(directPrintStateKey(state)) == state,
                "direct-print persisted state key must round-trip");
    }
    require(directPrintStateFromPersisted("legacy-or-invalid") == DirectPrintState::Unknown,
            "unknown persisted state must remain explicit");
}

void testTaskIdMatchWinsOverCloudFallback() {
    auto operation = makeOperation();
    auto fallback = makeProject(2);
    fallback.taskId = "other-task";
    fallback.currentFile = "fallback.pwsz";
    auto exact = makeProject(1);
    exact.cloudFileId = "other-cloud";
    exact.currentFile = "exact.pwsz";

    const std::vector<DirectPrintProjectSnapshot> projects{fallback, exact};
    const auto match = findMatchingDirectPrintProject(operation, projects);
    require(match.has_value() && *match == 1, "taskId must win over cloudFileId fallback");
}

void testCloudFallbackUsesMatchingPrinterOnly() {
    auto operation = makeOperation();
    operation.printTaskId.clear();
    auto wrongPrinter = makeProject(2);
    wrongPrinter.printerId = "printer-2";
    auto fallback = makeProject(1);
    fallback.taskId.clear();

    const std::vector<DirectPrintProjectSnapshot> projects{wrongPrinter, fallback};
    const auto match = findMatchingDirectPrintProject(operation, projects);
    require(match.has_value() && *match == 1,
            "cloudFileId fallback must stay scoped to the direct-print printer");
}

void testActiveProjectPersistsPrintingState() {
    const auto transition = reconcileDirectPrintOperation(makeOperation(), {makeProject(1)});
    require(transition.matchedProject, "active direct-print project should match");
    require(transition.operation.observedActive, "active project must mark the operation observed");
    require(transition.operation.state == DirectPrintState::Printing,
            "active project must move the operation to PRINTING");
    require(transition.operation.printerLocalFilename == "cube.pwsz",
            "active project must capture the exact printer-local filename");
    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
}

void testTerminalStatusBeforeActiveObservationDoesNothing() {
    const auto transition = reconcileDirectPrintOperation(makeOperation(), {makeProject(2)});
    require(transition.matchedProject, "terminal project should still correlate");
    require(transition.effects.empty(),
            "terminal status must not trigger cleanup before PRINTING was observed");
    require(transition.operation.state == DirectPrintState::PrintCommandSent,
            "unconfirmed terminal status must not advance the lifecycle");
}

void testSuccessWithoutCleanupRemovesPendingOperation() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::Printing;
    operation.deleteAfterSuccess = false;

    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(2)});
    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
}

void testSuccessRequestsLocalDeleteBeforeCloudDelete() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::Printing;

    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(2)});
    require(transition.operation.state == DirectPrintState::SuccessLocalDeletePending,
            "successful direct print must enter local-delete pending first");
    require(transition.effects.size() == 2, "local cleanup start must persist then request deletion");
    require(transition.effects[0].kind == DirectPrintLifecycleEffectKind::PersistOperation,
            "local cleanup state must be persisted before the command");
    require(transition.effects[1].kind == DirectPrintLifecycleEffectKind::RequestLocalDelete,
            "local deletion must be requested after persistence");
    require(transition.effects[1].completionKind == DirectPrintCompletionKind::Success,
            "success cleanup request must carry the success completion kind");
    require(!hasEffect(transition, DirectPrintLifecycleEffectKind::RequestCloudDelete),
            "cloud deletion must not start before local deletion succeeds");
}

void testLocalDeleteInFlightPreventsDuplicateRequest() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::SuccessLocalDeletePending;

    DirectPrintLifecycleContext context;
    context.localDeleteInFlight = true;
    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(2)}, context);
    require(transition.effects.empty(), "in-flight local delete must not be duplicated");
}

void testRestartedPendingLocalDeleteIsRecoverable() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::SuccessLocalDeletePending;

    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(2)});
    require(hasEffect(transition, DirectPrintLifecycleEffectKind::RequestLocalDelete,
                      DirectPrintCompletionKind::Success),
            "pending local delete must be reissued when no request is in flight after restart");
}

void testSuccessLocalDeleteConfirmationStartsCloudDelete() {
    auto operation = makeOperation();
    operation.state = DirectPrintState::SuccessLocalDeletePending;
    const auto transition = handleDirectPrintLocalDeleteConfirmation(
        operation, DirectPrintCompletionKind::Success, true);

    require(transition.operation.state == DirectPrintState::CloudDeletePending,
            "confirmed local deletion must advance to cloud-delete pending");
    require(transition.effects.size() == 2, "cloud cleanup start must persist then request deletion");
    require(transition.effects[0].kind == DirectPrintLifecycleEffectKind::PersistOperation,
            "cloud-delete pending state must be persisted before deletion");
    require(transition.effects[1].kind == DirectPrintLifecycleEffectKind::RequestCloudDelete,
            "cloud deletion must follow confirmed local deletion");
}

void testLocalDeleteDispatchFailureKeepsCloud() {
    auto operation = makeOperation();
    operation.state = DirectPrintState::SuccessLocalDeletePending;
    const auto transition = handleDirectPrintLocalDeleteDispatch(
        operation,
        DirectPrintCompletionKind::Success,
        DirectPrintLocalDeleteDispatchResult{false, false});

    require(transition.operation.state == DirectPrintState::SuccessLocalCleanupError,
            "failed local-delete dispatch must persist success cleanup error");
    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
    require(!hasEffect(transition, DirectPrintLifecycleEffectKind::RequestCloudDelete),
            "cloud file must be kept when local deletion cannot start");
}

void testLocalDeleteMqttFailureKeepsCloud() {
    auto operation = makeOperation();
    operation.state = DirectPrintState::SuccessLocalDeletePending;
    const auto transition = handleDirectPrintLocalDeleteConfirmation(
        operation, DirectPrintCompletionKind::Success, false);

    require(transition.operation.state == DirectPrintState::SuccessLocalCleanupError,
            "unconfirmed local deletion must become a cleanup error");
    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
}

void testCloudDeleteFailureIsPersistedForRetry() {
    auto operation = makeOperation();
    operation.state = DirectPrintState::CloudDeletePending;
    const auto transition = handleDirectPrintCloudDeleteResult(operation, false);

    require(transition.operation.state == DirectPrintState::CloudDeleteError,
            "failed cloud deletion must remain recoverable");
    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation);
}

void testCloudDeleteSuccessCompletesOperation() {
    auto operation = makeOperation();
    operation.state = DirectPrintState::CloudDeletePending;
    const auto transition = handleDirectPrintCloudDeleteResult(operation, true);
    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
}

void testCloudDeleteErrorRetriesAfterRestart() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::CloudDeleteError;
    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(2)});

    require(transition.operation.state == DirectPrintState::CloudDeletePending,
            "cloud-delete error must return to pending before retry");
    require(hasEffect(transition, DirectPrintLifecycleEffectKind::RequestCloudDelete),
            "cloud-delete error must retry when no request is in flight");
}

void testCloudDeleteInFlightPreventsDuplicateRetry() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::CloudDeletePending;
    DirectPrintLifecycleContext context;
    context.cloudDeleteInFlight = true;

    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(2)}, context);
    require(transition.effects.empty(), "in-flight cloud delete must not be duplicated");
}

void testFailedPrintKeepsBothFilesByDefault() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::Printing;
    operation.deleteLocalOnFailure = false;

    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(3)});
    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
    require(!hasEffect(transition, DirectPrintLifecycleEffectKind::RequestLocalDelete),
            "failed direct print must keep printer-local file by default");
    require(!hasEffect(transition, DirectPrintLifecycleEffectKind::RequestCloudDelete),
            "failed direct print must always keep cloud file");
}

void testFailedPrintCanDeleteOnlyPrinterLocalCopy() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::Printing;
    operation.deleteLocalOnFailure = true;
    operation.printerLocalFilename = "cube.pwsz";

    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(4)});
    require(transition.operation.state == DirectPrintState::FailureLocalDeletePending,
            "failed direct print with explicit preference must request local cleanup");
    require(hasEffect(transition, DirectPrintLifecycleEffectKind::RequestLocalDelete,
                      DirectPrintCompletionKind::Failure),
            "failed direct print must request only printer-local deletion");
    require(!hasEffect(transition, DirectPrintLifecycleEffectKind::RequestCloudDelete),
            "failed direct print must never delete cloud file");
}

void testFailedPrintWithoutLocalFilenameKeepsBothFiles() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::Printing;
    operation.deleteLocalOnFailure = true;
    operation.printerLocalFilename.clear();

    auto project = makeProject(4);
    project.currentFile.clear();
    project.gcodeName.clear();

    const auto transition = reconcileDirectPrintOperation(operation, {project});
    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
    require(!hasEffect(transition, DirectPrintLifecycleEffectKind::RequestLocalDelete),
            "failure cleanup without a printer-local filename must not issue local deletion");
    require(!hasEffect(transition, DirectPrintLifecycleEffectKind::RequestCloudDelete),
            "failure cleanup without a printer-local filename must keep the cloud file");
}

void testFailedLocalDeleteSuccessCompletesWithoutCloudDelete() {
    auto operation = makeOperation();
    operation.state = DirectPrintState::FailureLocalDeletePending;
    const auto transition = handleDirectPrintLocalDeleteConfirmation(
        operation, DirectPrintCompletionKind::Failure, true);

    requireOnlyEffect(transition, DirectPrintLifecycleEffectKind::RemoveOperation);
    require(!hasEffect(transition, DirectPrintLifecycleEffectKind::RequestCloudDelete),
            "failure cleanup completion must keep cloud file");
}

void testCleanupErrorsDoNotRetryImplicitly() {
    auto operation = makeOperation();
    operation.observedActive = true;
    operation.state = DirectPrintState::SuccessLocalCleanupError;
    const auto transition = reconcileDirectPrintOperation(operation, {makeProject(2)});
    require(transition.effects.empty(), "cleanup errors must require explicit recovery policy");
}

void testExplicitBeginLocalDeleteUsesTypedCompletion() {
    auto operation = makeOperation();
    const auto transition = beginDirectPrintLocalDelete(
        operation, DirectPrintCompletionKind::Failure, false);
    require(transition.operation.state == DirectPrintState::FailureLocalDeletePending,
            "explicit local cleanup begin must use failure pending state");
    require(hasEffect(transition, DirectPrintLifecycleEffectKind::PersistOperation),
            "explicit local cleanup begin must persist before dispatch");
    require(hasEffect(transition, DirectPrintLifecycleEffectKind::RequestLocalDelete,
                      DirectPrintCompletionKind::Failure),
            "explicit local cleanup begin must preserve typed completion kind");
}

void testMissingCloudFileIdDoesNotIssueDelete() {
    auto operation = makeOperation();
    operation.cloudFileId.clear();
    operation.state = DirectPrintState::CloudDeleteError;
    const auto transition = beginDirectPrintCloudDelete(operation);
    require(transition.effects.empty(), "cloud delete requires a stable cloud file id");
}

} // namespace

int main() {
    testStatePersistenceKeysStayCompatible();
    testTaskIdMatchWinsOverCloudFallback();
    testCloudFallbackUsesMatchingPrinterOnly();
    testActiveProjectPersistsPrintingState();
    testTerminalStatusBeforeActiveObservationDoesNothing();
    testSuccessWithoutCleanupRemovesPendingOperation();
    testSuccessRequestsLocalDeleteBeforeCloudDelete();
    testLocalDeleteInFlightPreventsDuplicateRequest();
    testRestartedPendingLocalDeleteIsRecoverable();
    testSuccessLocalDeleteConfirmationStartsCloudDelete();
    testLocalDeleteDispatchFailureKeepsCloud();
    testLocalDeleteMqttFailureKeepsCloud();
    testCloudDeleteFailureIsPersistedForRetry();
    testCloudDeleteSuccessCompletesOperation();
    testCloudDeleteErrorRetriesAfterRestart();
    testCloudDeleteInFlightPreventsDuplicateRetry();
    testFailedPrintKeepsBothFilesByDefault();
    testFailedPrintCanDeleteOnlyPrinterLocalCopy();
    testFailedPrintWithoutLocalFilenameKeepsBothFiles();
    testFailedLocalDeleteSuccessCompletesWithoutCloudDelete();
    testCleanupErrorsDoNotRetryImplicitly();
    testExplicitBeginLocalDeleteUsesTypedCompletion();
    testMissingCloudFileIdDoesNotIssueDelete();

    std::cout << "direct print lifecycle tests passed\n";
    return 0;
}
