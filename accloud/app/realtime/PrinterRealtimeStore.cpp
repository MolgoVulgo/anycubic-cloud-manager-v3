#include "PrinterRealtimeStore.h"

namespace accloud::realtime {

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
    if (snapshot.progress.has_value()) dst.progress = snapshot.progress;
    if (snapshot.elapsedSec.has_value()) dst.elapsedSec = snapshot.elapsedSec;
    if (snapshot.remainingSec.has_value()) dst.remainingSec = snapshot.remainingSec;
    if (snapshot.currentLayer.has_value()) dst.currentLayer = snapshot.currentLayer;
    if (snapshot.totalLayers.has_value()) dst.totalLayers = snapshot.totalLayers;
    if (snapshot.currentFile.has_value()) dst.currentFile = snapshot.currentFile;
    if (snapshot.reason.has_value()) dst.reason = snapshot.reason;
    if (snapshot.printState.has_value()) dst.printState = snapshot.printState;
}

void PrinterRealtimeStore::applyEvent(const PrinterRealtimeEvent& event) {
    if (event.printerKey.empty()) {
        return;
    }

    PrinterRealtimeSnapshot snapshot;
    if (!event.state.empty()) {
        if (event.type == MessageType::LastWill) {
            if (event.action == "onlineReport" && event.state == "online") {
                snapshot.state = std::string("READY");
            } else if (event.action == "onlineReport" && event.state == "offline") {
                snapshot.state = std::string("OFFLINE");
            }
        } else if (event.type == MessageType::Status) {
            if (event.action == "workReport" && event.state == "free") {
                snapshot.state = std::string("READY");
            } else if (event.action == "workReport" && event.state == "busy") {
                snapshot.state = std::string("PRINTING");
            }
        } else if (event.type == MessageType::Print) {
            snapshot.printState = event.printState.value_or(PrintState::Unknown);
            if (event.state == "failed") {
                snapshot.state = std::string("ERROR");
            } else if (event.state == "finished" || event.state == "stoped") {
                snapshot.state = std::string("READY");
            } else if (event.state == "printing" || event.state == "preheating"
                       || event.state == "downloading" || event.state == "checking"
                       || event.state == "pausing" || event.state == "paused"
                       || event.state == "resuming" || event.state == "resumed"
                       || event.state == "stopping") {
                snapshot.state = std::string("PRINTING");
            }
        }
    }

    if (event.progress.has_value()) {
        snapshot.progress = event.progress;
    }
    if (event.elapsedSec.has_value()) {
        snapshot.elapsedSec = event.elapsedSec;
    }
    if (event.remainingSec.has_value()) {
        snapshot.remainingSec = event.remainingSec;
    }
    if (event.currentLayer.has_value()) {
        snapshot.currentLayer = event.currentLayer;
    }
    if (event.totalLayers.has_value()) {
        snapshot.totalLayers = event.totalLayers;
    }
    if (event.currentFile.has_value()) {
        snapshot.currentFile = event.currentFile;
    }
    if (event.reason.has_value()) {
        snapshot.reason = event.reason;
    }

    if (!snapshot.state.has_value()
        && !snapshot.printState.has_value()
        && !snapshot.progress.has_value()
        && !snapshot.elapsedSec.has_value()
        && !snapshot.remainingSec.has_value()
        && !snapshot.currentLayer.has_value()
        && !snapshot.totalLayers.has_value()
        && !snapshot.currentFile.has_value()
        && !snapshot.reason.has_value()) {
        return;
    }
    upsert(event.printerKey, snapshot);
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
