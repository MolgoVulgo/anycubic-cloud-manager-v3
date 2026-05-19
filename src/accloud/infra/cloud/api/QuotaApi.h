#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api {

class QuotaApi {
public:
    CloudQuotaResult fetch(const std::string& accessToken,
                           const std::string& xxToken) const;
};

} // namespace accloud::cloud::api
