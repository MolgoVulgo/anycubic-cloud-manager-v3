#include "app/usecases/cloud/LoadCloudFilesUseCase.h"
#include "app/usecases/cloud/UpdateCloudPwszPreviewsUseCase.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using accloud::cloud::CloudFileInfo;
using accloud::usecases::cloud::CloudPwszPreviewUpdateItem;
using accloud::usecases::cloud::CloudPwszReplacementActionResult;
using accloud::usecases::cloud::CloudPwszReplacementFile;
using accloud::usecases::cloud::CloudPwszReplacementListResult;
using accloud::usecases::cloud::CloudPwszReplacementOperations;
using accloud::usecases::cloud::CloudPwszReplacementStatusResult;
using accloud::usecases::cloud::CloudPwszThumbnailValidationResult;
using accloud::usecases::cloud::LoadCloudFilesResult;
using accloud::usecases::cloud::collectAllCloudFilePages;
using accloud::usecases::cloud::finalizeRegisteredCloudPwszReplacement;
using accloud::usecases::cloud::orderCloudPwszPreviewUpdateItemsOldestFirst;
using accloud::usecases::cloud::detail::waitInterruptibly;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void requireId(const std::vector<CloudPwszPreviewUpdateItem>& items,
               std::size_t index,
               const std::string& expected) {
    require(index < items.size(), "ordered item index out of range");
    require(items[index].fileId == expected, "unexpected cloud update order");
}

CloudFileInfo makeCloudFile(std::string id, std::string name) {
    CloudFileInfo file{};
    file.id = std::move(id);
    file.name = std::move(name);
    return file;
}

CloudPwszReplacementListResult readyReplacementList() {
    CloudPwszReplacementFile file;
    file.fileId = "new";
    file.thumbnailCandidates = {"thumbnail://new"};
    return {true, {std::move(file)}};
}

CloudPwszReplacementOperations successfulReplacementOperations(
    std::vector<std::string>& calls) {
    CloudPwszReplacementOperations operations;
    operations.unlockRegistered = [&]() {
        calls.push_back("unlock");
        return CloudPwszReplacementActionResult{true, {}};
    };
    operations.getUploadStatus = [&]() {
        calls.push_back("status");
        return CloudPwszReplacementStatusResult{true, 1, {}};
    };
    operations.listFiles = [&]() {
        calls.push_back("list");
        return readyReplacementList();
    };
    operations.validateThumbnail = [&](const std::string& url) {
        calls.push_back("validate:" + url);
        return CloudPwszThumbnailValidationResult{true, false, false, {}};
    };
    operations.removeFile = [&](const std::string& fileId) {
        calls.push_back("remove:" + fileId);
        return CloudPwszReplacementActionResult{true, {}};
    };
    operations.wait = [&](std::chrono::milliseconds) {
        calls.push_back("wait");
        return true;
    };
    operations.shouldCancel = []() { return false; };
    return operations;
}

void testSuccessfulReplacementSequence() {
    std::vector<std::string> calls;
    const auto outcome = finalizeRegisteredCloudPwszReplacement(
        "old", "new", successfulReplacementOperations(calls));

    require(outcome.status == "modified", "successful replacement must be modified");
    require(!outcome.cancelled, "successful replacement must not be cancelled");
    const std::vector<std::string> expected{
        "unlock", "wait", "status", "list", "validate:thumbnail://new", "remove:old"};
    require(calls == expected, "replacement operations must preserve the destructive sequence");
}

void testUnlockFailurePreservesBothVersions() {
    std::vector<std::string> calls;
    auto operations = successfulReplacementOperations(calls);
    operations.unlockRegistered = [&]() {
        calls.push_back("unlock");
        return CloudPwszReplacementActionResult{false, "unlock failed"};
    };

    const auto outcome = finalizeRegisteredCloudPwszReplacement("old", "new", operations);
    require(outcome.status == "partial", "unlock failure must be partial");
    require(calls == std::vector<std::string>{"unlock"},
            "unlock failure must stop before polling or deletion");
}

void testUnreadyReplacementDeletesOnlyNewVersion() {
    std::vector<std::string> calls;
    auto operations = successfulReplacementOperations(calls);
    operations.getUploadStatus = [&]() {
        calls.push_back("status");
        return CloudPwszReplacementStatusResult{true, 2, "0"};
    };

    const auto outcome = finalizeRegisteredCloudPwszReplacement("old", "new", operations);
    require(outcome.status == "failed", "unready replacement with cleanup must fail safely");
    require(calls.back() == "remove:new", "unready replacement must clean up the new version");
    require(std::find(calls.begin(), calls.end(), "remove:old") == calls.end(),
            "unready replacement must preserve the original");
}

void testInvalidThumbnailCleanupFailurePreservesOriginal() {
    std::vector<std::string> calls;
    auto operations = successfulReplacementOperations(calls);
    operations.validateThumbnail = [&](const std::string& url) {
        calls.push_back("validate:" + url);
        return CloudPwszThumbnailValidationResult{false, false, false, "invalid"};
    };
    operations.removeFile = [&](const std::string& fileId) {
        calls.push_back("remove:" + fileId);
        return CloudPwszReplacementActionResult{false, "remove failed"};
    };

    const auto outcome = finalizeRegisteredCloudPwszReplacement("old", "new", operations);
    require(outcome.status == "partial", "failed cleanup of invalid thumbnail must be partial");
    require(calls.back() == "remove:new", "invalid thumbnail must attempt to remove the new version");
    require(std::find(calls.begin(), calls.end(), "remove:old") == calls.end(),
            "invalid thumbnail must never remove the original");
}

void testCancellationAfterUnlockPreservesBothVersions() {
    std::vector<std::string> calls;
    bool cancelled = false;
    auto operations = successfulReplacementOperations(calls);
    operations.unlockRegistered = [&]() {
        calls.push_back("unlock");
        cancelled = true;
        return CloudPwszReplacementActionResult{true, {}};
    };
    operations.shouldCancel = [&]() { return cancelled; };

    const auto outcome = finalizeRegisteredCloudPwszReplacement("old", "new", operations);
    require(outcome.status == "partial" && outcome.cancelled,
            "post-registration cancellation must be partial and cancelled");
    require(calls == std::vector<std::string>{"unlock"},
            "post-registration cancellation must preserve both versions without deletion");
}

void testOriginalRemovalFailureIsPartial() {
    std::vector<std::string> calls;
    auto operations = successfulReplacementOperations(calls);
    operations.removeFile = [&](const std::string& fileId) {
        calls.push_back("remove:" + fileId);
        return CloudPwszReplacementActionResult{fileId != "old", "remove failed"};
    };

    const auto outcome = finalizeRegisteredCloudPwszReplacement("old", "new", operations);
    require(outcome.status == "partial", "original removal failure must be partial");
    require(calls.back() == "remove:old", "original deletion must occur only after validation");
}

} // namespace

int main() {
    const std::vector<CloudPwszPreviewUpdateItem> input{
        {"50", "recent.pwsz", 10, 300},
        {"11", "old-second.pwsz", 10, 100},
        {"9", "old-first.pwsz", 10, 100},
        {"12", "middle.pwsz", 10, 200},
        {"unknown-b", "unknown-b.pwsz", 10, 0},
        {"unknown-a", "unknown-a.pwsz", 10, 0},
    };

    const auto ordered = orderCloudPwszPreviewUpdateItemsOldestFirst(input);
    require(ordered.size() == input.size(), "all update candidates must be preserved");
    requireId(ordered, 0, "9");
    requireId(ordered, 1, "11");
    requireId(ordered, 2, "12");
    requireId(ordered, 3, "50");
    requireId(ordered, 4, "unknown-a");
    requireId(ordered, 5, "unknown-b");

    int cancellationChecks = 0;
    const bool completedWait = waitInterruptibly(
        std::chrono::milliseconds(100),
        [&]() { return ++cancellationChecks >= 3; },
        std::chrono::milliseconds(1));
    require(!completedWait, "interruptible wait must stop after cancellation");
    require(cancellationChecks >= 3, "cancellation callback must be polled");

    const bool zeroDelayCompleted = waitInterruptibly(
        std::chrono::milliseconds(0),
        []() { return false; });
    require(zeroDelayCompleted, "zero delay must complete when not cancelled");

    const auto allPages = collectAllCloudFilePages(
        [](int page, int) {
            LoadCloudFilesResult result;
            result.ok = true;
            if (page == 1) {
                result.files = {makeCloudFile("1", "a.pwsz"),
                                makeCloudFile("2", "b.pwsz")};
            } else if (page == 2) {
                result.files = {makeCloudFile("3", "c.pwsz")};
            }
            return result;
        },
        2,
        10);
    require(allPages.ok && allPages.complete, "all cloud pages must be collected");
    require(allPages.files.size() == 3, "all cloud files must be preserved");
    require(allPages.pagesLoaded == 3, "pagination must stop only after an empty page");

    const auto repeatedPage = collectAllCloudFilePages(
        [](int, int) {
            LoadCloudFilesResult result;
            result.ok = true;
            result.files = {makeCloudFile("1", "a.pwsz"),
                            makeCloudFile("2", "b.pwsz")};
            return result;
        },
        2,
        10);
    require(!repeatedPage.ok && !repeatedPage.complete,
            "repeated full pages must not be reported as a complete inventory");
    require(repeatedPage.files.size() == 2, "repeated pages must be deduplicated");

    require(accloud::usecases::cloud::phase::kDownload == "pwsz.update.download",
            "progress phases must use stable keys");
    require(accloud::usecases::cloud::phase::kValidateThumbnail
                == "pwsz.update.validate_thumbnail",
            "thumbnail validation phase key mismatch");

    testSuccessfulReplacementSequence();
    testUnlockFailurePreservesBothVersions();
    testUnreadyReplacementDeletesOnlyNewVersion();
    testInvalidThumbnailCleanupFailurePreservesOriginal();
    testCancellationAfterUnlockPreservesBothVersions();
    testOriginalRemovalFailureIsPartial();

    std::cout << "PWSZ cloud preview update ordering, pagination and replacement workflow tests passed\n";
    return 0;
}
