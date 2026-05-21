#include "QuotaApi.h"
#include "CloudLegacyImpl.h"

namespace accloud::cloud::api {

CloudQuotaResult QuotaApi::fetch(const std::string& accessToken,
                                 const std::string& xxToken) const {
    return legacy::legacyFetchCloudQuota(accessToken, xxToken);
}

} // namespace accloud::cloud::api
