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
    bool previewAdded{false};
    bool localFileSynchronized{true};
    std::string recoveryPath;
};

class UploadLocalFileUseCase {
public:
    using ProgressCallback = std::function<void(double progress, const std::string& phase)>;

    UploadLocalFileResult execute(const std::string& localPath,
                                  bool completePwszPreview2 = false,
                                  const ProgressCallback& onProgress = {}) const;

    [[nodiscard]] static bool hasUsableGcodeId(const std::string& gcodeId);
    [[nodiscard]] static bool isUploadReady(int uploadStatus, const std::string& gcodeId);
};

} // namespace accloud::usecases::cloud
