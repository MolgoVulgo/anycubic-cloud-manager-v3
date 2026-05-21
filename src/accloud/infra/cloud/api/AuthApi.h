#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api {

class AuthApi {
public:
    CloudCheckResult checkAuth(const std::string& accessToken,
                               const std::string& xxToken = {}) const;
};

} // namespace accloud::cloud::api
