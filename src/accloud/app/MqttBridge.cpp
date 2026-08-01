#include "MqttBridge.h"

#include "MqttTailModel.h"

#include <QTimer>

namespace accloud {

MqttBridge::MqttBridge(QObject* parent)
    : QObject(parent), m_tailModel(new MqttTailModel(this)) {
    m_tailModel->setUpdatesEnabled(false);
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

    initializeSessionCallbacks();
    startBackgroundAutoConnect();
}

MqttBridge::~MqttBridge() {
    shutdownSession();
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

bool MqttBridge::uiDiagnosticsActive() const {
    return m_uiDiagnosticsActive;
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

void MqttBridge::updateConnected(bool value) {
    if (m_connected == value) {
        return;
    }
    m_connected = value;
    emit connectedChanged();
}

} // namespace accloud
