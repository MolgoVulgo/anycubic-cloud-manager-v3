#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api {

class ProjectsApi {
public:
    CloudPrinterProjectsResult listByPrinter(const std::string& accessToken,
                                             const std::string& xxToken,
                                             const std::string& printerId,
                                             int page = 1,
                                             int limit = 10) const;
};

} // namespace accloud::cloud::api
