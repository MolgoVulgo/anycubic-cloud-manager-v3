#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

struct LoadCloudFilesResult {
    bool ok{false};
    std::string message;
    std::vector<accloud::cloud::CloudFileInfo> files;
};

class LoadCloudFilesUseCase {
public:
    LoadCloudFilesResult execute(int page, int limit) const;
};

} // namespace accloud::usecases::cloud
