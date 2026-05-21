#include "FilesApi.h"
#include "CloudLegacyImpl.h"

namespace accloud::cloud::api {

CloudFilesResult FilesApi::list(const std::string& accessToken,
                                const std::string& xxToken,
                                int page,
                                int limit) const {
    return legacy::legacyFetchCloudFiles(accessToken, xxToken, page, limit);
}

CloudOpResult FilesApi::remove(const std::string& accessToken,
                               const std::string& xxToken,
                               const std::string& fileId) const {
    return legacy::legacyDeleteCloudFile(accessToken, xxToken, fileId);
}

} // namespace accloud::cloud::api
