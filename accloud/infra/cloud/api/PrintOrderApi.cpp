#include "PrintOrderApi.h"
#include "CloudLegacyImpl.h"

namespace accloud::cloud::api {

CloudPrintOrderResult PrintOrderApi::send(const std::string& accessToken,
                                          const std::string& xxToken,
                                          const std::string& printerId,
                                          const std::string& fileId,
                                          bool deleteAfterPrint) const {
    return legacy::legacySendCloudPrintOrder(accessToken, xxToken, printerId, fileId, deleteAfterPrint);
}

} // namespace accloud::cloud::api
