#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QVariantMap>
#include <deque>
#include <future>
#include <map>
#include <set>
class QTimer;

namespace accloud {

class MqttBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString subscribedTopics READ subscribedTopics NOTIFY subscribedTopicsChanged)
    Q_PROPERTY(QStringList receivedTopics READ receivedTopics NOTIFY receivedTopicsChanged)
    Q_PROPERTY(quint64 messageTick READ messageTick NOTIFY messageTickChanged)
    Q_PROPERTY(QString rawBuffer READ rawBuffer NOTIFY rawBufferChanged)
    Q_PROPERTY(QString telemetrySnapshot READ telemetrySnapshot NOTIFY telemetrySnapshotChanged)
    Q_PROPERTY(quint64 connectErrors READ connectErrors NOTIFY telemetryMetricsChanged)
    Q_PROPERTY(quint64 parseErrors READ parseErrors NOTIFY telemetryMetricsChanged)
    Q_PROPERTY(quint64 reconnectCount READ reconnectCount NOTIFY telemetryMetricsChanged)
    Q_PROPERTY(quint64 pendingOrders READ pendingOrders NOTIFY telemetryMetricsChanged)
    Q_PROPERTY(QString unknownTopSummary READ unknownTopSummary NOTIFY telemetryMetricsChanged)
    Q_PROPERTY(quint64 realtimeEventTick READ realtimeEventTick NOTIFY realtimeEventTickChanged)

public:
    explicit MqttBridge(QObject* parent = nullptr);
    ~MqttBridge() override;

    QString status() const;
    QString connectionState() const;
    bool connected() const;
    QString subscribedTopics() const;
    QStringList receivedTopics() const;
    quint64 messageTick() const;
    QString rawBuffer() const;
    QString telemetrySnapshot() const;
    quint64 connectErrors() const;
    quint64 parseErrors() const;
    quint64 reconnectCount() const;
    quint64 pendingOrders() const;
    QString unknownTopSummary() const;
    quint64 realtimeEventTick() const;

    Q_INVOKABLE bool connectRaw(const QString& host,
                                int port,
                                const QString& clientId,
                                const QString& username,
                                const QString& password,
                                const QString& topics,
                                bool useTls = true);
    Q_INVOKABLE void disconnectRaw();
    Q_INVOKABLE void clearRaw();
    Q_INVOKABLE QVariantMap suggestedConnection() const;
    Q_INVOKABLE QString messagesForTopic(const QString& topic) const;

signals:
    void statusChanged();
    void connectionStateChanged();
    void connectedChanged();
    void subscribedTopicsChanged();
    void receivedTopicsChanged();
    void messageTickChanged();
    void rawBufferChanged();
    void telemetrySnapshotChanged();
    void telemetryMetricsChanged();
    void realtimeEventTickChanged();

private:
    bool attemptAutoConnect();
    void refreshDynamicSubscriptions();
    void setStatus(const QString& value);
    void setConnectionState(const QString& value);
    void appendRawLine(const QString& line);
    void updateConnected(bool value);
    void refreshTelemetrySnapshot();

    QString m_status;
    QString m_connectionState;
    bool m_connected{false};
    std::set<std::string> m_subscribedTopics;
    std::set<std::string> m_receivedTopicSet;
    std::deque<std::pair<QString, QString>> m_topicMessageHistory;
    quint64 m_messageTick{0};
    QString m_rawBuffer;
    QString m_telemetrySnapshot;
    quint64 m_connectErrors{0};
    quint64 m_parseErrors{0};
    quint64 m_reconnectCount{0};
    quint64 m_pendingOrders{0};
    QString m_unknownTopSummary;
    quint64 m_realtimeEventTick{0};
    bool m_shuttingDown{false};
    bool m_manualMode{false};
    bool m_backgroundAutoConnectStarted{false};
    std::future<void> m_backgroundAutoConnectTask;
    QTimer* m_subscriptionRefreshTimer{nullptr};
    QTimer* m_telemetryTimer{nullptr};
    std::map<std::string, std::string> m_printerKeyToId;
};

} // namespace accloud
