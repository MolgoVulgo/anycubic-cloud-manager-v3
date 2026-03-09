#include "MqttSessionManager.h"

#include "infra/logging/JsonlLogger.h"
#include "infra/mqtt/observability/MqttTelemetry.h"

#ifdef ACCLOUD_WITH_MQTT
#include <QByteArray>
#include <QMetaObject>
#include <QMqttClient>
#include <QTimer>
#endif

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
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
    MqttSessionState sessionState{MqttSessionState::Stopped};
    bool running{false};
    bool stopping{false};
    bool hasConnectedOnce{false};
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
    void subscribeAll() {
        if (!client || client->state() != QMqttClient::Connected) {
            return;
        }
        for (const std::string& topic : subscriptions) {
            if (topic.empty()) {
                continue;
            }
            auto* subscription = client->subscribe(QString::fromStdString(topic), 0);
            if (subscription == nullptr) {
                logging::warn("cloud", "mqtt_session", "subscribe_failed",
                              "MQTT subscribe failed",
                              {{"topic", topic}});
            }
        }
    }

    void connectToBroker() {
        if (!client) {
            return;
        }
        publishState(MqttSessionState::Connecting, "connect_request");
        client->setHostname(QString::fromStdString(config.host));
        client->setPort(config.port);
        client->setClientId(QString::fromStdString(credentials.clientId));
        client->setUsername(QString::fromStdString(credentials.username));
        client->setPassword(QString::fromStdString(credentials.password));
        client->setKeepAlive(config.keepAliveSeconds);
        client->setCleanSession(config.cleanSession);
        client->setProtocolVersion(QMqttClient::MQTT_3_1_1);
        client->connectToHostEncrypted();
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
            reconnectAttempt = 0;
            subscribeAll();
            publishState(MqttSessionState::Connected, isReconnect ? "reconnected" : "connected");
            if (callbacks.onConnected) {
                callbacks.onConnected();
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
            scheduleReconnect("disconnected");
        });

        QObject::connect(client.get(), &QMqttClient::errorChanged, [this](QMqttClient::ClientError e) {
            if (e == QMqttClient::NoError) {
                return;
            }
            observability::MqttTelemetry::instance().incrementConnectErrors();
            publishState(MqttSessionState::Error, "client_error");
            logging::warn("cloud", "mqtt_session", "client_error",
                          "MQTT client error signaled",
                          {
                              {"error_code", std::to_string(static_cast<int>(e))},
                          });
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

#ifndef ACCLOUD_WITH_MQTT
    (void)subscriptions;
    m_impl->config = config;
    m_impl->credentials = credentials;
    m_impl->running = false;
    m_impl->publishState(MqttSessionState::Error, "mqtt_not_available");
    return {false, "mqtt_not_available",
            "MQTT support is not available in this build (enable ACCLOUD_ENABLE_MQTT and Qt6::Mqtt)."};
#else
    stop();
    m_impl->config = config;
    m_impl->credentials = credentials;
    m_impl->subscriptions = subscriptions;
    m_impl->running = true;
    m_impl->stopping = false;
    m_impl->hasConnectedOnce = false;
    m_impl->reconnectAttempt = 0;
    m_impl->ensureRuntimeObjects();
    m_impl->connectToBroker();
    return {true, "ok", "MQTT session startup requested"};
#endif
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
