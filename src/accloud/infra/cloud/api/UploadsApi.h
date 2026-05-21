#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace accloud::cloud {

struct CloudUploadLockResult {
    bool ok{false};
    std::string message;
    std::string lockId;
    std::string preSignUrl;
    std::string uploadUrl;
};

struct CloudUploadPutResult {
    bool ok{false};
    std::string message;
    int httpStatus{0};
};

struct CloudUploadRegisterResult {
    bool ok{false};
    std::string message;
    std::string fileId;
};

struct CloudUploadStatusResult {
    bool ok{false};
    std::string message;
    std::string fileId;
    int status{0};
    std::string gcodeId;
};

struct CloudUploadUnlockResult {
    bool ok{false};
    std::string message;
};

} // namespace accloud::cloud

namespace accloud::cloud::api {

class UploadsApi {
public:
    CloudUploadLockResult lockStorageSpace(const std::string& accessToken,
                                           const std::string& xxToken,
                                           const std::string& fileName,
                                           std::uint64_t sizeBytes,
                                           bool isTempFile = false) const;

    CloudUploadPutResult putPresigned(const std::string& preSignUrl,
                                      const std::filesystem::path& localFilePath) const;

    CloudUploadRegisterResult registerUploadedFile(const std::string& accessToken,
                                                   const std::string& xxToken,
                                                   const std::string& lockId) const;

    CloudUploadStatusResult getUploadStatus(const std::string& accessToken,
                                            const std::string& xxToken,
                                            const std::string& fileId) const;

    CloudUploadUnlockResult unlockStorageSpace(const std::string& accessToken,
                                               const std::string& xxToken,
                                               const std::string& lockId,
                                               bool deleteCosOnFailure) const;
};

} // namespace accloud::cloud::api
