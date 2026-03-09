#include "DownloadsApi.h"
#include "CloudLegacyImpl.h"

namespace accloud::cloud::api {

CloudDownloadResult DownloadsApi::getSignedUrl(const std::string& accessToken,
                                               const std::string& xxToken,
                                               const std::string& fileId) const {
    return legacy::legacyGetCloudDownloadUrl(accessToken, xxToken, fileId);
}

} // namespace accloud::cloud::api
