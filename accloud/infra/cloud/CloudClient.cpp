#include "CloudClient.h"

#include "infra/cloud/api/AuthApi.h"
#include "infra/cloud/api/DownloadsApi.h"
#include "infra/cloud/api/FilesApi.h"
#include "infra/cloud/api/PrintOrderApi.h"
#include "infra/cloud/api/PrintersApi.h"
#include "infra/cloud/api/ProjectsApi.h"
#include "infra/cloud/api/QuotaApi.h"
#include "infra/cloud/api/ReasonCatalogApi.h"

namespace accloud::cloud {

CloudCheckResult checkCloudAuth(const std::string& accessToken,
                                const std::string& xxToken) {
    return api::AuthApi{}.checkAuth(accessToken, xxToken);
}

CloudFilesResult fetchCloudFiles(const std::string& accessToken,
                                 const std::string& xxToken,
                                 int page, int limit) {
    return api::FilesApi{}.list(accessToken, xxToken, page, limit);
}

CloudQuotaResult fetchCloudQuota(const std::string& accessToken,
                                 const std::string& xxToken) {
    return api::QuotaApi{}.fetch(accessToken, xxToken);
}

CloudOpResult deleteCloudFile(const std::string& accessToken,
                              const std::string& xxToken,
                              const std::string& fileId) {
    return api::FilesApi{}.remove(accessToken, xxToken, fileId);
}

CloudDownloadResult getCloudDownloadUrl(const std::string& accessToken,
                                        const std::string& xxToken,
                                        const std::string& fileId) {
    return api::DownloadsApi{}.getSignedUrl(accessToken, xxToken, fileId);
}

CloudPrintersResult fetchCloudPrinters(const std::string& accessToken,
                                       const std::string& xxToken) {
    return api::PrintersApi{}.list(accessToken, xxToken);
}

CloudPrinterCompatResult fetchPrinterCompatibilityByExt(const std::string& accessToken,
                                                        const std::string& xxToken,
                                                        const std::string& fileExt) {
    return api::PrintersApi{}.compatibilityByExt(accessToken, xxToken, fileExt);
}

CloudPrinterCompatResult fetchPrinterCompatibilityByFileId(const std::string& accessToken,
                                                           const std::string& xxToken,
                                                           const std::string& fileId) {
    return api::PrintersApi{}.compatibilityByFileId(accessToken, xxToken, fileId);
}

CloudPrinterDetailsResult fetchPrinterDetails(const std::string& accessToken,
                                              const std::string& xxToken,
                                              const std::string& printerId) {
    return api::PrintersApi{}.details(accessToken, xxToken, printerId);
}

CloudReasonCatalogResult fetchReasonCatalog(const std::string& accessToken,
                                            const std::string& xxToken) {
    return api::ReasonCatalogApi{}.list(accessToken, xxToken);
}

CloudPrinterProjectsResult fetchPrinterProjects(const std::string& accessToken,
                                                const std::string& xxToken,
                                                const std::string& printerId,
                                                int page,
                                                int limit) {
    return api::ProjectsApi{}.listByPrinter(accessToken, xxToken, printerId, page, limit);
}

CloudPrintOrderResult sendCloudPrintOrder(const std::string& accessToken,
                                          const std::string& xxToken,
                                          const std::string& printerId,
                                          const std::string& fileId,
                                          bool deleteAfterPrint) {
    return api::PrintOrderApi{}.send(accessToken, xxToken, printerId, fileId, deleteAfterPrint);
}

} // namespace accloud::cloud
