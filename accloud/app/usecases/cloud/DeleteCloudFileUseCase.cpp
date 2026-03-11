#include "DeleteCloudFileUseCase.h"

#include "infra/cloud/api/FilesApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudOpResult DeleteCloudFileUseCase::execute(const std::string& fileId) const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable"};
    }

    const accloud::cloud::api::FilesApi filesApi;
    return filesApi.remove(contextResult.context.accessToken,
                           contextResult.context.xxToken,
                           fileId);
}

} // namespace accloud::usecases::cloud
