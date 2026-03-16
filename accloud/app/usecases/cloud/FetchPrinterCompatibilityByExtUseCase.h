#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class FetchPrinterCompatibilityByExtUseCase {
public:
    accloud::cloud::CloudPrinterCompatResult execute(const std::string& fileExt) const;
};

} // namespace accloud::usecases::cloud
