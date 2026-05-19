#include "GetDownloadUrlUseCase.h"

#include "infra/cloud/api/DownloadsApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudDownloadResult GetDownloadUrlUseCase::execute(const std::string& fileId) const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable", {}};
    }

    const accloud::cloud::api::DownloadsApi downloadsApi;
    return downloadsApi.getSignedUrl(contextResult.context.accessToken,
                                     contextResult.context.xxToken,
                                     fileId);
}

} // namespace accloud::usecases::cloud
