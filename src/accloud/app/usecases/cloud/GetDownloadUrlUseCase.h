#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class GetDownloadUrlUseCase {
public:
    accloud::cloud::CloudDownloadResult execute(const std::string& fileId) const;
};

} // namespace accloud::usecases::cloud
