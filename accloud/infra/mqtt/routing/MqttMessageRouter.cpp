#include "MqttMessageRouter.h"

#include "infra/mqtt/observability/MqttTelemetry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace accloud::mqtt::routing {
namespace {

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<std::string> splitTopic(const std::string& topic) {
    std::vector<std::string> parts;
    std::stringstream ss(topic);
    std::string item;
    while (std::getline(ss, item, '/')) {
        parts.push_back(item);
    }
    return parts;
}

std::optional<int> jsonToInt(const nlohmann::json& value) {
    if (value.is_number_integer()) {
        return value.get<int>();
    }
    if (value.is_number_float()) {
        return static_cast<int>(value.get<double>());
    }
    if (value.is_string()) {
        try {
            return std::stoi(value.get<std::string>());
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<int> firstIntField(const nlohmann::json& object,
                                 std::initializer_list<const char*> keys) {
    if (!object.is_object()) {
        return std::nullopt;
    }
    for (const char* key : keys) {
        if (!object.contains(key) || object[key].is_null()) {
            continue;
        }
        auto parsed = jsonToInt(object[key]);
        if (parsed.has_value()) {
            return parsed;
        }
    }
    return std::nullopt;
}

std::optional<std::string> firstStringField(const nlohmann::json& object,
                                            std::initializer_list<const char*> keys) {
    if (!object.is_object()) {
        return std::nullopt;
    }
    for (const char* key : keys) {
        if (!object.contains(key) || object[key].is_null()) {
            continue;
        }
        if (object[key].is_string()) {
            const std::string value = object[key].get<std::string>();
            if (!value.empty()) {
                return value;
            }
        } else if (object[key].is_number_integer()) {
            return std::to_string(object[key].get<long long>());
        }
    }
    return std::nullopt;
}

bool isKnownPrintState(const std::string& state) {
    static const std::array<const char*, 13> kStates = {
        "downloading", "checking", "preheating", "printing", "pausing",
        "paused", "resuming", "resumed", "finished", "stoped",
        "stopping", "updated", "failed",
    };
    return std::any_of(kStates.begin(), kStates.end(), [&](const char* v) {
        return state == v;
    });
}

} // namespace

MqttRouteResult MqttMessageRouter::route(const std::string& topic, const std::string& payload) const {
    MqttRouteResult out;
    out.topic = topic;
    out.isUserTopic = topicIsUserReport(topic);
    const auto printerKey = extractPrinterKey(topic);
    out.isPrinterTopic = printerKey.has_value();
    if (printerKey.has_value()) {
        out.printerKey = *printerKey;
    }

    if (payload.empty()) {
        out.disposition = RouteDisposition::Ignored;
        out.reason = "empty_payload";
        return out;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(payload);
    } catch (...) {
        observability::MqttTelemetry::instance().incrementParseErrors();
        out.disposition = RouteDisposition::InvalidJson;
        out.reason = "invalid_json";
        return out;
    }
    if (!root.is_object()) {
        observability::MqttTelemetry::instance().incrementParseErrors();
        out.disposition = RouteDisposition::InvalidEnvelope;
        out.reason = "root_not_object";
        return out;
    }

    MqttEnvelope env;
    env.type = toStringField(root, "type");
    env.action = toStringField(root, "action");
    env.state = normalizeState(root);
    env.msgid = toStringField(root, "msgid");
    if (root.contains("data") && root["data"].is_object()) {
        env.data = root["data"];
    }
    env.raw = root;
    out.envelope = env;
    out.signature = makeSignature(env);

    if (env.type.empty() || env.action.empty()) {
        observability::MqttTelemetry::instance().incrementParseErrors();
        out.disposition = RouteDisposition::InvalidEnvelope;
        out.reason = "missing_type_or_action";
        return out;
    }

    if (!isKnownType(env.type)) {
        observability::MqttTelemetry::instance().recordUnknownSignature(out.signature);
        out.disposition = RouteDisposition::UnknownMessage;
        out.reason = "unknown_type";
        return out;
    }

    if (out.isPrinterTopic && isStateRequiredForType(env.type) && env.state.empty()) {
        observability::MqttTelemetry::instance().incrementParseErrors();
        out.disposition = RouteDisposition::InvalidEnvelope;
        out.reason = "missing_state_on_printer_topic";
        return out;
    }

    auto messageTypeOpt = mapMessageType(env.type);
    if (!messageTypeOpt.has_value()) {
        observability::MqttTelemetry::instance().recordUnknownSignature(out.signature);
        out.disposition = RouteDisposition::UnknownMessage;
        out.reason = "type_not_mapped";
        return out;
    }

    accloud::realtime::PrinterRealtimeEvent event;
    event.type = *messageTypeOpt;
    event.kind = mapEventKind(*messageTypeOpt);
    event.printerKey = out.printerKey;
    event.action = env.action;
    event.state = env.state;
    event.msgid = env.msgid;
    event.topic = topic;
    if (event.type == accloud::realtime::MessageType::Print && !env.state.empty()) {
        event.printState = mapPrintState(env.state);
    }
    event.progress = firstIntField(env.data, {"progress", "print_progress", "percent", "percentage"});
    event.elapsedSec = firstIntField(env.data, {"elapsed_sec", "elapsed_time", "used_time", "duration"});
    event.remainingSec = firstIntField(env.data, {"remaining_sec", "remaining_time", "left_time"});
    event.currentLayer = firstIntField(env.data, {"current_layer", "layer_now", "layer"});
    event.totalLayers = firstIntField(env.data, {"total_layers", "layer_total", "total_layer"});
    event.currentFile = firstStringField(env.data, {"current_file", "file_name", "filename", "name"});
    event.reason = firstStringField(env.data, {"reason", "reason_text", "message", "msg"});

    out.event = event;
    out.disposition = RouteDisposition::Routed;
    out.reason = "ok";
    return out;
}

bool MqttMessageRouter::isKnownType(const std::string& type) {
    static const std::array<const char*, 11> kTypes = {
        "lastWill", "user", "status", "ota", "tempature", "fan",
        "print", "multiColorBox", "extfilbox", "file", "peripherie",
    };
    return std::any_of(kTypes.begin(), kTypes.end(), [&](const char* v) {
        return type == v;
    });
}

bool MqttMessageRouter::isStateRequiredForType(const std::string& type) {
    return isKnownType(type);
}

std::string MqttMessageRouter::normalizeState(const nlohmann::json& root) {
    if (!root.contains("state") || root["state"].is_null()) {
        return {};
    }
    if (root["state"].is_string()) {
        const std::string value = root["state"].get<std::string>();
        if (value.empty()) {
            return {};
        }
        return value;
    }
    if (root["state"].is_number_integer()) {
        return std::to_string(root["state"].get<long long>());
    }
    if (root["state"].is_boolean()) {
        return root["state"].get<bool>() ? "true" : "false";
    }
    return {};
}

std::string MqttMessageRouter::toStringField(const nlohmann::json& root, const char* key) {
    if (!root.contains(key) || root[key].is_null()) {
        return {};
    }
    if (root[key].is_string()) {
        return root[key].get<std::string>();
    }
    if (root[key].is_number_integer()) {
        return std::to_string(root[key].get<long long>());
    }
    if (root[key].is_number_float()) {
        return std::to_string(root[key].get<double>());
    }
    if (root[key].is_boolean()) {
        return root[key].get<bool>() ? "true" : "false";
    }
    return {};
}

std::optional<accloud::realtime::MessageType> MqttMessageRouter::mapMessageType(const std::string& type) {
    if (type == "lastWill") return accloud::realtime::MessageType::LastWill;
    if (type == "user") return accloud::realtime::MessageType::User;
    if (type == "status") return accloud::realtime::MessageType::Status;
    if (type == "ota") return accloud::realtime::MessageType::Ota;
    if (type == "tempature") return accloud::realtime::MessageType::Temperature;
    if (type == "fan") return accloud::realtime::MessageType::Fan;
    if (type == "print") return accloud::realtime::MessageType::Print;
    if (type == "multiColorBox") return accloud::realtime::MessageType::MultiColorBox;
    if (type == "extfilbox") return accloud::realtime::MessageType::ExternalFilamentBox;
    if (type == "file") return accloud::realtime::MessageType::File;
    if (type == "peripherie") return accloud::realtime::MessageType::Peripheral;
    return std::nullopt;
}

std::optional<accloud::realtime::PrintState> MqttMessageRouter::mapPrintState(const std::string& state) {
    const std::string lowered = toLowerAscii(state);
    if (!isKnownPrintState(lowered)) {
        return accloud::realtime::PrintState::Unknown;
    }
    if (lowered == "downloading") return accloud::realtime::PrintState::Downloading;
    if (lowered == "checking") return accloud::realtime::PrintState::Checking;
    if (lowered == "preheating") return accloud::realtime::PrintState::Preheating;
    if (lowered == "printing") return accloud::realtime::PrintState::Printing;
    if (lowered == "pausing") return accloud::realtime::PrintState::Pausing;
    if (lowered == "paused") return accloud::realtime::PrintState::Paused;
    if (lowered == "resuming") return accloud::realtime::PrintState::Resuming;
    if (lowered == "resumed") return accloud::realtime::PrintState::Resumed;
    if (lowered == "finished") return accloud::realtime::PrintState::Finished;
    if (lowered == "stoped") return accloud::realtime::PrintState::Stopped;
    if (lowered == "stopping") return accloud::realtime::PrintState::Stopping;
    if (lowered == "updated") return accloud::realtime::PrintState::Updated;
    if (lowered == "failed") return accloud::realtime::PrintState::Failed;
    return accloud::realtime::PrintState::Unknown;
}

accloud::realtime::EventKind MqttMessageRouter::mapEventKind(accloud::realtime::MessageType type) {
    using EventKind = accloud::realtime::EventKind;
    using MessageType = accloud::realtime::MessageType;
    switch (type) {
        case MessageType::LastWill:
            return EventKind::LastWillStatus;
        case MessageType::User:
            return EventKind::UserBinding;
        case MessageType::Status:
            return EventKind::PrinterAvailability;
        case MessageType::Ota:
            return EventKind::OtaProgress;
        case MessageType::Temperature:
            return EventKind::TemperatureUpdate;
        case MessageType::Fan:
            return EventKind::FanUpdate;
        case MessageType::Print:
            return EventKind::PrintUpdate;
        case MessageType::MultiColorBox:
            return EventKind::MultiColorBoxUpdate;
        case MessageType::ExternalFilamentBox:
            return EventKind::ExternalFilamentBoxUpdate;
        case MessageType::File:
            return EventKind::FileUpdate;
        case MessageType::Peripheral:
            return EventKind::PeripheralUpdate;
        case MessageType::Unknown:
            return EventKind::Unknown;
    }
    return EventKind::Unknown;
}

std::string MqttMessageRouter::makeSignature(const MqttEnvelope& envelope) {
    return envelope.type + "|" + envelope.action + "|" + (envelope.state.empty() ? "-" : envelope.state);
}

bool MqttMessageRouter::topicIsUserReport(const std::string& topic) {
    const auto parts = splitTopic(topic);
    if (parts.size() != 8) {
        return false;
    }
    const bool prefixOk = parts[0] == "anycubic"
        && parts[1] == "anycubicCloud"
        && parts[2] == "v1"
        && parts[3] == "server"
        && parts[4] == "app";
    if (!prefixOk) {
        return false;
    }
    const std::string tail = parts[6] + "/" + parts[7];
    return tail == "slice/report" || tail == "fdmslice/report";
}

std::optional<std::string> MqttMessageRouter::extractPrinterKey(const std::string& topic) {
    const auto parts = splitTopic(topic);
    if (parts.size() < 7) {
        return std::nullopt;
    }
    if (parts[0] != "anycubic" || parts[1] != "anycubicCloud" || parts[2] != "v1") {
        return std::nullopt;
    }

    // anycubic/anycubicCloud/v1/printer/app/<machine_type>/<printer_key>/...
    if (parts.size() >= 7 && parts[3] == "printer" && parts[4] == "app") {
        if (!parts[6].empty()) {
            return parts[6];
        }
    }

    // anycubic/anycubicCloud/v1/printer/public/<machine_type>/<printer_key>/...
    if (parts.size() >= 7 && parts[3] == "printer" && parts[4] == "public") {
        if (!parts[6].empty()) {
            return parts[6];
        }
    }

    // anycubic/anycubicCloud/v1/+/public/<machine_type>/<printer_key>/...
    if (parts.size() >= 7 && parts[4] == "public") {
        if (!parts[6].empty()) {
            return parts[6];
        }
    }

    return std::nullopt;
}

} // namespace accloud::mqtt::routing
