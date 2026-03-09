#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class SendPrintOrderUseCase {
public:
    accloud::cloud::CloudPrintOrderResult execute(const std::string& printerId,
                                                  const std::string& fileId,
                                                  bool deleteAfterPrint) const;
};

} // namespace accloud::usecases::cloud
