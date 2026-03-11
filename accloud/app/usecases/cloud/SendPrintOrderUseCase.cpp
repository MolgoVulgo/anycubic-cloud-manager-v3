#include "SendPrintOrderUseCase.h"

#include "OrderResponseTracker.h"
#include "infra/cloud/api/PrintOrderApi.h"
#include "infra/cloud/core/SessionProvider.h"
#include "infra/logging/JsonlLogger.h"

#include <chrono>

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
    auto result = printOrderApi.send(contextResult.context.accessToken,
                                     contextResult.context.xxToken,
                                     printerId, fileId, deleteAfterPrint);
    if (!result.ok) {
        result.correlationStatus = OrderResponseTracker::outcomeToString(CorrelationOutcome::Failure);
        return result;
    }

    TrackerOpenRequest request;
    request.printerId = printerId;
    request.correlationClass = CorrelationClass::PrintStart;
    request.msgid = result.msgId;
    request.timeout = std::chrono::seconds(45);

    auto& tracker = OrderResponseTracker::instance();
    const auto track = tracker.open(request);
    if (!track.ok) {
        result.ok = false;
        result.message = "Correlation tracking failed: " + track.message;
        result.correlationStatus = OrderResponseTracker::outcomeToString(CorrelationOutcome::AmbiguousFallback);
        return result;
    }

    result.correlationTicket = track.ticket;
    result.correlationStatus = OrderResponseTracker::outcomeToString(CorrelationOutcome::Pending);
    logging::info("cloud", "order_tracker", "open",
                  "Order correlation tracking opened",
                  {
                      {"ticket", result.correlationTicket},
                      {"msgid", result.msgId},
                      {"printer_id", printerId},
                      {"correlation_class", OrderResponseTracker::correlationClassToString(CorrelationClass::PrintStart)},
                  });
    return result;
}

} // namespace accloud::usecases::cloud
