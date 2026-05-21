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
        if (rt.activeTaskId.has_value()) {
            printer.mqttActiveTaskId = *rt.activeTaskId;
        }
        if (rt.printStateText.has_value()) {
            printer.mqttPrintState = *rt.printStateText;
        }
        if (rt.jobStageText.has_value()) {
            printer.mqttJobStage = *rt.jobStageText;
        }
        if (rt.downloadProgress.has_value()) {
            printer.mqttDownloadProgress = *rt.downloadProgress;
        }
        if (rt.resin.uiStatus.has_value()) {
            printer.mqttResinStatus = *rt.resin.uiStatus;
        }
        if (rt.resin.message.has_value()) {
            printer.mqttResinMessage = *rt.resin.message;
        }
        if (rt.resin.phase.has_value()) {
            printer.mqttResinPhase = *rt.resin.phase;
        }
        if (rt.resin.prePrintFillStatus.has_value()) {
            printer.mqttResinPrePrintFillStatus = *rt.resin.prePrintFillStatus;
        }
        if (rt.resin.runtimeTopupStatus.has_value()) {
            printer.mqttResinRuntimeTopupStatus = *rt.resin.runtimeTopupStatus;
        }
        if (rt.resin.bottleStatus.has_value()) {
            printer.mqttResinBottleStatus = *rt.resin.bottleStatus;
        }
        if (rt.resin.vatStatus.has_value()) {
            printer.mqttResinVatStatus = *rt.resin.vatStatus;
        }
        if (rt.resin.lastFeedResinCode.has_value()) {
            printer.mqttResinLastFeedCode = *rt.resin.lastFeedResinCode;
        }
        if (rt.resin.blockingPrint.has_value()) {
            printer.mqttResinBlocking = *rt.resin.blockingPrint;
        }
        if (rt.activeTaskId.has_value()) {
            const auto jobIt = rt.jobs.find(*rt.activeTaskId);
            if (jobIt != rt.jobs.end()) {
                printer.mqttHardwareChecks = jobIt->second.hardwareChecks;
                printer.mqttAutoChecks = jobIt->second.autoChecks;
            }
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
