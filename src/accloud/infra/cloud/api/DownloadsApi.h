#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api {

class DownloadsApi {
public:
    CloudDownloadResult getSignedUrl(const std::string& accessToken,
                                     const std::string& xxToken,
                                     const std::string& fileId) const;
};

} // namespace accloud::cloud::api
