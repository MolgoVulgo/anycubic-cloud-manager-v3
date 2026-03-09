#pragma once

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
};

struct PrinterRealtimeEvent {
    EventKind kind{EventKind::Unknown};
    MessageType type{MessageType::Unknown};
    std::string printerKey;
    std::string action;
    std::string state;
    std::string msgid;
    std::string topic;
    std::optional<PrintState> printState;
};

} // namespace accloud::realtime

