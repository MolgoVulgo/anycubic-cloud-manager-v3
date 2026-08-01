#include "app/MqttBridge.h"

#include "app/MqttTailModel.h"
#include "app/UiPerfTrace.h"
#include "app/usecases/cloud/OrderResponseTracker.h"
#include "infra/mqtt/observability/MqttTelemetry.h"
#include "infra/mqtt/observability/TelemetryObservationStore.h"

#include <QStringList>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace accloud {
namespace {

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

void MqttBridge::setUiDiagnosticsActive(bool active) {
    if (m_uiDiagnosticsActive == active) {
        return;
    }
    m_uiDiagnosticsActive = active;
    if (m_tailModel != nullptr) {
        m_tailModel->setUpdatesEnabled(active);
    }
    if (active) {
        refreshTelemetrySnapshot();
        emit receivedTopicsChanged();
        emit messageTickChanged();
        emit rawBufferChanged();
    }
    emit uiDiagnosticsActiveChanged();
}

void MqttBridge::appendRawLine(const QString& line) {
    if (!m_uiDiagnosticsActive) {
        return;
    }
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

void MqttBridge::refreshTelemetrySnapshot() {
    UiPerfTrace perf("mqtt_bridge.refresh_telemetry_snapshot");
    const std::size_t expired = usecases::cloud::OrderResponseTracker::instance().expireTimeouts();
    perf.setField("expired_orders", std::to_string(expired));
    if (expired > 0) {
        appendRawLine(QStringLiteral("[TRACKER] expired %1 order(s)").arg(static_cast<qulonglong>(expired)));
    }
    if (!m_uiDiagnosticsActive) {
        perf.setField("ui_diagnostics_active", "0");
        return;
    }
    perf.setField("ui_diagnostics_active", "1");
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
