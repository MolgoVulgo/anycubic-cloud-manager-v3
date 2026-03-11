#include "MqttSessionManager.h"

#include "infra/logging/JsonlLogger.h"
#include "infra/mqtt/core/OpenSslCompat.h"
#include "infra/mqtt/observability/MqttTelemetry.h"

#ifdef ACCLOUD_WITH_MQTT
#include <QByteArray>
#include <QFile>
#include <QList>
#include <QMetaObject>
#include <QMqttClient>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QString>
#include <QTimer>
#endif

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <utility>

namespace accloud::mqtt::core {
namespace {

std::string stateToString(MqttSessionState state) {
    switch (state) {
        case MqttSessionState::Stopped:
            return "stopped";
        case MqttSessionState::Connecting:
            return "connecting";
        case MqttSessionState::Connected:
            return "connected";
        case MqttSessionState::Reconnecting:
            return "reconnecting";
        case MqttSessionState::Error:
            return "error";
    }
    return "unknown";
}

int computeBackoffDelayMs(int attempt, const MqttSessionConfig& config, std::mt19937& rng) {
    const int safeAttempt = std::max(1, attempt);
    const int exponent = std::min(safeAttempt - 1, 16);
    const std::int64_t factor = (std::int64_t{1} << exponent);
    const std::int64_t rawDelay = std::int64_t{config.reconnectBaseDelayMs} * factor;
    const std::int64_t capped = std::min<std::int64_t>(rawDelay, config.reconnectMaxDelayMs);

    const int jitterMax = std::max(0, config.reconnectJitterMs);
    const int jitter = jitterMax > 0 ? std::uniform_int_distribution<int>(0, jitterMax)(rng) : 0;

    const std::int64_t withJitter = capped + jitter;
    return static_cast<int>(std::min<std::int64_t>(withJitter, std::numeric_limits<int>::max()));
}

} // namespace

struct MqttSessionManager::Impl {
    MqttSessionCallbacks callbacks;
    MqttSessionConfig config;
    MqttCredentials credentials;
    std::vector<std::string> subscriptions;
    std::set<std::string> subscriptionSet;
    MqttSessionState sessionState{MqttSessionState::Stopped};
    bool running{false};
    bool stopping{false};
    bool hasConnectedOnce{false};
    bool lastDisconnectFromClientError{false};
    int reconnectAttempt{0};
    std::mt19937 rng{std::random_device{}()};

#ifdef ACCLOUD_WITH_MQTT
    std::unique_ptr<QMqttClient> client;
    std::unique_ptr<QTimer> reconnectTimer;
#endif

    void publishState(MqttSessionState next, const std::string& reason) {
        sessionState = next;
        if (callbacks.onStateChanged) {
            callbacks.onStateChanged(next, reason);
        }
        logging::info("cloud", "mqtt_session", "state_changed",
                      "MQTT session state changed",
                      {
                          {"state", stateToString(next)},
                          {"reason", reason},
                      });
    }

#ifdef ACCLOUD_WITH_MQTT
    std::size_t subscribeAll() {
        if (!client || client->state() != QMqttClient::Connected) {
            return 0;
        }
        std::size_t subscribedCount = 0;
        for (const std::string& topic : subscriptionSet) {
            if (topic.empty()) {
                continue;
            }
            auto* subscription = client->subscribe(QString::fromStdString(topic), 0);
            if (subscription == nullptr) {
                logging::warn("cloud", "mqtt_session", "subscribe_failed",
                              "MQTT subscribe failed",
                              {{"topic", topic}});
                continue;
            }
            ++subscribedCount;
        }
        return subscribedCount;
    }

    std::size_t mergeSubscriptions(const std::vector<std::string>& topics) {
        std::size_t added = 0;
        for (const std::string& topic : topics) {
            if (topic.empty()) {
                continue;
            }
            if (subscriptionSet.insert(topic).second) {
                subscriptions.push_back(topic);
                ++added;
#ifdef ACCLOUD_WITH_MQTT
                if (client && client->state() == QMqttClient::Connected) {
                    auto* subscription = client->subscribe(QString::fromStdString(topic), 0);
                    if (subscription == nullptr) {
                        logging::warn("cloud", "mqtt_session", "subscribe_failed",
                                      "MQTT subscribe failed",
                                      {{"topic", topic}});
                    } else if (callbacks.onSubscriptionsApplied) {
                        callbacks.onSubscriptionsApplied(1);
                    }
                }
#endif
            }
        }
        return added;
    }

    void connectToBroker() {
        if (!client) {
            return;
        }
        lastDisconnectFromClientError = false;
        publishState(MqttSessionState::Connecting, "connect_request");
        client->setHostname(QString::fromStdString(config.host));
        client->setPort(config.port);
        client->setClientId(QString::fromStdString(credentials.clientId));
        client->setUsername(QString::fromStdString(credentials.username));
        client->setPassword(QString::fromStdString(credentials.password));
        client->setKeepAlive(config.keepAliveSeconds);
        client->setCleanSession(config.cleanSession);
        client->setProtocolVersion(QMqttClient::MQTT_3_1_1);

        if (config.allowInsecureTls) {
            const auto opensslCompat = ensureOpenSslSecurityLevelCompat(true);
            if (!opensslCompat.ok) {
                logging::warn("cloud", "mqtt_session", "openssl_compat_apply_failed",
                              "Unable to apply OpenSSL SECLEVEL=0 compatibility profile",
                              {
                                  {"code", opensslCompat.code},
                                  {"detail", opensslCompat.message},
                              });
            } else if (opensslCompat.applied) {
                logging::info("cloud", "mqtt_session", "openssl_compat_applied",
                              "Applied OpenSSL SECLEVEL=0 compatibility profile",
                              {
                                  {"conf_path", opensslCompat.configPath},
                              });
            }
        }

        QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
        ssl.setProtocol(QSsl::TlsV1_2);
        ssl.setPeerVerifyMode(config.allowInsecureTls ? QSslSocket::VerifyNone : QSslSocket::VerifyPeer);
        if (config.allowInsecureTls) {
            ssl.setBackendConfigurationOption(QByteArrayLiteral("CipherString"),
                                              QStringLiteral("ALL:@SECLEVEL=0"));
        }

        if (!config.caCertificatePath.empty()) {
            QFile caFile(QString::fromStdString(config.caCertificatePath));
            if (caFile.open(QIODevice::ReadOnly)) {
                const QList<QSslCertificate> caCerts = QSslCertificate::fromData(
                    caFile.readAll(), QSsl::Pem);
                if (!caCerts.isEmpty()) {
                    ssl.setCaCertificates(caCerts);
                }
            }
        }

        if (!config.clientCertificatePath.empty()) {
            QFile clientCertFile(QString::fromStdString(config.clientCertificatePath));
            if (clientCertFile.open(QIODevice::ReadOnly)) {
                const QList<QSslCertificate> certs = QSslCertificate::fromData(
                    clientCertFile.readAll(), QSsl::Pem);
                if (!certs.isEmpty()) {
                    ssl.setLocalCertificate(certs.front());
                }
            }
        }

        if (!config.clientKeyPath.empty()) {
            QFile clientKeyFile(QString::fromStdString(config.clientKeyPath));
            if (clientKeyFile.open(QIODevice::ReadOnly)) {
                const QSslKey key(clientKeyFile.readAll(), QSsl::Rsa, QSsl::Pem);
                if (!key.isNull()) {
                    ssl.setPrivateKey(key);
                }
            }
        }

        client->connectToHostEncrypted(ssl);
    }

    void scheduleReconnect(const std::string& reason) {
        if (!running || stopping || !reconnectTimer) {
            return;
        }
        ++reconnectAttempt;
        observability::MqttTelemetry::instance().incrementReconnectCount();
        const int delayMs = computeBackoffDelayMs(reconnectAttempt, config, rng);
        publishState(MqttSessionState::Reconnecting, reason);
        if (callbacks.onReconnecting) {
            callbacks.onReconnecting(reconnectAttempt, delayMs);
        }
        reconnectTimer->start(delayMs);
    }

    void ensureRuntimeObjects() {
        if (!client) {
            client = std::make_unique<QMqttClient>();
        }
        if (!reconnectTimer) {
            reconnectTimer = std::make_unique<QTimer>();
            reconnectTimer->setSingleShot(true);
            QObject::connect(reconnectTimer.get(), &QTimer::timeout, [this]() {
                if (!running || stopping) {
                    return;
                }
                connectToBroker();
            });
        }

        QObject::connect(client.get(), &QMqttClient::connected, [this]() {
            if (!running || stopping) {
                return;
            }

            const bool isReconnect = hasConnectedOnce;
            hasConnectedOnce = true;
            lastDisconnectFromClientError = false;
            reconnectAttempt = 0;
            const std::size_t subscribedCount = subscribeAll();
            publishState(MqttSessionState::Connected, isReconnect ? "reconnected" : "connected");
            if (callbacks.onConnected) {
                callbacks.onConnected();
            }
            if (callbacks.onSubscriptionsApplied && subscribedCount > 0) {
                callbacks.onSubscriptionsApplied(subscribedCount);
            }
            if (isReconnect && callbacks.onResyncRequired) {
                callbacks.onResyncRequired();
            }
        });

        QObject::connect(client.get(), &QMqttClient::disconnected, [this]() {
            if (callbacks.onDisconnected) {
                callbacks.onDisconnected(stopping ? "stopped" : "broker_disconnected");
            }

            if (stopping || !running) {
                publishState(MqttSessionState::Stopped, "stopped");
                return;
            }
            if (lastDisconnectFromClientError) {
                publishState(MqttSessionState::Error, "client_error_no_reconnect");
                logging::warn("cloud", "mqtt_session", "reconnect_disabled_on_error",
                              "Auto-reconnect disabled after MQTT client error");
                return;
            }
            scheduleReconnect("disconnected");
        });

        QObject::connect(client.get(), &QMqttClient::errorChanged, [this](QMqttClient::ClientError e) {
            if (e == QMqttClient::NoError) {
                return;
            }
            observability::MqttTelemetry::instance().incrementConnectErrors();
            lastDisconnectFromClientError = true;
            publishState(MqttSessionState::Error, "client_error");
            logging::warn("cloud", "mqtt_session", "client_error",
                          "MQTT client error signaled",
                          {
                              {"error_code", std::to_string(static_cast<int>(e))},
                          });
            if (reconnectTimer) {
                reconnectTimer->stop();
            }
        });

        QObject::connect(client.get(), &QMqttClient::messageReceived,
                         [this](const QByteArray& payload, const QMqttTopicName& topic) {
            if (callbacks.onMessage) {
                callbacks.onMessage(topic.name().toStdString(),
                                    QString::fromUtf8(payload).toStdString());
            }
        });
    }
#endif
};

MqttSessionManager::MqttSessionManager() : m_impl(std::make_unique<Impl>()) {}

MqttSessionManager::~MqttSessionManager() {
    stop();
}

void MqttSessionManager::setCallbacks(MqttSessionCallbacks callbacks) {
    m_impl->callbacks = std::move(callbacks);
}

MqttSessionStartResult MqttSessionManager::start(const MqttSessionConfig& config,
                                                 const MqttCredentials& credentials,
                                                 const std::vector<std::string>& subscriptions) {
    if (config.host.empty()) {
        return {false, "mqtt_invalid_config", "MQTT host is required"};
    }
    if (credentials.clientId.empty() || credentials.username.empty() || credentials.password.empty()) {
        return {false, "mqtt_missing_credentials", "MQTT credentials are incomplete"};
    }
    if (config.caCertificatePath.empty() || config.clientCertificatePath.empty()
        || config.clientKeyPath.empty()) {
        return {false, "mqtt_tls_missing_paths", "MQTT mTLS paths are required"};
    }

#ifndef ACCLOUD_WITH_MQTT
    (void)subscriptions;
    m_impl->config = config;
    m_impl->credentials = credentials;
    m_impl->running = false;
    m_impl->publishState(MqttSessionState::Error, "mqtt_not_available");
    return {false, "mqtt_not_available",
            "MQTT support is not available in this build (Qt6::Mqtt is required)."};
#else
    stop();
    m_impl->config = config;
    m_impl->credentials = credentials;
    m_impl->subscriptions.clear();
    m_impl->subscriptionSet.clear();
    m_impl->mergeSubscriptions(subscriptions);
    m_impl->running = true;
    m_impl->stopping = false;
    m_impl->hasConnectedOnce = false;
    m_impl->reconnectAttempt = 0;
    m_impl->ensureRuntimeObjects();
    m_impl->connectToBroker();
    return {true, "ok", "MQTT session startup requested"};
#endif
}

std::size_t MqttSessionManager::mergeSubscriptions(const std::vector<std::string>& subscriptions) {
    if (!m_impl) {
        return 0;
    }
    return m_impl->mergeSubscriptions(subscriptions);
}

void MqttSessionManager::stop() {
    if (!m_impl) {
        return;
    }
    m_impl->stopping = true;

#ifdef ACCLOUD_WITH_MQTT
    if (m_impl->reconnectTimer) {
        m_impl->reconnectTimer->stop();
    }
    if (m_impl->client && m_impl->client->state() != QMqttClient::Disconnected) {
        m_impl->client->disconnectFromHost();
    }
#endif

    m_impl->running = false;
    m_impl->stopping = false;
    m_impl->publishState(MqttSessionState::Stopped, "stop_called");
}

bool MqttSessionManager::isAvailable() const {
#ifdef ACCLOUD_WITH_MQTT
    return true;
#else
    return false;
#endif
}

MqttSessionState MqttSessionManager::state() const {
    return m_impl->sessionState;
}

} // namespace accloud::mqtt::core
