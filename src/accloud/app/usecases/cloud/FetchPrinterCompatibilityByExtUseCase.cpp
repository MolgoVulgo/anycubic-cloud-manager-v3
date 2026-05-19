#include "FetchPrinterCompatibilityByExtUseCase.h"

#include "infra/cloud/api/PrintersApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudPrinterCompatResult
FetchPrinterCompatibilityByExtUseCase::execute(const std::string& fileExt) const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable", {}};
    }

    const accloud::cloud::api::PrintersApi printersApi;
    return printersApi.compatibilityByExt(contextResult.context.accessToken,
                                          contextResult.context.xxToken,
                                          fileExt);
}

} // namespace accloud::usecases::cloud
