#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace accloud::usecases::cloud {

struct CloudPwszThumbnailValidationResult {
    bool valid{false};
    bool tooSmall{false};
    bool cancelled{false};
    std::string message;
};

struct CloudPwszReplacementActionResult {
    bool ok{false};
    std::string message;
};

struct CloudPwszReplacementStatusResult {
    bool ok{false};
    int status{0};
    std::string gcodeId;
};

struct CloudPwszReplacementFile {
    std::string fileId;
    std::string thumbnailUrl;
    std::vector<std::string> thumbnailCandidates;
};

struct CloudPwszReplacementListResult {
    bool ok{false};
    std::vector<CloudPwszReplacementFile> files;
};

struct CloudPwszReplacementOperations {
    std::function<CloudPwszReplacementActionResult()> unlockRegistered;
    std::function<CloudPwszReplacementStatusResult()> getUploadStatus;
    std::function<CloudPwszReplacementListResult()> listFiles;
    std::function<CloudPwszThumbnailValidationResult(const std::string&)> validateThumbnail;
    std::function<CloudPwszReplacementActionResult(const std::string&)> removeFile;
    std::function<bool(std::chrono::milliseconds)> wait;
    std::function<bool()> shouldCancel;
};

struct CloudPwszReplacementOutcome {
    std::string status; // modified | failed | partial
    std::string message;
    bool cancelled{false};
};

[[nodiscard]] CloudPwszReplacementOutcome finalizeRegisteredCloudPwszReplacement(
    const std::string& originalFileId,
    const std::string& newFileId,
    const CloudPwszReplacementOperations& operations);

} // namespace accloud::usecases::cloud
