#include "FetchPrinterProjectsUseCase.h"

#include "infra/cloud/api/ProjectsApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudPrinterProjectsResult
FetchPrinterProjectsUseCase::execute(const std::string& printerId, int page, int limit) const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable"};
    }

    const accloud::cloud::api::ProjectsApi projectsApi;
    return projectsApi.listByPrinter(contextResult.context.accessToken,
                                     contextResult.context.xxToken,
                                     printerId, page, limit);
}

} // namespace accloud::usecases::cloud
