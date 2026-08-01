#include "app/usecases/cloud/DeleteCloudFilesUseCase.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using accloud::usecases::cloud::CloudFileDeleteItem;
using accloud::usecases::cloud::DeleteCloudFilesUseCase;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testSequentialOrderAndSummary() {
    DeleteCloudFilesUseCase useCase;
    require(useCase.start({{"oldest", "a.pwsz"}, {"middle", "b.pwsz"}, {"newest", "c.pwsz"}}),
            "batch must start with valid files");
    require(useCase.running(), "batch must be running after start");
    require(useCase.current().has_value() && useCase.current()->fileId == "oldest",
            "batch must preserve input order");

    require(useCase.handleResult("oldest", true), "first result must be accepted");
    require(useCase.current().has_value() && useCase.current()->fileId == "middle",
            "second file must follow the first result");
    require(useCase.handleResult("middle", false, "backend rejected"),
            "second result must be accepted");
    require(useCase.current().has_value() && useCase.current()->fileId == "newest",
            "third file must follow the second result");
    require(useCase.handleResult("newest", true), "third result must be accepted");
    require(!useCase.running(), "batch must stop after the final result");

    const auto summary = useCase.summary();
    require(summary.requested == 3, "summary must retain requested count");
    require(summary.completed == 3, "summary must retain completed count");
    require(summary.succeeded == 2, "summary must retain success count");
    require(summary.failures.size() == 1, "summary must retain failure details");
    require(summary.failures[0].fileId == "middle", "failure must retain file id");
    require(summary.failures[0].message == "backend rejected", "failure must retain backend detail");
}

void testStaleResultIsIgnored() {
    DeleteCloudFilesUseCase useCase;
    require(useCase.start({{"a", "a.pwsz"}, {"b", "b.pwsz"}}), "batch must start");
    require(!useCase.handleResult("b", true), "out-of-order completion must be ignored");
    require(useCase.completed() == 0, "stale completion must not advance progress");
    require(useCase.current().has_value() && useCase.current()->fileId == "a",
            "stale completion must keep current file unchanged");
}

void testEmptyIdsAreFiltered() {
    DeleteCloudFilesUseCase useCase;
    require(useCase.start({{"", "invalid"}, {"valid", "valid.pwsz"}}),
            "batch must accept remaining valid files");
    require(useCase.total() == 1, "empty file ids must not enter the queue");
    require(useCase.current().has_value() && useCase.current()->fileId == "valid",
            "valid file must become current");

    DeleteCloudFilesUseCase onlyInvalid;
    require(!onlyInvalid.start({{"", "invalid"}}), "all-invalid batch must be rejected");
    require(!onlyInvalid.running(), "rejected batch must remain idle");
}

void testCancelStopsAfterCurrentCompletion() {
    DeleteCloudFilesUseCase useCase;
    require(useCase.start({{"a", "a.pwsz"}, {"b", "b.pwsz"}}), "batch must start");
    useCase.cancel();
    require(useCase.cancelRequested(), "cancel must be remembered while current delete is in flight");
    require(useCase.handleResult("a", true), "current completion must still be accepted after cancel");
    require(!useCase.running(), "cancelled batch must not dispatch the next file");
    require(!useCase.current().has_value(), "cancelled batch must have no current file");
    const auto summary = useCase.summary();
    require(summary.cancelled, "summary must expose cancellation");
    require(summary.completed == 1 && summary.requested == 2,
            "cancelled summary must distinguish completed from requested files");
}

void testRestartAfterCompletion() {
    DeleteCloudFilesUseCase useCase;
    require(useCase.start({{"a", "a.pwsz"}}), "first batch must start");
    require(useCase.handleResult("a", true), "first batch must complete");
    require(useCase.start({{"b", "b.pwsz"}}), "completed use case must accept a new batch");
    require(useCase.current().has_value() && useCase.current()->fileId == "b",
            "new batch must not retain old state");
    require(useCase.completed() == 0 && useCase.succeeded() == 0,
            "new batch counters must reset");
}

} // namespace

int main() {
    testSequentialOrderAndSummary();
    testStaleResultIsIgnored();
    testEmptyIdsAreFiltered();
    testCancelStopsAfterCurrentCompletion();
    testRestartAfterCompletion();
    std::cout << "Delete cloud files lifecycle tests passed\n";
    return 0;
}
