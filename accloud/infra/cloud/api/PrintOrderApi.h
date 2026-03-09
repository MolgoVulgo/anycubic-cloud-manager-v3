#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api {

class PrintOrderApi {
public:
    CloudPrintOrderResult send(const std::string& accessToken,
                               const std::string& xxToken,
                               const std::string& printerId,
                               const std::string& fileId,
                               bool deleteAfterPrint) const;
};

} // namespace accloud::cloud::api
