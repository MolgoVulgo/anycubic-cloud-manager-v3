#include "app/MqttBridge.h"

#include "app/usecases/cloud/LoadPrintersDashboardUseCase.h"
#include "app/usecases/cloud/ResyncCloudStateUseCase.h"
#include "infra/cloud/core/SessionProvider.h"
#include "infra/logging/JsonlLogger.h"
#include "infra/mqtt/core/MqttCredentialProvider.h"
#include "infra/mqtt/core/MqttSessionManager.h"
#include "infra/mqtt/core/TlsMaterialProvider.h"
#include "infra/mqtt/routing/MqttTopicBuilder.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <future>
#include <set>
#include <utility>
#include <vector>

namespace accloud {
namespace {

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

} // namespace

void MqttBridge::initializeSessionCallbacks() {
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
            handleIncomingMessage(topic, payload);
        },
    });
}

void MqttBridge::startBackgroundAutoConnect() {
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
                    bridge->m_backgroundAutoConnectStarted = false;
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

void MqttBridge::shutdownSession() {
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

bool MqttBridge::ensureAutoConnected() {
    if (connected()) {
        return true;
    }
    if (m_backgroundAutoConnectStarted) {
        return false;
    }
    m_manualMode = false;
    return attemptAutoConnect();
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

} // namespace accloud
