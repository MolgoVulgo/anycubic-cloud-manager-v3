#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <string>

namespace accloud::mqtt::observability {

struct MqttTelemetrySnapshot {
    std::size_t connectErrors{0};
    std::size_t parseErrors{0};
    std::size_t reconnectCount{0};
    std::size_t pendingOrders{0};
    std::map<std::string, std::size_t> unknownSignatures;
};

class MqttTelemetry {
public:
    static MqttTelemetry& instance();

    void incrementConnectErrors();
    void incrementParseErrors();
    void incrementReconnectCount();
    void setPendingOrders(std::size_t value);
    void recordUnknownSignature(const std::string& signature);

    MqttTelemetrySnapshot snapshot() const;
    void clear();

private:
    mutable std::mutex m_mutex;
    MqttTelemetrySnapshot m_state;
};

} // namespace accloud::mqtt::observability

