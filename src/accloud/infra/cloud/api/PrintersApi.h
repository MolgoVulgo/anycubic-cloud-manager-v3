#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::cloud::api {

class PrintersApi {
public:
    CloudPrintersResult list(const std::string& accessToken,
                             const std::string& xxToken) const;

    CloudPrinterCompatResult compatibilityByExt(const std::string& accessToken,
                                                const std::string& xxToken,
                                                const std::string& fileExt) const;

    CloudPrinterCompatResult compatibilityByFileId(const std::string& accessToken,
                                                   const std::string& xxToken,
                                                   const std::string& fileId) const;

    CloudPrinterDetailsResult details(const std::string& accessToken,
                                      const std::string& xxToken,
                                      const std::string& printerId) const;
};

} // namespace accloud::cloud::api
