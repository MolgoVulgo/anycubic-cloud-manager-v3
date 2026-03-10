#include "MqttBridge.h"

#include "app/realtime/PrinterRealtimeStore.h"
#include "app/usecases/cloud/LoadPrintersDashboardUseCase.h"
#include "app/usecases/cloud/OrderResponseTracker.h"
#include "app/usecases/cloud/ResyncCloudStateUseCase.h"
#include "infra/cloud/core/SessionProvider.h"
#include "infra/logging/JsonlLogger.h"
#include "infra/mqtt/core/MqttCredentialProvider.h"
#include "infra/mqtt/core/MqttSessionManager.h"
#include "infra/mqtt/core/TlsMaterialProvider.h"
#include "infra/mqtt/observability/MqttTelemetry.h"
#include "infra/mqtt/routing/MqttMessageRouter.h"
#include "infra/mqtt/routing/MqttTopicBuilder.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <vector>

namespace accloud {
namespace {

std::string md5LowerHex(const std::string& input) {
    return QCryptographicHash::hash(QByteArray::fromStdString(input), QCryptographicHash::Md5)
        .toHex()
        .toStdString();
}

QString appSetting(const QString& key, const QString& fallback = {}) {
    QString org = QCoreApplication::organizationName();
    if (org.trimmed().isEmpty()) {
        org = QStringLiteral("accloud");
    }
    QString app = QCoreApplication::applicationName();
    if (app.trimmed().isEmpty()) {
        app = QStringLiteral("accloud");
    }
    QSettings settings(org, app);
    return settings.value(key, fallback).toString();
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
    std::string authMode{"slicer"};
    std::string email;
    std::string userId;
    std::string authToken;
    std::vector<std::string> missingFields;
    mqtt::core::MqttSessionConfig config;
    mqtt::core::MqttCredentials credentials;
    std::vector<std::string> subscriptions;
    std::map<std::string, std::string> printerKeyToId;
};

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

    const QString configuredMode = appSetting(QStringLiteral("mqtt.authMode"), QStringLiteral("slicer"));
    const std::string preferredMode = configuredMode.trimmed().isEmpty()
        ? (ctxResult.context.modeAuth.empty() ? std::string("slicer") : ctxResult.context.modeAuth)
        : configuredMode.trimmed().toStdString();
    out.authMode = preferredMode;

    mqtt::core::MqttCredentialInput credInput;
    credInput.brokerHost = out.config.host;
    credInput.email = email;
    credInput.userId = userId;
    credInput.authToken = authToken;
    credInput.authMode = mqtt::core::MqttCredentialProvider::resolvePreferredMode(preferredMode);

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

    usecases::cloud::LoadPrintersDashboardUseCase printersUseCase;
    const auto dashboard = printersUseCase.execute();
    if (dashboard.ok) {
        for (const auto& p : dashboard.printers) {
            const std::string printerKey = p.printerKey.empty() ? p.id : p.printerKey;
            const std::string machineType = p.machineType.empty() ? p.type : p.machineType;
            if (!printerKey.empty() && !p.id.empty()) {
                out.printerKeyToId[printerKey] = p.id;
            }
            if (!p.id.empty()) {
                out.printerKeyToId[p.id] = p.id;
            }
            const auto printerTopics = mqtt::routing::MqttTopicBuilder::buildPrinterSubscriptionTopics(
                machineType, printerKey);
            out.subscriptions.insert(out.subscriptions.end(), printerTopics.begin(), printerTopics.end());
        }
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
    return lines.join('\n');
}

} // namespace

MqttBridge::MqttBridge(QObject* parent)
    : QObject(parent) {
    setStatus(QStringLiteral("idle"));
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
            appendRawLine(ts + QStringLiteral(" | ") + QString::fromStdString(topic)
                          + QStringLiteral(" | ") + QString::fromStdString(payload));

            const auto routed = messageRouter().route(topic, payload);
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

    // Manual-first flow: MQTT connection starts only when user clicks Connect.
    setStatus(QStringLiteral("manual_connect_required"));
}

MqttBridge::~MqttBridge() {
    if (m_subscriptionRefreshTimer != nullptr) {
        m_subscriptionRefreshTimer->stop();
    }
    if (m_telemetryTimer != nullptr) {
        m_telemetryTimer->stop();
    }
    sessionManager().stop();
}

QString MqttBridge::status() const {
    return m_status;
}

bool MqttBridge::connected() const {
    return m_connected;
}

QString MqttBridge::rawBuffer() const {
    return m_rawBuffer;
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
    return start.ok;
}

void MqttBridge::disconnectRaw() {
    sessionManager().stop();
    if (m_subscriptionRefreshTimer != nullptr) {
        m_subscriptionRefreshTimer->stop();
    }
    updateConnected(false);
    setStatus(QStringLiteral("disconnected"));
}

void MqttBridge::clearRaw() {
    m_rawBuffer.clear();
    emit rawBufferChanged();
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
    if (started.ok) {
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
    out.insert(QStringLiteral("clientId"), QString::fromStdString(profile.credentials.clientId));
    out.insert(QStringLiteral("username"), QString::fromStdString(profile.credentials.username));
    out.insert(QStringLiteral("password"), QString::fromStdString(profile.credentials.password));
    out.insert(QStringLiteral("authMode"), QString::fromStdString(profile.authMode));
    out.insert(QStringLiteral("email"), QString::fromStdString(profile.email));
    out.insert(QStringLiteral("userId"), QString::fromStdString(profile.userId));
    out.insert(QStringLiteral("authToken"), QString::fromStdString(profile.authToken));
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

    usecases::cloud::LoadPrintersDashboardUseCase printersUseCase;
    const auto dashboard = printersUseCase.execute();
    if (!dashboard.ok) {
        return;
    }

    std::vector<std::string> topics;
    std::map<std::string, std::string> keyToId;
    for (const auto& p : dashboard.printers) {
        const std::string printerKey = p.printerKey.empty() ? p.id : p.printerKey;
        const std::string machineType = p.machineType.empty() ? p.type : p.machineType;
        if (!printerKey.empty() && !p.id.empty()) {
            keyToId[printerKey] = p.id;
        }
        if (!p.id.empty()) {
            keyToId[p.id] = p.id;
        }
        const auto printerTopics = mqtt::routing::MqttTopicBuilder::buildPrinterSubscriptionTopics(
            machineType, printerKey);
        topics.insert(topics.end(), printerTopics.begin(), printerTopics.end());
    }
    m_printerKeyToId = std::move(keyToId);
    const std::size_t added = sessionManager().mergeSubscriptions(topics);
    if (added > 0) {
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
}

} // namespace accloud
