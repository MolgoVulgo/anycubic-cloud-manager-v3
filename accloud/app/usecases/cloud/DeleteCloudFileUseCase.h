#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class DeleteCloudFileUseCase {
public:
    accloud::cloud::CloudOpResult execute(const std::string& fileId) const;
};

} // namespace accloud::usecases::cloud
