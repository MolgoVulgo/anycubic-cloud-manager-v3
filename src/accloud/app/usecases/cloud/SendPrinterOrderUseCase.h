#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class SendPrinterOrderUseCase {
public:
    accloud::cloud::CloudPrintOrderResult execute(const std::string& printerId,
                                                  int orderId,
                                                  const std::string& projectId,
                                                  const std::string& dataJson) const;
};

} // namespace accloud::usecases::cloud
