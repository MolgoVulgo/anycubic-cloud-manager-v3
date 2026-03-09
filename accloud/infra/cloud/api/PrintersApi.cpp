#include "PrintersApi.h"
#include "CloudLegacyImpl.h"

namespace accloud::cloud::api {

CloudPrintersResult PrintersApi::list(const std::string& accessToken,
                                      const std::string& xxToken) const {
    return legacy::legacyFetchCloudPrinters(accessToken, xxToken);
}

CloudPrinterCompatResult PrintersApi::compatibilityByExt(const std::string& accessToken,
                                                         const std::string& xxToken,
                                                         const std::string& fileExt) const {
    return legacy::legacyFetchPrinterCompatibilityByExt(accessToken, xxToken, fileExt);
}

CloudPrinterCompatResult PrintersApi::compatibilityByFileId(const std::string& accessToken,
                                                            const std::string& xxToken,
                                                            const std::string& fileId) const {
    return legacy::legacyFetchPrinterCompatibilityByFileId(accessToken, xxToken, fileId);
}

CloudPrinterDetailsResult PrintersApi::details(const std::string& accessToken,
                                               const std::string& xxToken,
                                               const std::string& printerId) const {
    return legacy::legacyFetchPrinterDetails(accessToken, xxToken, printerId);
}

} // namespace accloud::cloud::api
