#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class FetchPrinterProjectsUseCase {
public:
    accloud::cloud::CloudPrinterProjectsResult execute(const std::string& printerId,
                                                       int page = 1,
                                                       int limit = 10) const;
};

} // namespace accloud::usecases::cloud
