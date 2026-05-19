#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class FetchPrinterDetailsUseCase {
public:
    accloud::cloud::CloudPrinterDetailsResult execute(const std::string& printerId) const;
};

} // namespace accloud::usecases::cloud
