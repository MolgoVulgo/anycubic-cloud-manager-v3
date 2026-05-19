#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

struct LoadPrintersDashboardResult {
    bool ok{false};
    std::string message;
    std::string rawJson;
    std::vector<accloud::cloud::CloudPrinterInfo> printers;
};

class LoadPrintersDashboardUseCase {
public:
    LoadPrintersDashboardResult execute() const;
};

} // namespace accloud::usecases::cloud
