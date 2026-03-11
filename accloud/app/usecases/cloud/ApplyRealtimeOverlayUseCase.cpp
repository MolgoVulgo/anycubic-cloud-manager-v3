#include "ApplyRealtimeOverlayUseCase.h"

#include "app/realtime/PrinterRealtimeStore.h"

namespace accloud::usecases::cloud {

std::vector<accloud::cloud::CloudPrinterInfo> ApplyRealtimeOverlayUseCase::execute(
    std::vector<accloud::cloud::CloudPrinterInfo> printers) const {
    auto snapshots = accloud::realtime::PrinterRealtimeStore::instance().snapshotAll();
    for (auto& printer : printers) {
        auto it = snapshots.find(printer.id);
        if (it == snapshots.end() && !printer.printerKey.empty()) {
            it = snapshots.find(printer.printerKey);
        }
        if (it == snapshots.end()) {
            continue;
        }
        const auto& rt = it->second;
        if (rt.state.has_value()) {
            printer.state = *rt.state;
        }
        if (rt.progress.has_value()) {
            printer.progress = *rt.progress;
        }
        if (rt.elapsedSec.has_value()) {
            printer.elapsedSec = *rt.elapsedSec;
        }
        if (rt.remainingSec.has_value()) {
            printer.remainingSec = *rt.remainingSec;
        }
        if (rt.currentLayer.has_value()) {
            printer.currentLayer = *rt.currentLayer;
        }
        if (rt.totalLayers.has_value()) {
            printer.totalLayers = *rt.totalLayers;
        }
        if (rt.currentFile.has_value()) {
            printer.currentFile = *rt.currentFile;
        }
        if (rt.reason.has_value()) {
            printer.reason = *rt.reason;
        }
    }
    return printers;
}

} // namespace accloud::usecases::cloud
