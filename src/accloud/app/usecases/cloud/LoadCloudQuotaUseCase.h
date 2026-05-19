#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class LoadCloudQuotaUseCase {
public:
    accloud::cloud::CloudQuotaResult execute() const;
};

} // namespace accloud::usecases::cloud
