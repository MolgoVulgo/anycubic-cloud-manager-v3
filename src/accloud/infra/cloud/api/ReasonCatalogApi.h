#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api {

class ReasonCatalogApi {
public:
    CloudReasonCatalogResult list(const std::string& accessToken,
                                  const std::string& xxToken) const;
};

} // namespace accloud::cloud::api
