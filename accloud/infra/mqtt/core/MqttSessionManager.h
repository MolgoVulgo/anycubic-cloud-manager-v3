#pragma once

#include <functional>
#include <memory>
#include <cstddef>
#include <string>
#include <vector>

namespace accloud::mqtt::core {

enum class MqttSessionState {
    Stopped,
    Connecting,
    Connected,
    Reconnecting,
    Error,
};

struct MqttSessionConfig {
    std::string host;
    int port{8883};
    int keepAliveSeconds{1200};
    bool cleanSession{true};
    std::string caCertificatePath;
    std::string clientCertificatePath;
    std::string clientKeyPath;
    bool allowInsecureTls{false};
    int reconnectBaseDelayMs{5000};
    int reconnectMaxDelayMs{60000};
    int reconnectJitterMs{500};
};

struct MqttCredentials {
    std::string clientId;
    std::string username;
    std::string password;
};

struct MqttSessionCallbacks {
    std::function<void(MqttSessionState, const std::string&)> onStateChanged;
    std::function<void()> onConnected;
    std::function<void(std::size_t subscribedCount)> onSubscriptionsApplied;
    std::function<void(const std::string&)> onDisconnected;
    std::function<void(int attempt, int delayMs)> onReconnecting;
    std::function<void()> onResyncRequired;
    std::function<void(const std::string& topic, const std::string& payload)> onMessage;
};

struct MqttSessionStartResult {
    bool ok{false};
    std::string code;
    std::string message;
};

class MqttSessionManager {
public:
    MqttSessionManager();
    ~MqttSessionManager();

    MqttSessionManager(const MqttSessionManager&) = delete;
    MqttSessionManager& operator=(const MqttSessionManager&) = delete;

    void setCallbacks(MqttSessionCallbacks callbacks);

    MqttSessionStartResult start(const MqttSessionConfig& config,
                                 const MqttCredentials& credentials,
                                 const std::vector<std::string>& subscriptions);
    std::size_t mergeSubscriptions(const std::vector<std::string>& subscriptions);
    void stop();

    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] MqttSessionState state() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace accloud::mqtt::core
