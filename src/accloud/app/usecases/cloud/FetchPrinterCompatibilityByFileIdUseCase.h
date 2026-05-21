#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class FetchPrinterCompatibilityByFileIdUseCase {
public:
    accloud::cloud::CloudPrinterCompatResult execute(const std::string& fileId) const;
};

} // namespace accloud::usecases::cloud
