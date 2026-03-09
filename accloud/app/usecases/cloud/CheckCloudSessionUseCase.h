#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class CheckCloudSessionUseCase {
public:
    accloud::cloud::CloudCheckResult execute() const;
};

} // namespace accloud::usecases::cloud
