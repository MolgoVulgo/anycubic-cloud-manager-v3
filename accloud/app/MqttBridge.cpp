#include "MqttBridge.h"

#include "MqttTailModel.h"
#include "UiPerfTrace.h"
#include "app/realtime/PrinterRealtimeStore.h"
#include "app/usecases/cloud/LoadPrintersDashboardUseCase.h"
#include "app/usecases/cloud/OrderResponseTracker.h"
#include "app/usecases/cloud/ResyncCloudStateUseCase.h"
#include "infra/cloud/core/SessionProvider.h"
#include "infra/logging/JsonlLogger.h"
#include "infra/logging/Redactor.h"
#include "infra/mqtt/core/MqttCredentialProvider.h"
#include "infra/mqtt/core/MqttSessionManager.h"
#include "infra/mqtt/core/TlsMaterialProvider.h"
#include "infra/mqtt/observability/MqttTelemetry.h"
#include "infra/mqtt/observability/TelemetryObservationStore.h"
#include "infra/mqtt/routing/MqttMessageRouter.h"
#include "infra/mqtt/routing/MqttTopicBuilder.h"
#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QPointer>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <vector>

namespace accloud {
namespace {

constexpr std::size_t kMaxTopicMessageHistory = 1000;
constexpr const char* kMqttCaptureFilename = "mqtt_topic_capture.jsonl";

std::string md5LowerHex(const std::string& input) {
    return QCryptographicHash::hash(QByteArray::fromStdString(input), QCryptographicHash::Md5)
        .toHex()
        .toStdString();
}

std::string trimAscii(std::string value) {
    auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool shouldEnableExtendedTopicsFromEnv() {
    const char* raw = std::getenv("ACCLOUD_MQTT_EXTENDED_TOPICS");
    if (raw == nullptr) {
        return false;
    }
    std::string v = trimAscii(raw);
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

accloud::mqtt::core::MqttSessionManager& sessionManager() {
    static accloud::mqtt::core::MqttSessionManager manager;
    return manager;
}

accloud::mqtt::routing::MqttMessageRouter& messageRouter() {
    static accloud::mqtt::routing::MqttMessageRouter router;
    return router;
}

struct PreparedMqttProfile {
    bool ok{false};
    std::string code;
    std::string message;
    std::string email;
    std::string userId;
    std::string authToken;
    std::vector<std::string> missingFields;
    mqtt::core::MqttSessionConfig config;
    mqtt::core::MqttCredentials credentials;
    std::vector<std::string> subscriptions;
    std::map<std::string, std::string> printerKeyToId;
};

QString toUiConnectionState(mqtt::core::MqttSessionState state) {
    using State = mqtt::core::MqttSessionState;
    switch (state) {
        case State::Stopped:
            return QStringLiteral("Disconnected");
        case State::Connecting:
            return QStringLiteral("Connecting");
        case State::Connected:
            return QStringLiteral("Connected");
        case State::Reconnecting:
            return QStringLiteral("Reconnecting");
        case State::Error:
            return QStringLiteral("Degraded");
    }
    return QStringLiteral("Degraded");
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

QVariantMap fileRecordToVariantMap(const nlohmann::json& record) {
    QVariantMap map;
    if (!record.is_object()) {
        return map;
    }
    map.insert("filename", toQStringField(record, "filename"));
    map.insert("path", toQStringField(record, "path"));
    map.insert("size", jsonInt64ValueOr(record.value("size", nlohmann::json{}), 0));
    map.insert("timestamp", jsonInt64ValueOr(record.value("timestamp", nlohmann::json{}), 0));
    bool isDir = false;
    if (record.contains("is_dir") && !record["is_dir"].is_null()) {
        const auto& isDirNode = record["is_dir"];
        if (isDirNode.is_boolean()) {
            isDir = isDirNode.get<bool>();
        } else {
            isDir = jsonIntValueOr(isDirNode, 0) != 0;
        }
    }
    map.insert("isDir", isDir);
    return map;
}

PreparedMqttProfile buildPreparedProfile() {
    PreparedMqttProfile out;
    out.config.host = "mqtt-universe.anycubic.com";
    out.config.port = 8883;
    out.config.keepAliveSeconds = 1200;
    out.config.cleanSession = true;

    cloud::core::SessionProvider sessionProvider;
    const auto ctxResult = sessionProvider.loadRequestContext();
    if (!ctxResult.ok) {
        out.code = "session_unavailable";
        out.message = ctxResult.message;
        return out;
    }

    const std::string email = ctxResult.context.email;
    const std::string userId = ctxResult.context.userId;
    const std::string authToken = ctxResult.context.mqttAuthToken;
    out.email = email;
    out.userId = userId;
    out.authToken = authToken;
    if (email.empty()) {
        out.missingFields.push_back("email");
    }
    if (userId.empty()) {
        out.missingFields.push_back("user_id");
    }
    if (authToken.empty()) {
        out.missingFields.push_back("auth_token");
    }
    if (!out.missingFields.empty()) {
        logging::warn("app",
                      "mqtt",
                      "mqtt_prereq_missing",
                      "MQTT prefill prerequisites missing",
                      {{"email_present", out.email.empty() ? "0" : "1"},
                       {"user_id_present", out.userId.empty() ? "0" : "1"},
                       {"auth_token_present", out.authToken.empty() ? "0" : "1"}});
        out.code = "mqtt_prereq_missing";
        out.message = "Missing MQTT prerequisites: ";
        for (std::size_t i = 0; i < out.missingFields.size(); ++i) {
            if (i > 0) {
                out.message += ", ";
            }
            out.message += out.missingFields[i];
        }
        return out;
    }

    mqtt::core::MqttCredentialInput credInput;
    credInput.brokerHost = out.config.host;
    credInput.email = email;
    credInput.userId = userId;
    credInput.authToken = authToken;
    credInput.authMode = mqtt::core::MqttAuthMode::Slicer;

    mqtt::core::MqttCredentialProvider credentialProvider;
    const auto built = credentialProvider.buildCandidates(credInput);
    if (!built.ok || built.candidates.empty()) {
        out.code = built.code.empty() ? "mqtt_credentials_failed" : built.code;
        out.message = built.message.empty() ? "mqtt_credentials_failed" : built.message;
        return out;
    }
    out.credentials = built.candidates.front().credentials;

    const std::string userMd5 = md5LowerHex(userId);
    out.subscriptions = mqtt::routing::MqttTopicBuilder::buildUserReportTopics(userId, userMd5);

    const bool includeExtendedTopics = shouldEnableExtendedTopicsFromEnv();
    usecases::cloud::LoadPrintersDashboardUseCase printersUseCase;
    const auto dashboard = printersUseCase.execute();
    if (dashboard.ok) {
        std::set<std::pair<std::string, std::string>> seenPrinterTargets;
        std::size_t skippedInvalidTarget = 0;
        std::size_t skippedDuplicateTarget = 0;
        for (const auto& p : dashboard.printers) {
            const std::string printerKey = trimAscii(p.printerKey.empty() ? p.id : p.printerKey);
            const std::string machineType = trimAscii(p.machineType.empty() ? p.type : p.machineType);
            const std::string deviceId = printerKey;
            if (!printerKey.empty() && !p.id.empty()) {
                out.printerKeyToId[printerKey] = p.id;
            }
            if (!p.id.empty()) {
                out.printerKeyToId[p.id] = p.id;
            }
            if (machineType.empty() || deviceId.empty()) {
                ++skippedInvalidTarget;
                continue;
            }
            const auto target = std::make_pair(machineType, deviceId);
            if (!seenPrinterTargets.insert(target).second) {
                ++skippedDuplicateTarget;
                continue;
            }
            const auto printerTopics = mqtt::routing::MqttTopicBuilder::buildPrinterSubscriptionTopics(
                machineType, deviceId, includeExtendedTopics);
            out.subscriptions.insert(out.subscriptions.end(), printerTopics.begin(), printerTopics.end());
        }
        const std::set<std::string> uniqueTopics(out.subscriptions.begin(), out.subscriptions.end());
        logging::info("mqtt",
                      "mqtt_flow",
                      "subscription_profile_built",
                      "MQTT subscription profile built",
                      {
                          {"printer_count", std::to_string(dashboard.printers.size())},
                          {"printer_targets", std::to_string(seenPrinterTargets.size())},
                          {"skipped_invalid_target", std::to_string(skippedInvalidTarget)},
                          {"skipped_duplicate_target", std::to_string(skippedDuplicateTarget)},
                          {"topics_total", std::to_string(out.subscriptions.size())},
                          {"topics_unique", std::to_string(uniqueTopics.size())},
                          {"extended_topics", includeExtendedTopics ? "1" : "0"},
                      });
    }

    mqtt::core::TlsMaterialProvider tlsProvider;
    const auto tls = tlsProvider.loadFromEnvironment();
    if (!tls.ok) {
        out.code = tls.code.empty() ? "mqtt_tls_unavailable" : tls.code;
        out.message = tls.message.empty() ? "MQTT TLS materials unavailable" : tls.message;
        return out;
    }
    out.config.caCertificatePath = tls.paths.caCertificatePath.string();
    out.config.clientCertificatePath = tls.paths.clientCertificatePath.string();
    out.config.clientKeyPath = tls.paths.clientKeyPath.string();
    out.config.allowInsecureTls = tls.paths.allowInsecureTls;

    out.ok = true;
    out.code = "ok";
    out.message = "MQTT profile prepared";
    return out;
}

QString formatTelemetrySnapshot() {
    const auto snapshot = accloud::mqtt::observability::MqttTelemetry::instance().snapshot();
    const auto discoveryTop = accloud::mqtt::observability::TelemetryObservationStore::instance().topByCount(5);

    QStringList lines;
    lines << QStringLiteral("connectErrors=%1").arg(static_cast<qulonglong>(snapshot.connectErrors));
    lines << QStringLiteral("parseErrors=%1").arg(static_cast<qulonglong>(snapshot.parseErrors));
    lines << QStringLiteral("reconnectCount=%1").arg(static_cast<qulonglong>(snapshot.reconnectCount));
    lines << QStringLiteral("pendingOrders=%1").arg(static_cast<qulonglong>(snapshot.pendingOrders));

    std::vector<std::pair<std::string, std::size_t>> unknown(snapshot.unknownSignatures.begin(),
                                                              snapshot.unknownSignatures.end());
    std::sort(unknown.begin(), unknown.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }
        return a.first < b.first;
    });
    if (!unknown.empty()) {
        lines << QStringLiteral("unknownSignatures(top5):");
        const std::size_t count = std::min<std::size_t>(5, unknown.size());
        for (std::size_t i = 0; i < count; ++i) {
            lines << QStringLiteral("  %1 => %2")
                         .arg(QString::fromStdString(unknown[i].first))
                         .arg(static_cast<qulonglong>(unknown[i].second));
        }
    }
    if (!discoveryTop.empty()) {
        lines << QStringLiteral("discovery(top5):");
        for (const auto& o : discoveryTop) {
            lines << QStringLiteral("  %1 => %2 [%3]")
                         .arg(QString::fromStdString(o.signature))
                         .arg(static_cast<qulonglong>(o.count))
                         .arg(QString::fromStdString(o.disposition));
        }
    }
    return lines.join('\n');
}

} // namespace

MqttBridge::MqttBridge(QObject* parent)
    : QObject(parent), m_tailModel(new MqttTailModel(this)) {
    setStatus(QStringLiteral("idle"));
    setConnectionState(QStringLiteral("Disconnected"));
    m_subscriptionRefreshTimer = new QTimer(this);
    m_subscriptionRefreshTimer->setInterval(30000);
    QObject::connect(m_subscriptionRefreshTimer, &QTimer::timeout, this, [this]() {
        refreshDynamicSubscriptions();
    });
    m_telemetryTimer = new QTimer(this);
    m_telemetryTimer->setInterval(1000);
    QObject::connect(m_telemetryTimer, &QTimer::timeout, this, [this]() {
        refreshTelemetrySnapshot();
    });
    m_telemetryTimer->start();

    auto& manager = sessionManager();
    manager.setCallbacks({
        .onStateChanged = [this](mqtt::core::MqttSessionState state, const std::string& reason) {
            updateConnected(state == mqtt::core::MqttSessionState::Connected);
            setConnectionState(toUiConnectionState(state));
            setStatus(QString::fromStdString(reason));
        },
        .onConnected = [this]() {
            updateConnected(true);
            setStatus(QStringLiteral("connected"));
            if (!m_manualMode && m_subscriptionRefreshTimer != nullptr) {
                m_subscriptionRefreshTimer->start();
            }
            refreshDynamicSubscriptions();
        },
        .onSubscriptionsApplied = [this](std::size_t subscribedCount) {
            if (subscribedCount == 0) {
                return;
            }
            setConnectionState(QStringLiteral("Subscribed"));
            setStatus(QStringLiteral("subscribed (%1 topic(s))")
                          .arg(static_cast<qulonglong>(subscribedCount)));
        },
        .onDisconnected = [this](const std::string& reason) {
            updateConnected(false);
            setStatus(QString::fromStdString(reason));
            if (m_subscriptionRefreshTimer != nullptr) {
                m_subscriptionRefreshTimer->stop();
            }
        },
        .onReconnecting = [this](int attempt, int delayMs) {
            setStatus(QStringLiteral("reconnecting #%1 (%2 ms)").arg(attempt).arg(delayMs));
        },
        .onResyncRequired = [this]() {
            usecases::cloud::ResyncCloudStateUseCase resyncUseCase;
            const auto res = resyncUseCase.execute();
            appendRawLine(QString::fromStdString("[RESYNC] " + res.message));
        },
        .onMessage = [this](const std::string& topic, const std::string& payload) {
            const QString ts = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
            const std::string redactedPayload = redactPayloadForDebug(payload);
            appendMqttCaptureLine(topic, redactedPayload, payload.size(), ts);
            const QString topicName = QString::fromStdString(topic);
            const QString messageLine = ts + QStringLiteral(" | topic=") + topicName
                + QStringLiteral(" | payload=") + QString::fromStdString(redactedPayload);
            appendRawLine(messageLine);

            const bool topicAdded = m_receivedTopicSet.insert(topic).second;
            if (topicAdded) {
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
            emit messageTickChanged();

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

                const bool hasRecordsArray = routed.envelope.data.is_object()
                    && routed.envelope.data.contains("records")
                    && routed.envelope.data["records"].is_array();
                const bool listLikeAction = actionLower.startsWith(QStringLiteral("list"));

                if (!source.isEmpty() && (hasRecordsArray || listLikeAction)) {
                    std::string effectivePrinterId = routed.printerKey;
                    if (!routed.printerKey.empty()) {
                        const auto it = m_printerKeyToId.find(routed.printerKey);
                        if (it != m_printerKeyToId.end() && !it->second.empty()) {
                            effectivePrinterId = it->second;
                        }
                    }

                    QVariantList records;
                    if (routed.envelope.data.is_object()
                        && routed.envelope.data.contains("records")
                        && routed.envelope.data["records"].is_array()) {
                        for (const auto& record : routed.envelope.data["records"]) {
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
            if (!routed.envelope.msgid.empty()) {
                (void)tracker.resolveByMsgid(routed.envelope.msgid, ok, routed.envelope.state);
            } else if (!routed.printerKey.empty() && routed.event.has_value()
                       && routed.event->type == realtime::MessageType::Print) {
                auto it = m_printerKeyToId.find(routed.printerKey);
                const std::string printerId = (it != m_printerKeyToId.end()) ? it->second : routed.printerKey;
                (void)tracker.resolveByFallback(printerId,
                                                usecases::cloud::CorrelationClass::PrintStart,
                                                ok,
                                                routed.envelope.state.empty() ? "fallback" : routed.envelope.state);
            }
        },
    });

    setStatus(QStringLiteral("idle"));
    QTimer::singleShot(0, this, [this]() {
        if (m_shuttingDown || m_manualMode || m_backgroundAutoConnectStarted || connected()) {
            return;
        }
        m_backgroundAutoConnectStarted = true;
        setConnectionState(QStringLiteral("Connecting"));
        setStatus(QStringLiteral("mqtt_background_connecting"));

        QPointer<MqttBridge> self(this);
        m_backgroundAutoConnectTask = std::async(std::launch::async, [self]() {
            const auto profile = buildPreparedProfile();
            if (self.isNull()) {
                return;
            }
            if (self->m_shuttingDown) {
                return;
            }
            QMetaObject::invokeMethod(
                self.data(),
                [self, profile]() {
                    if (self.isNull()) {
                        return;
                    }
                    MqttBridge* bridge = self.data();
                    if (bridge->m_shuttingDown || bridge->m_manualMode || bridge->connected()) {
                        return;
                    }
                    if (!profile.ok) {
                        bridge->setStatus(QString::fromStdString(
                            profile.code.empty() ? "mqtt_profile_not_ready" : profile.code));
                        bridge->setConnectionState(QStringLiteral("Degraded"));
                        return;
                    }
                    bridge->m_printerKeyToId = profile.printerKeyToId;

                    const auto started = sessionManager().start(profile.config,
                                                                profile.credentials,
                                                                profile.subscriptions);
                    bridge->setStatus(QString::fromStdString(started.message));
                    if (!started.ok) {
                        bridge->setConnectionState(QStringLiteral("Degraded"));
                        if (!bridge->m_subscribedTopics.empty()) {
                            bridge->m_subscribedTopics.clear();
                            emit bridge->subscribedTopicsChanged();
                        }
                        return;
                    }

                    bridge->m_subscribedTopics.clear();
                    for (const auto& topic : profile.subscriptions) {
                        if (topic.empty()) {
                            continue;
                        }
                        bridge->m_subscribedTopics.insert(topic);
                        bridge->appendRawLine(QStringLiteral("[SUBSCRIBE] topic=%1")
                                                  .arg(QString::fromStdString(topic)));
                    }
                    emit bridge->subscribedTopicsChanged();
                    bridge->refreshDynamicSubscriptions();
                },
                Qt::QueuedConnection);
        });
    });
}

MqttBridge::~MqttBridge() {
    m_shuttingDown = true;
    if (m_subscriptionRefreshTimer != nullptr) {
        m_subscriptionRefreshTimer->stop();
    }
    if (m_telemetryTimer != nullptr) {
        m_telemetryTimer->stop();
    }
    auto& manager = sessionManager();
    manager.setCallbacks({});
    manager.stop();
    if (m_backgroundAutoConnectTask.valid()) {
        m_backgroundAutoConnectTask.wait();
    }
}

QString MqttBridge::status() const {
    return m_status;
}

QString MqttBridge::connectionState() const {
    return m_connectionState;
}

bool MqttBridge::connected() const {
    return m_connected;
}

QString MqttBridge::subscribedTopics() const {
    QStringList out;
    out.reserve(static_cast<int>(m_subscribedTopics.size()));
    for (const auto& topic : m_subscribedTopics) {
        out.push_back(QString::fromStdString(topic));
    }
    return out.join('\n');
}

QStringList MqttBridge::receivedTopics() const {
    QStringList out;
    out.reserve(static_cast<int>(m_receivedTopicSet.size()));
    for (const auto& topic : m_receivedTopicSet) {
        out.push_back(QString::fromStdString(topic));
    }
    return out;
}

quint64 MqttBridge::messageTick() const {
    return m_messageTick;
}

QString MqttBridge::rawBuffer() const {
    return m_rawBuffer;
}

QAbstractListModel* MqttBridge::tailModel() {
    return m_tailModel;
}

QString MqttBridge::telemetrySnapshot() const {
    return m_telemetrySnapshot;
}

quint64 MqttBridge::connectErrors() const {
    return m_connectErrors;
}

quint64 MqttBridge::parseErrors() const {
    return m_parseErrors;
}

quint64 MqttBridge::reconnectCount() const {
    return m_reconnectCount;
}

quint64 MqttBridge::pendingOrders() const {
    return m_pendingOrders;
}

QString MqttBridge::unknownTopSummary() const {
    return m_unknownTopSummary;
}

quint64 MqttBridge::realtimeEventTick() const {
    return m_realtimeEventTick;
}

bool MqttBridge::connectRaw(const QString& host,
                            int port,
                            const QString& clientId,
                            const QString& username,
                            const QString& password,
                            const QString& topics,
                            bool useTls) {
    m_manualMode = true;

    mqtt::core::MqttSessionConfig config;
    mqtt::core::MqttCredentials credentials;
    std::vector<std::string> subscriptions;

    const bool hasExplicitCredentials = !clientId.trimmed().isEmpty()
        && !username.trimmed().isEmpty()
        && !password.isEmpty();
    if (!hasExplicitCredentials) {
        const auto profile = buildPreparedProfile();
        if (!profile.ok) {
            setStatus(QString::fromStdString(profile.code.empty() ? "mqtt_profile_not_ready" : profile.code));
            return false;
        }
        config = profile.config;
        credentials = profile.credentials;
        subscriptions = profile.subscriptions;
        m_printerKeyToId = profile.printerKeyToId;
    }

    config.host = host.trimmed().isEmpty() ? config.host : host.trimmed().toStdString();
    if (config.host.empty()) {
        config.host = "mqtt-universe.anycubic.com";
    }
    config.port = (port > 0 ? port : (config.port > 0 ? config.port : 8883));
    config.keepAliveSeconds = 1200;
    config.cleanSession = true;

    if (hasExplicitCredentials) {
        credentials.clientId = clientId.trimmed().toStdString();
        credentials.username = username.trimmed().toStdString();
        credentials.password = password.toStdString();
    }

    mqtt::core::TlsMaterialProvider tlsProvider;
    const auto tls = tlsProvider.loadFromEnvironment();
    if (tls.ok) {
        config.caCertificatePath = tls.paths.caCertificatePath.string();
        config.clientCertificatePath = tls.paths.clientCertificatePath.string();
        config.clientKeyPath = tls.paths.clientKeyPath.string();
        config.allowInsecureTls = tls.paths.allowInsecureTls;
    } else {
        setStatus(QString::fromStdString(tls.code.empty() ? "mqtt_tls_unavailable" : tls.code));
        return false;
    }

    const QStringList rawTopics = topics.split(QRegularExpression("[,;\\n]"), Qt::SkipEmptyParts);
    if (!rawTopics.isEmpty()) {
        subscriptions.clear();
        subscriptions.reserve(rawTopics.size());
        for (const QString& t : rawTopics) {
            const std::string topic = t.trimmed().toStdString();
            if (!topic.empty()) {
                subscriptions.push_back(topic);
            }
        }
    }
    if (subscriptions.empty()) {
        subscriptions.push_back("anycubic/anycubicCloud/v1/#");
    }

    if (!useTls) {
        setStatus(QStringLiteral("manual_insecure_not_supported"));
        return false;
    }

    const auto start = sessionManager().start(config, credentials, subscriptions);
    setStatus(QString::fromStdString(start.message));
    if (!start.ok) {
        setConnectionState(QStringLiteral("Degraded"));
        if (!m_subscribedTopics.empty()) {
            m_subscribedTopics.clear();
            emit subscribedTopicsChanged();
        }
        return start.ok;
    }
    m_subscribedTopics.clear();
    for (const auto& topic : subscriptions) {
        if (!topic.empty()) {
            m_subscribedTopics.insert(topic);
            appendRawLine(QStringLiteral("[SUBSCRIBE] topic=%1").arg(QString::fromStdString(topic)));
        }
    }
    emit subscribedTopicsChanged();
    return start.ok;
}

void MqttBridge::disconnectRaw() {
    sessionManager().stop();
    if (m_subscriptionRefreshTimer != nullptr) {
        m_subscriptionRefreshTimer->stop();
    }
    updateConnected(false);
    setConnectionState(QStringLiteral("Disconnected"));
    setStatus(QStringLiteral("disconnected"));
    if (!m_subscribedTopics.empty()) {
        m_subscribedTopics.clear();
        emit subscribedTopicsChanged();
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

bool MqttBridge::attemptAutoConnect() {
    if (m_manualMode) {
        return false;
    }

    const auto profile = buildPreparedProfile();
    if (!profile.ok) {
        setStatus(QString::fromStdString(profile.code.empty() ? "mqtt_profile_not_ready" : profile.code));
        return false;
    }
    m_printerKeyToId = profile.printerKeyToId;

    const auto started = sessionManager().start(profile.config,
                                                profile.credentials,
                                                profile.subscriptions);
    setStatus(QString::fromStdString(started.message));
    if (!started.ok) {
        setConnectionState(QStringLiteral("Degraded"));
        if (!m_subscribedTopics.empty()) {
            m_subscribedTopics.clear();
            emit subscribedTopicsChanged();
        }
    }
    if (started.ok) {
        m_subscribedTopics.clear();
        for (const auto& topic : profile.subscriptions) {
            if (!topic.empty()) {
                m_subscribedTopics.insert(topic);
                appendRawLine(QStringLiteral("[SUBSCRIBE] topic=%1").arg(QString::fromStdString(topic)));
            }
        }
        emit subscribedTopicsChanged();
        refreshDynamicSubscriptions();
    }
    return started.ok;
}

QVariantMap MqttBridge::suggestedConnection() const {
    QVariantMap out;
    const auto profile = buildPreparedProfile();
    out.insert(QStringLiteral("ok"), profile.ok);
    out.insert(QStringLiteral("code"), QString::fromStdString(profile.code));
    out.insert(QStringLiteral("message"), QString::fromStdString(profile.message));
    out.insert(QStringLiteral("host"), QString::fromStdString(profile.config.host));
    out.insert(QStringLiteral("port"), profile.config.port);
    out.insert(QStringLiteral("useTls"), true);
    out.insert(QStringLiteral("authMode"), QStringLiteral("slicer"));
    out.insert(QStringLiteral("email"), QString::fromStdString(profile.email));
    out.insert(QStringLiteral("userId"), QString::fromStdString(profile.userId));
    out.insert(QStringLiteral("authTokenPresent"), !profile.authToken.empty());
    QVariantList missingFields;
    for (const auto& field : profile.missingFields) {
        missingFields.push_back(QString::fromStdString(field));
    }
    out.insert(QStringLiteral("missingFields"), missingFields);
    out.insert(QStringLiteral("caPath"), QString::fromStdString(profile.config.caCertificatePath));
    out.insert(QStringLiteral("clientCertPath"), QString::fromStdString(profile.config.clientCertificatePath));
    out.insert(QStringLiteral("clientKeyPath"), QString::fromStdString(profile.config.clientKeyPath));
    QStringList topicList;
    for (const std::string& t : profile.subscriptions) {
        topicList.push_back(QString::fromStdString(t));
    }
    out.insert(QStringLiteral("topics"), topicList.join(QStringLiteral(", ")));
    return out;
}

void MqttBridge::refreshDynamicSubscriptions() {
    if (m_manualMode) {
        return;
    }
    if (!connected()) {
        return;
    }

    const bool includeExtendedTopics = shouldEnableExtendedTopicsFromEnv();
    usecases::cloud::LoadPrintersDashboardUseCase printersUseCase;
    const auto dashboard = printersUseCase.execute();
    if (!dashboard.ok) {
        return;
    }

    std::vector<std::string> topics;
    std::map<std::string, std::string> keyToId;
    std::set<std::pair<std::string, std::string>> seenPrinterTargets;
    std::size_t skippedInvalidTarget = 0;
    std::size_t skippedDuplicateTarget = 0;
    for (const auto& p : dashboard.printers) {
        const std::string printerKey = trimAscii(p.printerKey.empty() ? p.id : p.printerKey);
        const std::string machineType = trimAscii(p.machineType.empty() ? p.type : p.machineType);
        const std::string deviceId = printerKey;
        if (!printerKey.empty() && !p.id.empty()) {
            keyToId[printerKey] = p.id;
        }
        if (!p.id.empty()) {
            keyToId[p.id] = p.id;
        }
        if (machineType.empty() || deviceId.empty()) {
            ++skippedInvalidTarget;
            continue;
        }
        const auto target = std::make_pair(machineType, deviceId);
        if (!seenPrinterTargets.insert(target).second) {
            ++skippedDuplicateTarget;
            continue;
        }
        const auto printerTopics = mqtt::routing::MqttTopicBuilder::buildPrinterSubscriptionTopics(
            machineType, deviceId, includeExtendedTopics);
        topics.insert(topics.end(), printerTopics.begin(), printerTopics.end());
    }
    m_printerKeyToId = std::move(keyToId);
    std::vector<std::string> newlyTracked;
    newlyTracked.reserve(topics.size());
    for (const auto& topic : topics) {
        if (topic.empty()) {
            continue;
        }
        if (m_subscribedTopics.insert(topic).second) {
            newlyTracked.push_back(topic);
        }
    }
    const std::size_t added = sessionManager().mergeSubscriptions(topics);
    const std::set<std::string> uniqueTopics(topics.begin(), topics.end());
    logging::info("mqtt",
                  "mqtt_flow",
                  "subscription_refresh_summary",
                  "MQTT dynamic subscription refresh completed",
                  {
                      {"printer_count", std::to_string(dashboard.printers.size())},
                      {"printer_targets", std::to_string(seenPrinterTargets.size())},
                      {"skipped_invalid_target", std::to_string(skippedInvalidTarget)},
                      {"skipped_duplicate_target", std::to_string(skippedDuplicateTarget)},
                      {"topics_total", std::to_string(topics.size())},
                      {"topics_unique", std::to_string(uniqueTopics.size())},
                      {"topics_newly_tracked", std::to_string(newlyTracked.size())},
                      {"topics_newly_applied", std::to_string(added)},
                      {"extended_topics", includeExtendedTopics ? "1" : "0"},
                  });
    if (!newlyTracked.empty()) {
        for (const auto& topic : newlyTracked) {
            appendRawLine(QStringLiteral("[SUBSCRIBE] topic=%1").arg(QString::fromStdString(topic)));
        }
        emit subscribedTopicsChanged();
    } else if (added > 0) {
        appendRawLine(QStringLiteral("[SUBSCRIPTIONS] +%1 topic(s)").arg(static_cast<qulonglong>(added)));
    }
}

void MqttBridge::setStatus(const QString& value) {
    if (m_status == value) {
        return;
    }
    m_status = value;
    emit statusChanged();
}

void MqttBridge::setConnectionState(const QString& value) {
    if (m_connectionState == value) {
        return;
    }
    m_connectionState = value;
    emit connectionStateChanged();
}

void MqttBridge::appendRawLine(const QString& line) {
    static constexpr int kMaxChars = 200000;
    if (!m_rawBuffer.isEmpty()) {
        m_rawBuffer.append('\n');
    }
    m_rawBuffer.append(line);
    if (m_rawBuffer.size() > kMaxChars) {
        m_rawBuffer = m_rawBuffer.right(kMaxChars);
    }
    emit rawBufferChanged();
}

void MqttBridge::updateConnected(bool value) {
    if (m_connected == value) {
        return;
    }
    m_connected = value;
    emit connectedChanged();
}

void MqttBridge::refreshTelemetrySnapshot() {
    UiPerfTrace perf("mqtt_bridge.refresh_telemetry_snapshot");
    const std::size_t expired = usecases::cloud::OrderResponseTracker::instance().expireTimeouts();
    perf.setField("expired_orders", std::to_string(expired));
    if (expired > 0) {
        appendRawLine(QStringLiteral("[TRACKER] expired %1 order(s)").arg(static_cast<qulonglong>(expired)));
    }
    const auto snapshot = accloud::mqtt::observability::MqttTelemetry::instance().snapshot();
    const QString next = formatTelemetrySnapshot();

    bool metricsChanged = false;
    const quint64 nextConnect = static_cast<quint64>(snapshot.connectErrors);
    const quint64 nextParse = static_cast<quint64>(snapshot.parseErrors);
    const quint64 nextReconnect = static_cast<quint64>(snapshot.reconnectCount);
    const quint64 nextPending = static_cast<quint64>(snapshot.pendingOrders);
    if (m_connectErrors != nextConnect) {
        m_connectErrors = nextConnect;
        metricsChanged = true;
    }
    if (m_parseErrors != nextParse) {
        m_parseErrors = nextParse;
        metricsChanged = true;
    }
    if (m_reconnectCount != nextReconnect) {
        m_reconnectCount = nextReconnect;
        metricsChanged = true;
    }
    if (m_pendingOrders != nextPending) {
        m_pendingOrders = nextPending;
        metricsChanged = true;
    }

    std::vector<std::pair<std::string, std::size_t>> unknown(snapshot.unknownSignatures.begin(),
                                                              snapshot.unknownSignatures.end());
    std::sort(unknown.begin(), unknown.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }
        return a.first < b.first;
    });
    QString nextUnknown = QStringLiteral("-");
    if (!unknown.empty()) {
        nextUnknown = QString::fromStdString(unknown.front().first)
            + QStringLiteral(" x")
            + QString::number(static_cast<qulonglong>(unknown.front().second));
    }
    if (m_unknownTopSummary != nextUnknown) {
        m_unknownTopSummary = nextUnknown;
        metricsChanged = true;
    }

    if (m_telemetrySnapshot != next) {
        m_telemetrySnapshot = next;
        emit telemetrySnapshotChanged();
    }
    if (metricsChanged) {
        emit telemetryMetricsChanged();
    }
    perf.setField("metrics_changed", metricsChanged ? "1" : "0");
}

} // namespace accloud
