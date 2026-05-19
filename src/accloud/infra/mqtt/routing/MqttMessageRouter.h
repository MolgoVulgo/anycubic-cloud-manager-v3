#pragma once

#include "domain/realtime/PrinterRealtimeEvent.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <set>
#include <string>

namespace accloud::mqtt::routing {

enum class RouteDisposition {
    Routed,
    UnknownMessage,
    InvalidEnvelope,
    InvalidJson,
    Ignored,
};

struct MqttEnvelope {
    std::string type;
    std::string action;
    std::string state;
    std::string msgid;
    nlohmann::json data = nlohmann::json::object();
    nlohmann::json raw = nlohmann::json::object();
};

struct MqttRouteResult {
    RouteDisposition disposition{RouteDisposition::Ignored};
    bool isUserTopic{false};
    bool isPrinterTopic{false};
    std::string topic;
    std::string printerKey;
    std::string signature;
    std::string reason;
    MqttEnvelope envelope;
    std::optional<accloud::realtime::PrinterRealtimeEvent> event;
};

class MqttMessageRouter {
public:
    MqttRouteResult route(const std::string& topic, const std::string& payload) const;

private:
    static bool isKnownType(const std::string& type);
    static bool isStateRequiredForType(const std::string& type);
    static std::string normalizeState(const nlohmann::json& root);
    static std::string toStringField(const nlohmann::json& root, const char* key);
    static std::optional<accloud::realtime::MessageType> mapMessageType(const std::string& type);
    static std::optional<accloud::realtime::PrintState> mapPrintState(const std::string& state);
    static accloud::realtime::EventKind mapEventKind(accloud::realtime::MessageType type);
    static std::string makeSignature(const MqttEnvelope& envelope);
    static bool topicIsUserReport(const std::string& topic);
    static std::optional<std::string> extractPrinterKey(const std::string& topic);
};

} // namespace accloud::mqtt::routing

