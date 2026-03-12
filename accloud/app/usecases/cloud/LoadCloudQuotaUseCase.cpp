#include "LoadCloudQuotaUseCase.h"

#include "infra/cloud/api/QuotaApi.h"
#include "infra/cloud/core/SessionProvider.h"

#include <array>
#include <chrono>
#include <thread>

namespace accloud::usecases::cloud {

accloud::cloud::CloudQuotaResult LoadCloudQuotaUseCase::execute() const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable"};
    }

    static constexpr std::array<std::chrono::milliseconds, 3> kDelays = {
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(200),
        std::chrono::milliseconds(800),
    };

    const accloud::cloud::api::QuotaApi quotaApi;
    accloud::cloud::CloudQuotaResult result;
    for (const auto& delay : kDelays) {
        if (delay.count() > 0) {
            std::this_thread::sleep_for(delay);
        }
        result = quotaApi.fetch(contextResult.context.accessToken,
                                contextResult.context.xxToken);
        if (result.ok) {
            break;
        }
    }
    return result;
}

} // namespace accloud::usecases::cloud
