#include "LoadCloudFilesUseCase.h"

#include "infra/cloud/api/FilesApi.h"
#include "infra/cloud/core/SessionProvider.h"

#include <array>
#include <chrono>
#include <thread>

namespace accloud::usecases::cloud {

LoadCloudFilesResult LoadCloudFilesUseCase::execute(int page, int limit) const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable", {}};
    }

    static constexpr std::array<std::chrono::milliseconds, 3> kDelays = {
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(250),
        std::chrono::milliseconds(900),
    };

    const accloud::cloud::api::FilesApi filesApi;
    accloud::cloud::CloudFilesResult result;
    for (const auto& delay : kDelays) {
        if (delay.count() > 0) {
            std::this_thread::sleep_for(delay);
        }
        result = filesApi.list(contextResult.context.accessToken,
                               contextResult.context.xxToken,
                               page, limit);
        if (result.ok) {
            break;
        }
    }

    return {result.ok, result.message, std::move(result.files)};
}

} // namespace accloud::usecases::cloud
