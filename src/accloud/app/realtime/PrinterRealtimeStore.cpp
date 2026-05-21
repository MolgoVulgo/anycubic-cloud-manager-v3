#include "PrinterRealtimeStore.h"

#include <algorithm>
#include <cctype>

namespace accloud::realtime {
namespace {

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isActiveStage(PrintJobStage stage) {
    return stage == PrintJobStage::Downloading
        || stage == PrintJobStage::Downloaded
        || stage == PrintJobStage::Loaded
        || stage == PrintJobStage::Checking
        || stage == PrintJobStage::Preheating
        || stage == PrintJobStage::Printing;
}

std::string textForStage(PrintJobStage stage) {
    switch (stage) {
        case PrintJobStage::CommandSent:
            return "command_sent";
        case PrintJobStage::Downloading:
            return "downloading";
        case PrintJobStage::Downloaded:
            return "downloaded";
        case PrintJobStage::Loaded:
            return "loaded";
        case PrintJobStage::Checking:
            return "checking";
        case PrintJobStage::Preheating:
            return "preheating";
        case PrintJobStage::Printing:
            return "printing";
        case PrintJobStage::Finished:
            return "finished";
        case PrintJobStage::InterruptedOrUnknown:
            return "interrupted";
        case PrintJobStage::Unknown:
            return "unknown";
    }
    return "unknown";
}

std::optional<std::string> legacyStateFor(const PrinterRealtimeSnapshot& snapshot) {
    if (snapshot.jobStage.has_value()) {
        if (*snapshot.jobStage == PrintJobStage::CommandSent) {
            return std::string("PENDING");
        }
        if (*snapshot.jobStage == PrintJobStage::Finished) {
            if (snapshot.availability.has_value() && *snapshot.availability == PrinterAvailability::Free) {
                return std::string("READY");
            }
            return std::string("PRINTING");
        }
        if (isActiveStage(*snapshot.jobStage)) {
            return std::string("PRINTING");
        }
    }
    if (snapshot.availability.has_value()) {
        if (*snapshot.availability == PrinterAvailability::Free) {
            return std::string("READY");
        }
        if (*snapshot.availability == PrinterAvailability::Busy) {
            return std::string("BUSY");
        }
    }
    return std::nullopt;
}

bool checkMapHasResin(const std::map<std::string, int>& checks) {
    return std::any_of(checks.begin(), checks.end(), [](const auto& item) {
        return toLowerAscii(item.first).find("resin") != std::string::npos;
    });
}

std::string resinPhaseFor(const PrinterRealtimeSnapshot& snapshot) {
    if (!snapshot.jobStage.has_value()) {
        return snapshot.activeTaskId.has_value() ? "before_preheating" : "idle";
    }
    switch (*snapshot.jobStage) {
        case PrintJobStage::CommandSent:
        case PrintJobStage::Downloading:
        case PrintJobStage::Downloaded:
        case PrintJobStage::Loaded:
        case PrintJobStage::Checking:
            return "before_preheating";
        case PrintJobStage::Preheating:
            return "preheating";
        case PrintJobStage::Printing:
            return "during_print";
        case PrintJobStage::Finished:
            return "finished";
        case PrintJobStage::InterruptedOrUnknown:
            return "interrupted";
        case PrintJobStage::Unknown:
            return "unknown";
    }
    return "unknown";
}

std::string resinMessageFor(const PrinterRealtimeEvent& event) {
    if (event.reason.has_value() && !event.reason->empty()) {
        return *event.reason;
    }
    if (event.code.has_value()) {
        return "feedResin code=" + std::to_string(*event.code);
    }
    return "feedResin";
}

void inferResinSuccessOnPreheat(PrinterRealtimeSnapshot& snapshot) {
    if (!snapshot.jobStage.has_value() || *snapshot.jobStage != PrintJobStage::Preheating) {
        return;
    }
    if (!snapshot.resin.autoLoadSeen
        && !snapshot.resin.lastFeedResinCode.has_value()
        && !snapshot.resin.prePrintFillStatus.has_value()) {
        return;
    }
    if (snapshot.resin.blockingPrint.value_or(false)) {
        return;
    }
    snapshot.resin.phase = std::string("preheating");
    snapshot.resin.prePrintFillStatus = std::string("done");
    snapshot.resin.vatStatus = std::string("assumed_ok");
    snapshot.resin.blockingPrint = false;
    snapshot.resin.uiStatus = std::string("done");
}

void applyResinFeedEvent(PrinterRealtimeSnapshot& snapshot, const PrinterRealtimeEvent& event) {
    const std::string phase = resinPhaseFor(snapshot);
    const int code = event.code.value_or(0);
    snapshot.resin.phase = phase;
    snapshot.resin.lastFeedResinCode = code;
    snapshot.resin.lastFeedResinPhase = phase;
    snapshot.resin.message = resinMessageFor(event);

    if (phase == "during_print") {
        if (code == 0) {
            snapshot.resin.runtimeTopupStatus = std::string("done");
            snapshot.resin.blockingPrint = false;
            snapshot.resin.uiStatus = std::string("done");
            return;
        }
        snapshot.resin.runtimeTopupStatus = code == 1501
            ? std::string("bottle_empty_or_unavailable")
            : std::string("warning");
        snapshot.resin.bottleStatus = code == 1501
            ? std::string("empty_or_unavailable")
            : std::string("warning");
        snapshot.resin.vatStatus = std::string("assumed_ok");
        snapshot.resin.blockingPrint = false;
        snapshot.resin.uiStatus = std::string("warning");
        return;
    }

    if (code == 0) {
        snapshot.resin.prePrintFillStatus = std::string("done");
        snapshot.resin.vatStatus = std::string("assumed_ok");
        snapshot.resin.blockingPrint = false;
        snapshot.resin.uiStatus = std::string("done");
        return;
    }

    snapshot.resin.prePrintFillStatus = std::string("error");
    snapshot.resin.blockingPrint = true;
    snapshot.resin.uiStatus = std::string("stop");
}

void updateResinFromPrintEvent(PrinterRealtimeSnapshot& snapshot, const PrinterRealtimeEvent& event) {
    if (!event.autoChecks.empty() && checkMapHasResin(event.autoChecks)) {
        snapshot.resin.autoLoadSeen = true;
        if (!snapshot.resin.prePrintFillStatus.has_value()) {
            snapshot.resin.prePrintFillStatus = std::string("checking");
            snapshot.resin.uiStatus = std::string("resin fill");
        }
    }

    inferResinSuccessOnPreheat(snapshot);

    if (!snapshot.jobStage.has_value()
        || *snapshot.jobStage != PrintJobStage::InterruptedOrUnknown
        || !snapshot.resin.lastFeedResinCode.has_value()
        || *snapshot.resin.lastFeedResinCode == 0) {
        return;
    }

    snapshot.resin.vatStatus = std::string("suspected_resin_issue");
    snapshot.resin.blockingPrint = true;
    snapshot.resin.uiStatus = std::string("stop");
    if (event.reason.has_value() && !event.reason->empty()) {
        snapshot.resin.message = *event.reason;
    }
}

bool hasResinData(const ResinRealtimeState& resin) {
    return resin.autoLoadSeen
        || resin.phase.has_value()
        || resin.prePrintFillStatus.has_value()
        || resin.runtimeTopupStatus.has_value()
        || resin.bottleStatus.has_value()
        || resin.vatStatus.has_value()
        || resin.lastFeedResinCode.has_value()
        || resin.lastFeedResinPhase.has_value()
        || resin.message.has_value()
        || resin.blockingPrint.has_value()
        || resin.uiStatus.has_value();
}

std::string pendingTaskKey(const std::string& fileId, const std::string& msgId) {
    if (!msgId.empty()) {
        return "pending:msgid:" + msgId;
    }
    if (!fileId.empty()) {
        return "pending:file:" + fileId;
    }
    return "pending:command";
}

std::optional<std::string> findCommandSentJobKey(const PrinterRealtimeSnapshot& snapshot) {
    if (snapshot.activeTaskId.has_value()) {
        const auto activeIt = snapshot.jobs.find(*snapshot.activeTaskId);
        if (activeIt != snapshot.jobs.end()
            && activeIt->second.stage.has_value()
            && *activeIt->second.stage == PrintJobStage::CommandSent) {
            return activeIt->first;
        }
    }
    for (const auto& [key, job] : snapshot.jobs) {
        if (job.stage.has_value() && *job.stage == PrintJobStage::CommandSent) {
            return key;
        }
    }
    return std::nullopt;
}

PrintJobStage stageFromPrintEvent(const PrinterRealtimeEvent& event) {
    if (event.action == "update" && event.state == "downloading") {
        if (event.downloadProgress.has_value() && *event.downloadProgress >= 100) {
            return PrintJobStage::Downloaded;
        }
        return PrintJobStage::Downloading;
    }
    if (event.action == "monitor" || event.action == "autoOperation") {
        return PrintJobStage::Checking;
    }
    if (event.action == "start" && event.state == "preheating") {
        return PrintJobStage::Preheating;
    }
    if (event.action == "start" && event.state == "printing") {
        if (event.currentLayer.has_value() && *event.currentLayer >= 1) {
            return PrintJobStage::Printing;
        }
        return PrintJobStage::Loaded;
    }
    if (event.action == "start" && event.state == "finished") {
        return PrintJobStage::Finished;
    }
    if (event.state == "waiting" || event.state == "resuming" || event.state == "resumed") {
        return PrintJobStage::Printing;
    }
    if (event.state == "pausing" || event.state == "paused" || event.state == "stopping") {
        return PrintJobStage::InterruptedOrUnknown;
    }
    if (event.state == "printing") {
        return PrintJobStage::Printing;
    }
    if (event.state == "preheating") {
        return PrintJobStage::Preheating;
    }
    if (event.state == "downloading") {
        return PrintJobStage::Downloading;
    }
    if (event.state == "failed" || event.state == "stoped") {
        return PrintJobStage::InterruptedOrUnknown;
    }
    return PrintJobStage::Unknown;
}

void applyJobFields(PrintJobSnapshot& job, const PrinterRealtimeEvent& event) {
    if (!event.msgid.empty()) job.msgId = event.msgid;
    if (!event.state.empty()) job.printStateText = event.state;
    if (event.downloadProgress.has_value()) job.downloadProgress = event.downloadProgress;
    if (event.printProgress.has_value()) job.printProgress = event.printProgress;
    if (event.elapsedSec.has_value()) job.elapsedSec = event.elapsedSec;
    if (event.remainingSec.has_value()) job.remainingSec = event.remainingSec;
    if (event.currentLayer.has_value()) job.currentLayer = event.currentLayer;
    if (event.totalLayers.has_value()) job.totalLayers = event.totalLayers;
    if (event.taskMode.has_value()) job.taskMode = event.taskMode;
    if (event.heatingSkipAllowed.has_value()) job.heatingSkipAllowed = event.heatingSkipAllowed;
    if (event.heatingRemainingSec.has_value()) job.heatingRemainingSec = event.heatingRemainingSec;
    if (event.currentFile.has_value()) job.currentFile = event.currentFile;
    if (event.slicer.has_value()) job.slicer = event.slicer;
    if (!event.hardwareChecks.empty()) job.hardwareChecks = event.hardwareChecks;
    if (!event.autoChecks.empty()) job.autoChecks = event.autoChecks;
}

void exposeActiveJob(PrinterRealtimeSnapshot& snapshot, const PrintJobSnapshot& job) {
    snapshot.activeTaskId = job.taskId;
    if (job.printStateText.has_value()) snapshot.printStateText = job.printStateText;
    if (job.stage.has_value()) {
        snapshot.jobStage = job.stage;
        snapshot.jobStageText = textForStage(*job.stage);
    }
    if (job.downloadProgress.has_value()) snapshot.downloadProgress = job.downloadProgress;
    if (job.printProgress.has_value()) {
        snapshot.printProgress = job.printProgress;
        snapshot.progress = job.printProgress;
    }
    if (job.elapsedSec.has_value()) snapshot.elapsedSec = job.elapsedSec;
    if (job.remainingSec.has_value()) snapshot.remainingSec = job.remainingSec;
    if (job.currentLayer.has_value()) snapshot.currentLayer = job.currentLayer;
    if (job.totalLayers.has_value()) snapshot.totalLayers = job.totalLayers;
    if (job.currentFile.has_value()) snapshot.currentFile = job.currentFile;
    if (job.slicer.has_value()) snapshot.slicer = job.slicer;
}

} // namespace

PrinterRealtimeStore& PrinterRealtimeStore::instance() {
    static PrinterRealtimeStore store;
    return store;
}

void PrinterRealtimeStore::upsert(const std::string& printerId, const PrinterRealtimeSnapshot& snapshot) {
    if (printerId.empty()) {
        return;
    }
    std::scoped_lock lock(m_mutex);
    PrinterRealtimeSnapshot& dst = m_states[printerId];
    if (snapshot.state.has_value()) dst.state = snapshot.state;
    if (snapshot.availability.has_value()) dst.availability = snapshot.availability;
    if (snapshot.activeTaskId.has_value()) dst.activeTaskId = snapshot.activeTaskId;
    if (snapshot.printStateText.has_value()) dst.printStateText = snapshot.printStateText;
    if (snapshot.jobStageText.has_value()) dst.jobStageText = snapshot.jobStageText;
    if (snapshot.jobStage.has_value()) dst.jobStage = snapshot.jobStage;
    if (snapshot.progress.has_value()) dst.progress = snapshot.progress;
    if (snapshot.downloadProgress.has_value()) dst.downloadProgress = snapshot.downloadProgress;
    if (snapshot.printProgress.has_value()) dst.printProgress = snapshot.printProgress;
    if (snapshot.elapsedSec.has_value()) dst.elapsedSec = snapshot.elapsedSec;
    if (snapshot.remainingSec.has_value()) dst.remainingSec = snapshot.remainingSec;
    if (snapshot.currentLayer.has_value()) dst.currentLayer = snapshot.currentLayer;
    if (snapshot.totalLayers.has_value()) dst.totalLayers = snapshot.totalLayers;
    if (snapshot.currentFile.has_value()) dst.currentFile = snapshot.currentFile;
    if (snapshot.slicer.has_value()) dst.slicer = snapshot.slicer;
    if (snapshot.reason.has_value()) dst.reason = snapshot.reason;
    if (snapshot.releaseFilmStatus.has_value()) dst.releaseFilmStatus = snapshot.releaseFilmStatus;
    if (snapshot.releaseFilmLayers.has_value()) dst.releaseFilmLayers = snapshot.releaseFilmLayers;
    if (snapshot.releaseFilmTimes.has_value()) dst.releaseFilmTimes = snapshot.releaseFilmTimes;
    if (snapshot.releaseFilmStatusCode.has_value()) dst.releaseFilmStatusCode = snapshot.releaseFilmStatusCode;
    if (snapshot.printState.has_value()) dst.printState = snapshot.printState;
    if (hasResinData(snapshot.resin)) dst.resin = snapshot.resin;
    for (const auto& [taskId, job] : snapshot.jobs) {
        dst.jobs[taskId] = job;
    }
}

void PrinterRealtimeStore::recordPrintCommandSent(const std::string& printerId,
                                                  const std::string& taskId,
                                                  const std::string& fileId,
                                                  const std::string& msgId) {
    if (printerId.empty()) {
        return;
    }

    std::scoped_lock lock(m_mutex);
    PrinterRealtimeSnapshot& snapshot = m_states[printerId];
    const std::string key = taskId.empty() ? pendingTaskKey(fileId, msgId) : taskId;
    PrintJobSnapshot& job = snapshot.jobs[key];
    job.taskId = key;
    job.stage = PrintJobStage::CommandSent;
    if (!fileId.empty()) job.fileId = fileId;
    if (!msgId.empty()) job.msgId = msgId;
    exposeActiveJob(snapshot, job);
    if (auto legacy = legacyStateFor(snapshot); legacy.has_value()) {
        snapshot.state = legacy;
    }
}

void PrinterRealtimeStore::applyEvent(const PrinterRealtimeEvent& event) {
    if (event.printerKey.empty()) {
        return;
    }

    std::scoped_lock lock(m_mutex);
    PrinterRealtimeSnapshot& snapshot = m_states[event.printerKey];

    if (event.type == MessageType::LastWill) {
        if (event.action == "onlineReport" && event.state == "online") {
            snapshot.state = std::string("READY");
        } else if (event.action == "onlineReport" && event.state == "offline") {
            snapshot.state = std::string("OFFLINE");
        }
        return;
    }

    if (event.type == MessageType::Status && event.action == "workReport") {
        if (event.state == "free") {
            snapshot.availability = PrinterAvailability::Free;
        } else if (event.state == "busy") {
            snapshot.availability = PrinterAvailability::Busy;
        }
        if (auto legacy = legacyStateFor(snapshot); legacy.has_value()) {
            snapshot.state = legacy;
        }
        return;
    }

    if (event.type != MessageType::Print || !event.taskId.has_value() || event.taskId->empty()) {
        if (event.progress.has_value()) snapshot.progress = event.progress;
        if (event.elapsedSec.has_value()) snapshot.elapsedSec = event.elapsedSec;
        if (event.remainingSec.has_value()) snapshot.remainingSec = event.remainingSec;
        if (event.currentLayer.has_value()) snapshot.currentLayer = event.currentLayer;
        if (event.totalLayers.has_value()) snapshot.totalLayers = event.totalLayers;
        if (event.currentFile.has_value()) snapshot.currentFile = event.currentFile;
        if (event.slicer.has_value()) snapshot.slicer = event.slicer;
        if (event.reason.has_value()) snapshot.reason = event.reason;
        if (event.releaseFilmStatus.has_value()) snapshot.releaseFilmStatus = event.releaseFilmStatus;
        if (event.releaseFilmLayers.has_value()) snapshot.releaseFilmLayers = event.releaseFilmLayers;
        if (event.releaseFilmTimes.has_value()) snapshot.releaseFilmTimes = event.releaseFilmTimes;
        if (event.releaseFilmStatusCode.has_value()) {
            snapshot.releaseFilmStatusCode = event.releaseFilmStatusCode;
        }
        if (event.wireType == "resin" && event.action == "feedResin") {
            applyResinFeedEvent(snapshot, event);
        }
        return;
    }

    if (snapshot.jobs.find(*event.taskId) == snapshot.jobs.end()) {
        const auto pendingKey = findCommandSentJobKey(snapshot);
        if (pendingKey.has_value() && *pendingKey != *event.taskId) {
            PrintJobSnapshot pending = snapshot.jobs[*pendingKey];
            snapshot.jobs.erase(*pendingKey);
            pending.taskId = *event.taskId;
            snapshot.jobs[*event.taskId] = std::move(pending);
        }
    }

    PrintJobSnapshot& job = snapshot.jobs[*event.taskId];
    job.taskId = *event.taskId;
    const PrintJobStage nextStage = stageFromPrintEvent(event);
    if (nextStage != PrintJobStage::Unknown) {
        const bool canEnterChecking = nextStage == PrintJobStage::Checking
            && (!job.stage.has_value()
                || (*job.stage != PrintJobStage::Preheating
                    && *job.stage != PrintJobStage::Printing
                    && *job.stage != PrintJobStage::Finished));
        if (nextStage != PrintJobStage::Checking || canEnterChecking) {
            job.stage = nextStage;
        }
    }
    applyJobFields(job, event);

    snapshot.printState = event.printState.value_or(PrintState::Unknown);
    if (event.reason.has_value()) snapshot.reason = event.reason;
    exposeActiveJob(snapshot, job);
    updateResinFromPrintEvent(snapshot, event);
    if (auto legacy = legacyStateFor(snapshot); legacy.has_value()) {
        snapshot.state = legacy;
    }
}

std::optional<PrinterRealtimeSnapshot> PrinterRealtimeStore::get(const std::string& printerId) const {
    if (printerId.empty()) {
        return std::nullopt;
    }
    std::scoped_lock lock(m_mutex);
    const auto it = m_states.find(printerId);
    if (it == m_states.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::map<std::string, PrinterRealtimeSnapshot> PrinterRealtimeStore::snapshotAll() const {
    std::scoped_lock lock(m_mutex);
    return m_states;
}

void PrinterRealtimeStore::clear() {
    std::scoped_lock lock(m_mutex);
    m_states.clear();
}

} // namespace accloud::realtime
