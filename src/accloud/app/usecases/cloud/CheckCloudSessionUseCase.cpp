#include "CheckCloudSessionUseCase.h"

#include "infra/cloud/api/AuthApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudCheckResult CheckCloudSessionUseCase::execute() const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable"};
    }

    const accloud::cloud::api::AuthApi authApi;
    return authApi.checkAuth(contextResult.context.accessToken, contextResult.context.xxToken);
}

} // namespace accloud::usecases::cloud
