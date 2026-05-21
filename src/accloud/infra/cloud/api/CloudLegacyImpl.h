#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api::legacy {

CloudCheckResult legacyCheckCloudAuth(const std::string& accessToken,
                                      const std::string& xxToken = {});

CloudFilesResult legacyFetchCloudFiles(const std::string& accessToken,
                                       const std::string& xxToken,
                                       int page = 1,
                                       int limit = 20);

CloudQuotaResult legacyFetchCloudQuota(const std::string& accessToken,
                                       const std::string& xxToken);

CloudOpResult legacyDeleteCloudFile(const std::string& accessToken,
                                    const std::string& xxToken,
                                    const std::string& fileId);

CloudDownloadResult legacyGetCloudDownloadUrl(const std::string& accessToken,
                                              const std::string& xxToken,
                                              const std::string& fileId);

CloudPrintersResult legacyFetchCloudPrinters(const std::string& accessToken,
                                             const std::string& xxToken);

CloudPrinterCompatResult legacyFetchPrinterCompatibilityByExt(const std::string& accessToken,
                                                              const std::string& xxToken,
                                                              const std::string& fileExt);

CloudPrinterCompatResult legacyFetchPrinterCompatibilityByFileId(const std::string& accessToken,
                                                                 const std::string& xxToken,
                                                                 const std::string& fileId);

CloudPrinterDetailsResult legacyFetchPrinterDetails(const std::string& accessToken,
                                                    const std::string& xxToken,
                                                    const std::string& printerId);

CloudReasonCatalogResult legacyFetchReasonCatalog(const std::string& accessToken,
                                                  const std::string& xxToken);

CloudPrinterProjectsResult legacyFetchPrinterProjects(const std::string& accessToken,
                                                      const std::string& xxToken,
                                                      const std::string& printerId,
                                                      int page = 1,
                                                      int limit = 10);

CloudPrintOrderResult legacySendCloudPrintOrder(const std::string& accessToken,
                                                const std::string& xxToken,
                                                const std::string& printerId,
                                                const std::string& fileId,
                                                bool deleteAfterPrint);

CloudPrintOrderResult legacySendCloudPrinterOrder(const std::string& accessToken,
                                                  const std::string& xxToken,
                                                  const std::string& printerId,
                                                  int orderId,
                                                  const std::string& projectId,
                                                  const std::string& dataJson);

} // namespace accloud::cloud::api::legacy
