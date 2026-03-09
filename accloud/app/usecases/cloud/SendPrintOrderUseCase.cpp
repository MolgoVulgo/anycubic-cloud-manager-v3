#include "SendPrintOrderUseCase.h"

#include "infra/cloud/api/PrintOrderApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudPrintOrderResult SendPrintOrderUseCase::execute(const std::string& printerId,
                                                                     const std::string& fileId,
                                                                     bool deleteAfterPrint) const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable", {}};
    }

    const accloud::cloud::api::PrintOrderApi printOrderApi;
    return printOrderApi.send(contextResult.context.accessToken,
                              contextResult.context.xxToken,
                              printerId, fileId, deleteAfterPrint);
}

} // namespace accloud::usecases::cloud
