#pragma once

#include <functional>
#include <string>

namespace accloud::usecases::cloud {

struct UploadLocalFileResult {
    bool ok{false};
    std::string message;
    std::string fileId;
    std::string gcodeId;
    int uploadStatus{0};
    bool unlockOk{false};
};

class UploadLocalFileUseCase {
public:
    using ProgressCallback = std::function<void(double progress, const std::string& phase)>;

    UploadLocalFileResult execute(const std::string& localPath,
                                  const ProgressCallback& onProgress = {}) const;
};

} // namespace accloud::usecases::cloud
