#include "ProjectsApi.h"
#include "CloudLegacyImpl.h"

namespace accloud::cloud::api {

CloudPrinterProjectsResult ProjectsApi::listByPrinter(const std::string& accessToken,
                                                      const std::string& xxToken,
                                                      const std::string& printerId,
                                                      int page,
                                                      int limit) const {
    return legacy::legacyFetchPrinterProjects(accessToken, xxToken, printerId, page, limit);
}

} // namespace accloud::cloud::api
