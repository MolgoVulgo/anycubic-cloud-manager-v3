#include "FetchReasonCatalogUseCase.h"

#include "infra/cloud/api/ReasonCatalogApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudReasonCatalogResult FetchReasonCatalogUseCase::execute() const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable"};
    }

    const accloud::cloud::api::ReasonCatalogApi reasonCatalogApi;
    return reasonCatalogApi.list(contextResult.context.accessToken,
                                 contextResult.context.xxToken);
}

} // namespace accloud::usecases::cloud
