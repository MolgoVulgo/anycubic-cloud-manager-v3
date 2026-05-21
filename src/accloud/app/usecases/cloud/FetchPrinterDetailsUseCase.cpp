#include "FetchPrinterDetailsUseCase.h"

#include "infra/cloud/api/PrintersApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudPrinterDetailsResult
FetchPrinterDetailsUseCase::execute(const std::string& printerId) const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable"};
    }

    const accloud::cloud::api::PrintersApi printersApi;
    return printersApi.details(contextResult.context.accessToken,
                               contextResult.context.xxToken,
                               printerId);
}

} // namespace accloud::usecases::cloud
