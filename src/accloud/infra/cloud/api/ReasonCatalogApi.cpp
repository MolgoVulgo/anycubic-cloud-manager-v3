#include "ReasonCatalogApi.h"
#include "CloudLegacyImpl.h"

namespace accloud::cloud::api {

CloudReasonCatalogResult ReasonCatalogApi::list(const std::string& accessToken,
                                                const std::string& xxToken) const {
    return legacy::legacyFetchReasonCatalog(accessToken, xxToken);
}

} // namespace accloud::cloud::api
