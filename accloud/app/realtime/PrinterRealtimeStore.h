#pragma once

#include "domain/realtime/PrinterRealtimeEvent.h"

#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace accloud::realtime {

struct PrintJobSnapshot {
    std::string taskId;
    std::optional<std::string> fileId;
    std::optional<std::string> msgId;
    std::optional<std::string> printStateText;
    std::optional<PrintJobStage> stage;
    std::optional<int> downloadProgress;
    std::optional<int> printProgress;
    std::optional<int> elapsedSec;
    std::optional<int> remainingSec;
    std::optional<int> currentLayer;
    std::optional<int> totalLayers;
    std::optional<int> taskMode;
    std::optional<bool> heatingSkipAllowed;
    std::optional<int> heatingRemainingSec;
    std::optional<std::string> currentFile;
    std::optional<std::string> slicer;
    std::map<std::string, int> hardwareChecks;
    std::map<std::string, int> autoChecks;
};

struct PrinterRealtimeSnapshot {
    std::optional<std::string> state;
    std::optional<PrinterAvailability> availability;
    std::optional<std::string> activeTaskId;
    std::optional<std::string> printStateText;
    std::optional<std::string> jobStageText;
    std::optional<PrintJobStage> jobStage;
    std::optional<int> progress;
    std::optional<int> downloadProgress;
    std::optional<int> printProgress;
    std::optional<int> elapsedSec;
    std::optional<int> remainingSec;
    std::optional<int> currentLayer;
    std::optional<int> totalLayers;
    std::optional<std::string> currentFile;
    std::optional<std::string> slicer;
    std::optional<std::string> reason;
    std::optional<std::string> releaseFilmStatus;
    std::optional<int> releaseFilmLayers;
    std::optional<int> releaseFilmTimes;
    std::optional<int> releaseFilmStatusCode;
    std::optional<PrintState> printState;
    std::map<std::string, PrintJobSnapshot> jobs;
};

class PrinterRealtimeStore {
public:
    static PrinterRealtimeStore& instance();

    void upsert(const std::string& printerId, const PrinterRealtimeSnapshot& snapshot);
    void recordPrintCommandSent(const std::string& printerId,
                                const std::string& taskId,
                                const std::string& fileId,
                                const std::string& msgId);
    void applyEvent(const PrinterRealtimeEvent& event);
    std::optional<PrinterRealtimeSnapshot> get(const std::string& printerId) const;
    std::map<std::string, PrinterRealtimeSnapshot> snapshotAll() const;
    void clear();

private:
    mutable std::mutex m_mutex;
    std::map<std::string, PrinterRealtimeSnapshot> m_states;
};

} // namespace accloud::realtime
