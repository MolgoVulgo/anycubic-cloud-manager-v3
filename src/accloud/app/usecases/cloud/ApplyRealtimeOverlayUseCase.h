#pragma once

#include "infra/cloud/CloudClient.h"

#include <vector>

namespace accloud::usecases::cloud {

class ApplyRealtimeOverlayUseCase {
public:
    std::vector<accloud::cloud::CloudPrinterInfo> execute(
        std::vector<accloud::cloud::CloudPrinterInfo> printers) const;
};

} // namespace accloud::usecases::cloud

