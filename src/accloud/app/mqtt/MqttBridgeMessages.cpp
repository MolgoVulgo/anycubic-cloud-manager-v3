#include "app/MqttBridge.h"

#include "app/MqttTailModel.h"
#include "app/realtime/PrinterRealtimeStore.h"
#include "app/usecases/cloud/OrderResponseTracker.h"
#include "infra/logging/JsonlLogger.h"
#include "infra/logging/Redactor.h"
#include "infra/mqtt/observability/TelemetryObservationStore.h"
#include "infra/mqtt/routing/MqttMessageRouter.h"

#include <nlohmann/json.hpp>

#include <QDateTime>
#include <QMetaObject>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <mutex>

namespace accloud {
namespace {

constexpr std::size_t kMaxTopicMessageHistory = 1000;
constexpr const char* kMqttCaptureFilename = "mqtt_topic_capture.jsonl";

accloud::mqtt::routing::MqttMessageRouter& messageRouter() {
    static accloud::mqtt::routing::MqttMessageRouter router;
    return router;
}

std::string routeDispositionToString(mqtt::routing::RouteDisposition disposition) {
    using D = mqtt::routing::RouteDisposition;
    switch (disposition) {
        case D::Routed:
            return "Routed";
        case D::UnknownMessage:
            return "UnknownMessage";
        case D::InvalidEnvelope:
            return "InvalidEnvelope";
        case D::InvalidJson:
            return "InvalidJson";
        case D::Ignored:
            return "Ignored";
    }
    return "Ignored";
}

bool isSensitiveKey(const std::string& key) {
    std::string lowered;
    lowered.resize(key.size());
    std::transform(key.begin(), key.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered == "token"
        || lowered == "auth_token"
        || lowered == "accesstoken"
        || lowered == "access_token"
        || lowered == "refresh_token"
        || lowered == "password"
        || lowered == "passwd"
        || lowered == "secret"
        || lowered == "client_secret"
        || lowered == "private_key"
        || lowered == "authorization";
}

bool isPrintOrderProgressEnvelope(const mqtt::routing::MqttEnvelope& envelope) {
    if (envelope.type != "print") {
        return false;
    }
    if (envelope.state == "failed") {
        return true;
    }
    if (!envelope.data.is_object() || !envelope.data.contains("taskid") || envelope.data["taskid"].is_null()) {
        return false;
    }
    return (envelope.action == "update" && envelope.state == "downloading")
        || envelope.action == "start";
}

void redactJsonInPlace(nlohmann::json& node) {
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            if (isSensitiveKey(it.key())) {
                if (!it.value().is_null()) {
                    it.value() = "***REDACTED***";
                }
                continue;
            }
            redactJsonInPlace(it.value());
        }
        return;
    }
    if (node.is_array()) {
        for (auto& item : node) {
            redactJsonInPlace(item);
        }
    }
}

std::string redactPayloadForDebug(const std::string& payload) {
    if (payload.empty()) {
        return payload;
    }
    try {
        nlohmann::json root = nlohmann::json::parse(payload);
        redactJsonInPlace(root);
        return root.dump(2);
    } catch (...) {
        return logging::redactMessage(payload);
    }
}

std::filesystem::path mqttCapturePath() {
    if (const char* envPath = std::getenv("ACCLOUD_MQTT_CAPTURE_PATH");
        envPath != nullptr && *envPath != '\0') {
        return std::filesystem::path(envPath);
    }
    return logging::logDirectory() / kMqttCaptureFilename;
}

void appendMqttCaptureLine(const std::string& topic,
                           const std::string& redactedPayload,
                           std::size_t payloadBytes,
                           const QString& timestampIso) {
    static std::mutex captureMutex;
    std::lock_guard<std::mutex> lock(captureMutex);

    static std::filesystem::path captureFile;
    static bool openAttempted = false;
    static std::ofstream stream;
    static bool writeFailureReported = false;

    if (!openAttempted) {
        openAttempted = true;
        captureFile = mqttCapturePath();
        std::error_code ec;
        const auto parent = captureFile.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
        }
        stream.open(captureFile, std::ios::out | std::ios::app);
        if (!stream.is_open() && !writeFailureReported) {
            writeFailureReported = true;
            logging::warn("mqtt",
                          "mqtt_capture",
                          "capture_open_failed",
                          "Unable to open MQTT capture file",
                          {{"path", captureFile.string()}});
        } else if (stream.is_open()) {
            logging::info("mqtt",
                          "mqtt_capture",
                          "capture_file_ready",
                          "MQTT capture file initialized",
                          {{"path", captureFile.string()}});
        }
    }

    if (!stream.is_open()) {
        return;
    }

    nlohmann::json line;
    line["ts"] = timestampIso.toStdString();
    line["direction"] = "rx";
    line["topic"] = topic;
    line["payload"] = redactedPayload;
    line["payload_bytes"] = payloadBytes;

    stream << line.dump() << '\n';
    stream.flush();
}

int jsonIntValueOr(const nlohmann::json& node, int fallback = 0) {
    if (node.is_number_integer()) {
        return node.get<int>();
    }
    if (node.is_number_float()) {
        return static_cast<int>(node.get<double>());
    }
    if (node.is_string()) {
        try {
            return std::stoi(node.get<std::string>());
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

qint64 jsonInt64ValueOr(const nlohmann::json& node, qint64 fallback = 0) {
    if (node.is_number_integer()) {
        return static_cast<qint64>(node.get<long long>());
    }
    if (node.is_number_float()) {
        return static_cast<qint64>(node.get<double>());
    }
    if (node.is_string()) {
        bool ok = false;
        const qlonglong parsed = QString::fromStdString(node.get<std::string>()).toLongLong(&ok);
        return ok ? static_cast<qint64>(parsed) : fallback;
    }
    return fallback;
}

QString toQStringField(const nlohmann::json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || object[key].is_null()) {
        return {};
    }
    const auto& value = object[key];
    if (value.is_string()) {
        return QString::fromStdString(value.get<std::string>());
    }
    if (value.is_number_integer()) {
        return QString::number(value.get<long long>());
    }
    if (value.is_number_float()) {
        return QString::number(value.get<double>());
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return {};
}

const nlohmann::json* firstArrayField(const nlohmann::json& object,
                                      std::initializer_list<const char*> keys) {
    if (!object.is_object()) {
        return nullptr;
    }
    for (const char* key : keys) {
        if (!object.contains(key) || !object[key].is_array()) {
            continue;
        }
        return &object[key];
    }
    return nullptr;
}

QString firstQStringField(const nlohmann::json& object, std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const QString value = toQStringField(object, key).trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QVariantMap fileRecordToVariantMap(const nlohmann::json& record) {
    QVariantMap map;
    if (record.is_string()) {
        map.insert("filename", QString::fromStdString(record.get<std::string>()));
        map.insert("path", QString());
        map.insert("size", 0);
        map.insert("timestamp", 0);
        map.insert("isDir", false);
        return map;
    }
    if (!record.is_object()) {
        return map;
    }
    map.insert("filename", firstQStringField(record, {"filename", "fileName", "file_name", "name"}));
    map.insert("path", firstQStringField(record, {"path", "dir", "directory"}));
    map.insert("size", jsonInt64ValueOr(record.value("size", nlohmann::json{}), 0));
    map.insert("timestamp", jsonInt64ValueOr(record.value("timestamp", record.value("time", nlohmann::json{})), 0));
    bool isDir = false;
    for (const char* key : {"is_dir", "isDir", "dir", "directory"}) {
        if (record.contains(key) && !record[key].is_null()) {
            const auto& isDirNode = record[key];
            if (isDirNode.is_boolean()) {
                isDir = isDirNode.get<bool>();
            } else {
                isDir = jsonIntValueOr(isDirNode, 0) != 0;
            }
            break;
        }
    }
    map.insert("isDir", isDir);
    return map;
}

} // namespace

void MqttBridge::handleIncomingMessage(const std::string& topic, const std::string& payload) {
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const std::string redactedPayload = redactPayloadForDebug(payload);
    appendMqttCaptureLine(topic, redactedPayload, payload.size(), ts);
    const QString topicName = QString::fromStdString(topic);
    const QString messageLine = ts + QStringLiteral(" | topic=") + topicName
        + QStringLiteral(" | payload=") + QString::fromStdString(redactedPayload);
    appendRawLine(messageLine);

    const bool topicAdded = m_receivedTopicSet.insert(topic).second;
    if (topicAdded && m_uiDiagnosticsActive) {
        emit receivedTopicsChanged();
    }
    m_topicMessageHistory.emplace_back(topicName, messageLine);
    while (m_topicMessageHistory.size() > kMaxTopicMessageHistory) {
        m_topicMessageHistory.pop_front();
    }
    m_tailModel->appendMessage(ts,
                               topicName,
                               QString::fromStdString(redactedPayload),
                               static_cast<qsizetype>(payload.size()),
                               messageLine);
    ++m_messageTick;
    if (m_uiDiagnosticsActive) {
        emit messageTickChanged();
    }

    const auto routed = messageRouter().route(topic, payload);
    logging::info("mqtt", "mqtt_flow", "topic_routed",
                  "MQTT topic routed",
                  {
                      {"topic", topic},
                      {"disposition", routeDispositionToString(routed.disposition)},
                      {"reason", routed.reason},
                  });
    if (routed.disposition == mqtt::routing::RouteDisposition::UnknownMessage
        || routed.disposition == mqtt::routing::RouteDisposition::InvalidEnvelope
        || routed.disposition == mqtt::routing::RouteDisposition::InvalidJson) {
        accloud::mqtt::observability::TelemetryObservationStore::instance().observe(
            routed.signature,
            routed.topic,
            routed.printerKey,
            redactedPayload,
            routeDispositionToString(routed.disposition),
            routed.reason);
    }

    if (routed.envelope.type == "file") {
        const QString action = QString::fromStdString(routed.envelope.action).trimmed();
        const QString actionLower = action.toLower();
        QString source;
        if (actionLower == QStringLiteral("listlocal")
            || actionLower == QStringLiteral("local")
            || actionLower.contains(QStringLiteral("local"))) {
            source = QStringLiteral("local");
        } else if (actionLower == QStringLiteral("listudisk")
                   || actionLower == QStringLiteral("udisk")
                   || actionLower.contains(QStringLiteral("udisk"))
                   || actionLower.contains(QStringLiteral("usb"))) {
            source = QStringLiteral("udisk");
        }

        const nlohmann::json* fileRecords =
            firstArrayField(routed.envelope.data, {"records", "files"});
        const bool listLikeAction = actionLower.startsWith(QStringLiteral("list"));

        std::string effectivePrinterId = routed.printerKey;
        if (!routed.printerKey.empty()) {
            const auto it = m_printerKeyToId.find(routed.printerKey);
            if (it != m_printerKeyToId.end() && !it->second.empty()) {
                effectivePrinterId = it->second;
            }
        }

        if (!source.isEmpty() && (fileRecords != nullptr || listLikeAction)) {
            QVariantList records;
            if (fileRecords != nullptr) {
                for (const auto& record : *fileRecords) {
                    records.push_back(fileRecordToVariantMap(record));
                }
            }

            const int code = routed.envelope.raw.contains("code")
                ? jsonIntValueOr(routed.envelope.raw["code"], 0)
                : 0;
            const QString fileState = QString::fromStdString(routed.envelope.state);
            const QString fileMessage = toQStringField(routed.envelope.raw, "msg");
            const QString printerIdText = QString::fromStdString(effectivePrinterId);
            logging::info("app",
                          "mqtt_file_list",
                          "file_list_event",
                          "Printer file list event received",
                          {{"action", routed.envelope.action},
                           {"source", source.toStdString()},
                           {"printer_key", routed.printerKey},
                           {"printer_id", effectivePrinterId},
                           {"records_count", std::to_string(records.size())},
                           {"state", routed.envelope.state},
                           {"code", std::to_string(code)}});
            QMetaObject::invokeMethod(this,
                                      [this, printerIdText, source, records, fileState, code, fileMessage]() {
                                          emit printerFileListReceived(printerIdText,
                                                                       source,
                                                                       records,
                                                                       fileState,
                                                                       code,
                                                                       fileMessage);
                                      },
                                      Qt::QueuedConnection);
        }

        if (!listLikeAction) {
            const int actionCode = routed.envelope.raw.contains("code")
                ? jsonIntValueOr(routed.envelope.raw["code"], 0) : 0;
            const QString actionState = QString::fromStdString(routed.envelope.state);
            const QString actionMessage = toQStringField(routed.envelope.raw, "msg");
            const QString actionMsgId = QString::fromStdString(routed.envelope.msgid);
            const QString printerIdText = QString::fromStdString(effectivePrinterId);
            QMetaObject::invokeMethod(this,
                                      [this, printerIdText, action, actionState, actionCode, actionMsgId, actionMessage]() {
                                          emit printerFileActionReceived(printerIdText,
                                                                         action,
                                                                         actionState,
                                                                         actionCode,
                                                                         actionMsgId,
                                                                         actionMessage);
                                      },
                                      Qt::QueuedConnection);
        }
    }

    if (routed.event.has_value()) {
        auto event = *routed.event;
        if (!routed.printerKey.empty()) {
            auto it = m_printerKeyToId.find(routed.printerKey);
            if (it != m_printerKeyToId.end() && !it->second.empty()) {
                event.printerKey = it->second;
            }
        }
        realtime::PrinterRealtimeStore::instance().applyEvent(event);
        ++m_realtimeEventTick;
        emit realtimeEventTickChanged();
    }
    refreshTelemetrySnapshot();

    const bool ok = routed.envelope.state != "failed";
    auto& tracker = usecases::cloud::OrderResponseTracker::instance();
    if (!routed.envelope.msgid.empty() && isPrintOrderProgressEnvelope(routed.envelope)) {
        (void)tracker.resolveByMsgid(routed.envelope.msgid, ok, routed.envelope.state);
    } else if (!routed.printerKey.empty() && routed.event.has_value()
               && routed.event->type == realtime::MessageType::Print
               && isPrintOrderProgressEnvelope(routed.envelope)) {
        auto it = m_printerKeyToId.find(routed.printerKey);
        const std::string printerId = (it != m_printerKeyToId.end()) ? it->second : routed.printerKey;
        (void)tracker.resolveByFallback(printerId,
                                        usecases::cloud::CorrelationClass::PrintStart,
                                        ok,
                                        routed.envelope.state.empty() ? "fallback" : routed.envelope.state);
    }
}

void MqttBridge::clearRaw() {
    const bool hadTopics = !m_receivedTopicSet.empty();
    const bool hadMessages = !m_topicMessageHistory.empty();
    m_receivedTopicSet.clear();
    m_topicMessageHistory.clear();
    if (hadTopics) {
        emit receivedTopicsChanged();
    }
    if (hadMessages) {
        ++m_messageTick;
        emit messageTickChanged();
    }
    if (m_tailModel != nullptr) {
        m_tailModel->clear();
    }
    m_rawBuffer.clear();
    emit rawBufferChanged();
}

QString MqttBridge::messagesForTopic(const QString& topic) const {
    if (m_tailModel != nullptr) {
        return m_tailModel->messagesForTopic(topic);
    }
    const QString needle = topic.trimmed();
    QStringList out;
    out.reserve(static_cast<int>(m_topicMessageHistory.size()));
    for (const auto& [entryTopic, entryLine] : m_topicMessageHistory) {
        if (needle.isEmpty() || entryTopic == needle) {
            out.push_back(entryLine);
        }
    }
    return out.join('\n');
}

} // namespace accloud
