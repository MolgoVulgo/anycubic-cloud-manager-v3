#include "AuthApi.h"
#include "CloudLegacyImpl.h"

namespace accloud::cloud::api {

CloudCheckResult AuthApi::checkAuth(const std::string& accessToken,
                                    const std::string& xxToken) const {
    return legacy::legacyCheckCloudAuth(accessToken, xxToken);
}

} // namespace accloud::cloud::api
