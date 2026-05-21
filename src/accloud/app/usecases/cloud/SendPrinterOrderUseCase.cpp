#include "SendPrinterOrderUseCase.h"

#include "infra/cloud/api/PrintOrderApi.h"
#include "infra/cloud/core/SessionProvider.h"

namespace accloud::usecases::cloud {

accloud::cloud::CloudPrintOrderResult SendPrinterOrderUseCase::execute(
    const std::string& printerId,
    int orderId,
    const std::string& projectId,
    const std::string& dataJson) const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable", {}, {}, {}, {}};
    }

    const accloud::cloud::api::PrintOrderApi printOrderApi;
    return printOrderApi.sendCommand(contextResult.context.accessToken,
                                     contextResult.context.xxToken,
                                     printerId,
                                     orderId,
                                     projectId,
                                     dataJson);
}

} // namespace accloud::usecases::cloud
