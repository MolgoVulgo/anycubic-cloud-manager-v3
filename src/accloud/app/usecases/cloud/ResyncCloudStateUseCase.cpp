#include "ResyncCloudStateUseCase.h"

#include "LoadCloudFilesUseCase.h"
#include "LoadCloudQuotaUseCase.h"
#include "LoadPrintersDashboardUseCase.h"

#include <sstream>

namespace accloud::usecases::cloud {

ResyncCloudStateResult ResyncCloudStateUseCase::execute() const {
    const LoadCloudFilesUseCase filesUseCase;
    const LoadCloudQuotaUseCase quotaUseCase;
    const LoadPrintersDashboardUseCase printersUseCase;

    const auto files = filesUseCase.execute(1, 20);
    const auto quota = quotaUseCase.execute();
    const auto printers = printersUseCase.execute();

    ResyncCloudStateResult out;
    out.filesOk = files.ok;
    out.quotaOk = quota.ok;
    out.printersOk = printers.ok;
    out.ok = out.filesOk && out.quotaOk && out.printersOk;

    std::ostringstream msg;
    msg << "resync files=" << (out.filesOk ? "ok" : "ko")
        << " quota=" << (out.quotaOk ? "ok" : "ko")
        << " printers=" << (out.printersOk ? "ok" : "ko");
    out.message = msg.str();
    return out;
}

} // namespace accloud::usecases::cloud

