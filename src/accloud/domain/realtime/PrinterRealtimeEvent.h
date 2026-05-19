#pragma once

#include <map>
#include <optional>
#include <string>

namespace accloud::realtime {

enum class MessageType {
    LastWill,
    User,
    Status,
    Ota,
    Temperature,
    Fan,
    Print,
    MultiColorBox,
    ExternalFilamentBox,
    File,
    Peripheral,
    Unknown,
};

enum class PrinterAvailability {
    Unknown,
    Free,
    Busy,
};

enum class PrintState {
    Downloading,
    Checking,
    Preheating,
    Printing,
    Pausing,
    Paused,
    Resuming,
    Resumed,
    Finished,
    Stopped,
    Stopping,
    Updated,
    Failed,
    Unknown,
};

enum class PrintJobStage {
    CommandSent,
    Downloading,
    Downloaded,
    Loaded,
    Checking,
    Preheating,
    Printing,
    Finished,
    InterruptedOrUnknown,
    Unknown,
};

enum class EventKind {
    Unknown,
    Invalid,
    LastWillStatus,
    UserBinding,
    PrinterAvailability,
    OtaProgress,
    TemperatureUpdate,
    FanUpdate,
    PrintUpdate,
    MultiColorBoxUpdate,
    ExternalFilamentBoxUpdate,
    FileUpdate,
    PeripheralUpdate,
    ReleaseFilmUpdate,
    AutoOperationUpdate,
    WifiUpdate,
};

struct PrinterRealtimeEvent {
    EventKind kind{EventKind::Unknown};
    MessageType type{MessageType::Unknown};
    std::string wireType;
    std::string printerKey;
    std::string action;
    std::string state;
    std::string msgid;
    std::string topic;
    std::optional<std::string> taskId;
    std::optional<PrintState> printState;
    std::optional<int> progress;
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
    std::optional<std::string> reason;
    std::optional<int> code;
    std::optional<std::string> releaseFilmStatus;
    std::optional<int> releaseFilmLayers;
    std::optional<int> releaseFilmTimes;
    std::optional<int> releaseFilmStatusCode;
    std::map<std::string, int> hardwareChecks;
    std::map<std::string, int> autoChecks;
};

} // namespace accloud::realtime
