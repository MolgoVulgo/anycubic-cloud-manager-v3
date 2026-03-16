#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api {

class FilesApi {
public:
    CloudFilesResult list(const std::string& accessToken,
                          const std::string& xxToken,
                          int page = 1,
                          int limit = 20) const;

    CloudOpResult remove(const std::string& accessToken,
                         const std::string& xxToken,
                         const std::string& fileId) const;
};

} // namespace accloud::cloud::api
