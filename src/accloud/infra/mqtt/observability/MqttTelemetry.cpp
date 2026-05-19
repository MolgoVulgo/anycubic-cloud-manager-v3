#include "MqttTelemetry.h"

namespace accloud::mqtt::observability {

MqttTelemetry& MqttTelemetry::instance() {
    static MqttTelemetry telemetry;
    return telemetry;
}

void MqttTelemetry::incrementConnectErrors() {
    std::scoped_lock lock(m_mutex);
    ++m_state.connectErrors;
}

void MqttTelemetry::incrementParseErrors() {
    std::scoped_lock lock(m_mutex);
    ++m_state.parseErrors;
}

void MqttTelemetry::incrementReconnectCount() {
    std::scoped_lock lock(m_mutex);
    ++m_state.reconnectCount;
}

void MqttTelemetry::setPendingOrders(std::size_t value) {
    std::scoped_lock lock(m_mutex);
    m_state.pendingOrders = value;
}

void MqttTelemetry::recordUnknownSignature(const std::string& signature) {
    if (signature.empty()) {
        return;
    }
    std::scoped_lock lock(m_mutex);
    ++m_state.unknownSignatures[signature];
}

MqttTelemetrySnapshot MqttTelemetry::snapshot() const {
    std::scoped_lock lock(m_mutex);
    return m_state;
}

void MqttTelemetry::clear() {
    std::scoped_lock lock(m_mutex);
    m_state = {};
}

} // namespace accloud::mqtt::observability

