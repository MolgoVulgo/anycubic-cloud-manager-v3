#include "CloudBridgeSupport.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QUrl>

namespace accloud::cloud_bridge_support {

QString formatBytes(uint64_t bytes) {
    if (bytes >= uint64_t{1} << 30)
        return QString::number(bytes / double(uint64_t{1} << 30), 'f', 1) + " GB";
    if (bytes >= uint64_t{1} << 20)
        return QString::number(bytes / double(uint64_t{1} << 20), 'f', 1) + " MB";
    if (bytes >= uint64_t{1} << 10)
        return QString::number(bytes / double(uint64_t{1} << 10), 'f', 1) + " KB";
    return QString::number(bytes) + " B";
}

// ── Formatage statut ──────────────────────────────────────────────────────

QString formatStatus(int status) {
    switch (status) {
        case 1:  return QStringLiteral("READY");
        case 2:  return QStringLiteral("PROCESSING");
        default: return QStringLiteral("UNKNOWN");
    }
}

QVariantMap intMapToVariantMap(const std::map<std::string, int>& source) {
    QVariantMap out;
    for (const auto& [key, value] : source) {
        out.insert(QString::fromStdString(key), value);
    }
    return out;
}

QString formatUploadTime(long long updateTimeEpochSec) {
    if (updateTimeEpochSec <= 0)
        return {};
    qint64 epochSec = static_cast<qint64>(updateTimeEpochSec);
    if (epochSec > 1000000000000LL)  // defensive: epoch ms
        epochSec /= 1000;
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(epochSec).toLocalTime();
    if (!dt.isValid())
        return {};
    const QLocale locale = QLocale::system();
    QString value = locale.toString(dt.date(), QLocale::ShortFormat);
    if (value.isEmpty())
        value = dt.date().toString(QStringLiteral("yyyy-MM-dd"));
    return value;
}

QString normalizeUploadLocalPath(const QString& pathOrUrl) {
    const QString trimmed = pathOrUrl.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl parsed(trimmed);
    if (parsed.isValid() && parsed.isLocalFile()) {
        const QString localPath = parsed.toLocalFile().trimmed();
        if (!localPath.isEmpty()) {
            return localPath;
        }
    }

    if (trimmed.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        const QUrl fallback = QUrl::fromUserInput(trimmed);
        if (fallback.isValid() && fallback.isLocalFile()) {
            const QString localPath = fallback.toLocalFile().trimmed();
            if (!localPath.isEmpty()) {
                return localPath;
            }
        }
    }

    return trimmed;
}



std::string compactJsonFromVariantMap(const QVariantMap& data) {
    if (data.isEmpty()) {
        return {};
    }
    const QJsonObject object = QJsonObject::fromVariantMap(data);
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return json.toStdString();
}

// ── Conversion CloudFileInfo → QVariantMap ────────────────────────────────

QVariantMap fileInfoToMap(const cloud::CloudFileInfo& f) {
    const QString name = QString::fromStdString(f.name);
    const bool isPwmb  = name.endsWith(".pwmb", Qt::CaseInsensitive);
    bool layersOk = false;
    const int layersValue = QString::fromStdString(f.layers).toInt(&layersOk);

    QVariantMap m;
    m.insert("fileId",        QString::fromStdString(f.id));
    m.insert("fileName",      name);
    m.insert("status",        formatStatus(f.status));
    m.insert("statusCode",    f.status);
    m.insert("sizeBytes",     static_cast<qulonglong>(f.sizeBytes));
    m.insert("sizeText",      formatBytes(f.sizeBytes));
    m.insert("machine",       QString::fromStdString(f.machine));
    m.insert("printers",      QString::fromStdString(f.printers));
    m.insert("material",      QString::fromStdString(f.material));
    m.insert("createTime",    formatUploadTime(f.createTime));
    m.insert("createTimeEpoch", static_cast<qlonglong>(f.createTime));
    m.insert("updateTime",    formatUploadTime(f.updateTime));
    m.insert("uploadTime",    formatUploadTime(f.updateTime));
    m.insert("printTime",     QString::fromStdString(f.printTime));
    m.insert("layerThickness",QString::fromStdString(f.layerHeight));
    m.insert("layers",        layersOk ? layersValue : 0);
    m.insert("isPwmb",        isPwmb);
    m.insert("resinUsage",    QString::fromStdString(f.resinUsage));
    m.insert("dimensions",    QString::fromStdString(f.dimensions));
    m.insert("bottomLayers",  QString::fromStdString(f.bottomLayers));
    m.insert("exposureTime",  QString::fromStdString(f.exposureTime));
    m.insert("offTime",       QString::fromStdString(f.offTime));
    m.insert("md5",           QString::fromStdString(f.md5));
    m.insert("downloadUrl",   QString::fromStdString(f.downloadUrl));
    m.insert("region",        QString::fromStdString(f.region));
    m.insert("bucket",        QString::fromStdString(f.bucket));
    m.insert("path",          QString::fromStdString(f.path));
    const QString thumbnailSource = QString::fromStdString(f.thumbnailUrl);
    QStringList thumbnailCandidates;
    thumbnailCandidates.reserve(static_cast<qsizetype>(f.thumbnailCandidates.size()));
    for (const std::string& candidate : f.thumbnailCandidates) {
        const QString value = QString::fromStdString(candidate).trimmed();
        if (!value.isEmpty() && !thumbnailCandidates.contains(value)) {
            thumbnailCandidates.append(value);
        }
    }
    if (thumbnailCandidates.isEmpty() && !thumbnailSource.trimmed().isEmpty()) {
        thumbnailCandidates.append(thumbnailSource.trimmed());
    }
    m.insert("thumbnailCandidates", thumbnailCandidates);
    m.insert("thumbnailSourceUrl", thumbnailSource);
    m.insert("thumbnailUrl", thumbnailSource);
    m.insert("gcodeId",       QString::fromStdString(f.gcodeId));
    return m;
}

QVariantMap printerInfoToMap(const cloud::CloudPrinterInfo& p) {
    QVariantMap m;
    m.insert("id",          QString::fromStdString(p.id));
    m.insert("printerKey",  QString::fromStdString(p.printerKey));
    m.insert("machineType", QString::fromStdString(p.machineType));
    m.insert("name",        QString::fromStdString(p.name));
    m.insert("model",       QString::fromStdString(p.model));
    m.insert("type",        QString::fromStdString(p.type));
    m.insert("lastSeen",    QString::fromStdString(p.lastSeen));
    m.insert("state",       QString::fromStdString(p.state));
    m.insert("reason",      QString::fromStdString(p.reason));
    m.insert("available",   p.available);
    m.insert("progress",    p.progress);
    m.insert("elapsedSec",  p.elapsedSec);
    m.insert("remainingSec",p.remainingSec);
    m.insert("currentLayer",p.currentLayer);
    m.insert("totalLayers", p.totalLayers);
    m.insert("currentFile", QString::fromStdString(p.currentFile));
    m.insert("mqttActiveTaskId", QString::fromStdString(p.mqttActiveTaskId));
    m.insert("mqttPrintState", QString::fromStdString(p.mqttPrintState));
    m.insert("mqttJobStage", QString::fromStdString(p.mqttJobStage));
    m.insert("mqttDownloadProgress", p.mqttDownloadProgress);
    m.insert("mqttResinStatus", QString::fromStdString(p.mqttResinStatus));
    m.insert("mqttResinMessage", QString::fromStdString(p.mqttResinMessage));
    m.insert("mqttResinBlocking", p.mqttResinBlocking);
    QVariantMap details;
    if (!p.mqttActiveTaskId.empty()) {
        details.insert(QStringLiteral("mqttActiveTaskId"), QString::fromStdString(p.mqttActiveTaskId));
    }
    if (!p.mqttPrintState.empty()) {
        details.insert(QStringLiteral("mqttPrintState"), QString::fromStdString(p.mqttPrintState));
    }
    if (!p.mqttJobStage.empty()) {
        details.insert(QStringLiteral("mqttJobStage"), QString::fromStdString(p.mqttJobStage));
    }
    if (p.mqttDownloadProgress >= 0) {
        details.insert(QStringLiteral("mqttDownloadProgress"), p.mqttDownloadProgress);
    }
    if (!p.mqttHardwareChecks.empty()) {
        details.insert(QStringLiteral("mqttHardwareChecks"), intMapToVariantMap(p.mqttHardwareChecks));
    }
    if (!p.mqttAutoChecks.empty()) {
        details.insert(QStringLiteral("mqttAutoChecks"), intMapToVariantMap(p.mqttAutoChecks));
    }
    if (!p.mqttResinStatus.empty()) {
        details.insert(QStringLiteral("mqttResinStatus"), QString::fromStdString(p.mqttResinStatus));
    }
    if (!p.mqttResinMessage.empty()) {
        details.insert(QStringLiteral("mqttResinMessage"), QString::fromStdString(p.mqttResinMessage));
    }
    if (!p.mqttResinPhase.empty()) {
        details.insert(QStringLiteral("mqttResinPhase"), QString::fromStdString(p.mqttResinPhase));
    }
    if (!p.mqttResinPrePrintFillStatus.empty()) {
        details.insert(QStringLiteral("mqttResinPrePrintFillStatus"),
                       QString::fromStdString(p.mqttResinPrePrintFillStatus));
    }
    if (!p.mqttResinRuntimeTopupStatus.empty()) {
        details.insert(QStringLiteral("mqttResinRuntimeTopupStatus"),
                       QString::fromStdString(p.mqttResinRuntimeTopupStatus));
    }
    if (!p.mqttResinBottleStatus.empty()) {
        details.insert(QStringLiteral("mqttResinBottleStatus"), QString::fromStdString(p.mqttResinBottleStatus));
    }
    if (!p.mqttResinVatStatus.empty()) {
        details.insert(QStringLiteral("mqttResinVatStatus"), QString::fromStdString(p.mqttResinVatStatus));
    }
    if (p.mqttResinLastFeedCode >= 0) {
        details.insert(QStringLiteral("mqttResinLastFeedCode"), p.mqttResinLastFeedCode);
    }
    details.insert(QStringLiteral("mqttResinBlocking"), p.mqttResinBlocking);
    m.insert("details", details);
    return m;
}

QVariantMap printerCompatToMap(const cloud::CloudPrinterCompatItem& p) {
    QVariantMap m;
    m.insert("id",        QString::fromStdString(p.id));
    m.insert("available", p.available);
    m.insert("reason",    QString::fromStdString(p.reason));
    return m;
}

void applyRealtimeOverlayToPrinterMap(
    QVariantMap& printer,
    const std::map<std::string, accloud::realtime::PrinterRealtimeSnapshot>& snapshots) {
    const QString printerId = printer.value(QStringLiteral("id")).toString().trimmed();
    const QString printerKey = printer.value(QStringLiteral("printerKey")).toString().trimmed();

    auto it = snapshots.find(printerId.toStdString());
    if (it == snapshots.end() && !printerKey.isEmpty()) {
        it = snapshots.find(printerKey.toStdString());
    }
    if (it == snapshots.end()) {
        return;
    }

    const auto& rt = it->second;
    if (rt.state.has_value()) {
        printer.insert(QStringLiteral("state"), QString::fromStdString(*rt.state));
    }
    if (rt.activeTaskId.has_value()) {
        printer.insert(QStringLiteral("mqttActiveTaskId"), QString::fromStdString(*rt.activeTaskId));
        QVariantMap details = printer.value(QStringLiteral("details")).toMap();
        details.insert(QStringLiteral("mqttActiveTaskId"), QString::fromStdString(*rt.activeTaskId));
        printer.insert(QStringLiteral("details"), details);
    }
    if (rt.printStateText.has_value()) {
        printer.insert(QStringLiteral("mqttPrintState"), QString::fromStdString(*rt.printStateText));
        QVariantMap details = printer.value(QStringLiteral("details")).toMap();
        details.insert(QStringLiteral("mqttPrintState"), QString::fromStdString(*rt.printStateText));
        printer.insert(QStringLiteral("details"), details);
    }
    if (rt.jobStageText.has_value()) {
        printer.insert(QStringLiteral("mqttJobStage"), QString::fromStdString(*rt.jobStageText));
        QVariantMap details = printer.value(QStringLiteral("details")).toMap();
        details.insert(QStringLiteral("mqttJobStage"), QString::fromStdString(*rt.jobStageText));
        printer.insert(QStringLiteral("details"), details);
    }
    if (rt.downloadProgress.has_value()) {
        printer.insert(QStringLiteral("mqttDownloadProgress"), *rt.downloadProgress);
        QVariantMap details = printer.value(QStringLiteral("details")).toMap();
        details.insert(QStringLiteral("mqttDownloadProgress"), *rt.downloadProgress);
        printer.insert(QStringLiteral("details"), details);
    }
    if (rt.resin.uiStatus.has_value()
        || rt.resin.message.has_value()
        || rt.resin.phase.has_value()
        || rt.resin.lastFeedResinCode.has_value()) {
        QVariantMap details = printer.value(QStringLiteral("details")).toMap();
        if (rt.resin.uiStatus.has_value()) {
            printer.insert(QStringLiteral("mqttResinStatus"), QString::fromStdString(*rt.resin.uiStatus));
            details.insert(QStringLiteral("mqttResinStatus"), QString::fromStdString(*rt.resin.uiStatus));
        }
        if (rt.resin.message.has_value()) {
            printer.insert(QStringLiteral("mqttResinMessage"), QString::fromStdString(*rt.resin.message));
            details.insert(QStringLiteral("mqttResinMessage"), QString::fromStdString(*rt.resin.message));
        }
        if (rt.resin.phase.has_value()) {
            details.insert(QStringLiteral("mqttResinPhase"), QString::fromStdString(*rt.resin.phase));
        }
        if (rt.resin.prePrintFillStatus.has_value()) {
            details.insert(QStringLiteral("mqttResinPrePrintFillStatus"),
                           QString::fromStdString(*rt.resin.prePrintFillStatus));
        }
        if (rt.resin.runtimeTopupStatus.has_value()) {
            details.insert(QStringLiteral("mqttResinRuntimeTopupStatus"),
                           QString::fromStdString(*rt.resin.runtimeTopupStatus));
        }
        if (rt.resin.bottleStatus.has_value()) {
            details.insert(QStringLiteral("mqttResinBottleStatus"), QString::fromStdString(*rt.resin.bottleStatus));
        }
        if (rt.resin.vatStatus.has_value()) {
            details.insert(QStringLiteral("mqttResinVatStatus"), QString::fromStdString(*rt.resin.vatStatus));
        }
        if (rt.resin.lastFeedResinCode.has_value()) {
            details.insert(QStringLiteral("mqttResinLastFeedCode"), *rt.resin.lastFeedResinCode);
        }
        if (rt.resin.blockingPrint.has_value()) {
            printer.insert(QStringLiteral("mqttResinBlocking"), *rt.resin.blockingPrint);
            details.insert(QStringLiteral("mqttResinBlocking"), *rt.resin.blockingPrint);
        }
        printer.insert(QStringLiteral("details"), details);
    }
    if (rt.activeTaskId.has_value()) {
        const auto jobIt = rt.jobs.find(*rt.activeTaskId);
        if (jobIt != rt.jobs.end()) {
            QVariantMap details = printer.value(QStringLiteral("details")).toMap();
            if (!jobIt->second.hardwareChecks.empty()) {
                details.insert(QStringLiteral("mqttHardwareChecks"), intMapToVariantMap(jobIt->second.hardwareChecks));
            }
            if (!jobIt->second.autoChecks.empty()) {
                details.insert(QStringLiteral("mqttAutoChecks"), intMapToVariantMap(jobIt->second.autoChecks));
            }
            printer.insert(QStringLiteral("details"), details);
        }
    }
    if (rt.progress.has_value()) {
        printer.insert(QStringLiteral("progress"), *rt.progress);
    }
    if (rt.elapsedSec.has_value()) {
        printer.insert(QStringLiteral("elapsedSec"), *rt.elapsedSec);
    }
    if (rt.remainingSec.has_value()) {
        printer.insert(QStringLiteral("remainingSec"), *rt.remainingSec);
    }
    if (rt.currentLayer.has_value()) {
        printer.insert(QStringLiteral("currentLayer"), *rt.currentLayer);
    }
    if (rt.totalLayers.has_value()) {
        printer.insert(QStringLiteral("totalLayers"), *rt.totalLayers);
    }
    if (rt.currentFile.has_value()) {
        printer.insert(QStringLiteral("currentFile"), QString::fromStdString(*rt.currentFile));
    }
    if (rt.reason.has_value()) {
        printer.insert(QStringLiteral("reason"), QString::fromStdString(*rt.reason));
    }
    if (rt.releaseFilmStatus.has_value()) {
        QVariantMap details = printer.value(QStringLiteral("details")).toMap();
        details.insert(QStringLiteral("releaseFilmStatus"), QString::fromStdString(*rt.releaseFilmStatus));
        printer.insert(QStringLiteral("details"), details);
    }
    if (rt.releaseFilmLayers.has_value()
        || rt.releaseFilmTimes.has_value()
        || rt.releaseFilmStatusCode.has_value()) {
        QVariantMap details = printer.value(QStringLiteral("details")).toMap();
        if (rt.releaseFilmLayers.has_value()) {
            details.insert(QStringLiteral("releaseFilmLayers"), *rt.releaseFilmLayers);
        }
        if (rt.releaseFilmTimes.has_value()) {
            details.insert(QStringLiteral("releaseFilmTimes"), *rt.releaseFilmTimes);
        }
        if (rt.releaseFilmStatusCode.has_value()) {
            details.insert(QStringLiteral("releaseFilmStatusCode"), *rt.releaseFilmStatusCode);
        }
        printer.insert(QStringLiteral("details"), details);
    }
}

QVariantMap printerDetailsToMap(const cloud::CloudPrinterDetailsResult& d) {
    QVariantMap m;
    m.insert("progress", d.progress);
    m.insert("elapsedSec", d.elapsedSec);
    m.insert("remainingSec", d.remainingSec);
    m.insert("currentLayer", d.currentLayer);
    m.insert("totalLayers", d.totalLayers);
    m.insert("currentFile", QString::fromStdString(d.currentFile));
    m.insert("firmwareVersion", QString::fromStdString(d.firmwareVersion));
    m.insert("printCount", QString::fromStdString(d.printCount));
    m.insert("printTotalTime", QString::fromStdString(d.printTotalTime));
    m.insert("materialType", QString::fromStdString(d.materialType));
    m.insert("materialUsed", QString::fromStdString(d.materialUsed));
    m.insert("machineMac", QString::fromStdString(d.machineMac));
    m.insert("helpUrl", QString::fromStdString(d.helpUrl));
    m.insert("quickStartUrl", QString::fromStdString(d.quickStartUrl));
    m.insert("releaseFilmStatus", QString::fromStdString(d.releaseFilmStatus));
    m.insert("releaseFilmLayers", QString::fromStdString(d.releaseFilmLayers));

    QVariantList tools;
    tools.reserve(static_cast<qsizetype>(d.tools.size()));
    for (const auto& t : d.tools)
        tools.append(QString::fromStdString(t));
    m.insert("tools", tools);

    QVariantList advances;
    advances.reserve(static_cast<qsizetype>(d.advances.size()));
    for (const auto& a : d.advances)
        advances.append(QString::fromStdString(a));
    m.insert("advances", advances);
    return m;
}

QVariantMap reasonCatalogItemToMap(const cloud::CloudReasonCatalogItem& item) {
    QVariantMap m;
    m.insert("reason", item.reason);
    m.insert("desc", QString::fromStdString(item.desc));
    m.insert("helpUrl", QString::fromStdString(item.helpUrl));
    m.insert("type", QString::fromStdString(item.type));
    m.insert("push", item.push);
    m.insert("popup", item.popup);
    return m;
}

QVariantMap printerProjectToMap(const cloud::CloudPrinterProjectItem& item) {
    QVariantMap m;
    m.insert("taskId", QString::fromStdString(item.taskId));
    m.insert("cloudFileId", QString::fromStdString(item.cloudFileId));
    m.insert("gcodeId", QString::fromStdString(item.gcodeId));
    m.insert("gcodeName", QString::fromStdString(item.gcodeName));
    m.insert("printerId", QString::fromStdString(item.printerId));
    m.insert("printerName", QString::fromStdString(item.printerName));
    m.insert("printStatus", item.printStatus);
    m.insert("progress", item.progress);
    m.insert("elapsedSec", item.elapsedSec);
    m.insert("remainingSec", item.remainingSec);
    m.insert("currentLayer", item.currentLayer);
    m.insert("totalLayers", item.totalLayers);
    m.insert("currentFile", QString::fromStdString(item.currentFile));
    m.insert("reason", QString::fromStdString(item.reason));
    m.insert("createTime", static_cast<qlonglong>(item.createTime));
    m.insert("endTime", static_cast<qlonglong>(item.endTime));
    const QString rawImg = QString::fromStdString(item.img);
    m.insert("img", QString{});
    m.insert("imgRaw", rawImg);
    return m;
}

void finalizeUiMessage(QVariantMap& out) {
    if (!out.contains("message")) {
        return;
    }
    const QString message = out.value("message").toString().trimmed();
    const QString lowered = message.toLower();
    const bool ok = out.value("ok").toBool();

    if (!out.contains("messageKey")) {
        QString key = ok ? QStringLiteral("info.ok") : QStringLiteral("error.generic");
        if (lowered.contains("session")) {
            key = QStringLiteral("error.session.invalid");
        } else if (lowered.contains("network") || lowered.contains("réseau")
                   || lowered.contains("reseau")) {
            key = QStringLiteral("error.network");
        } else if (lowered.contains("cache")) {
            key = ok ? QStringLiteral("info.cache") : QStringLiteral("error.cache");
        } else if (lowered.contains("compat")) {
            key = QStringLiteral("error.compatibility");
        } else if (lowered.contains("download") || lowered.contains("url")) {
            key = ok ? QStringLiteral("info.download") : QStringLiteral("error.download");
        } else if (lowered.contains("upload")) {
            key = ok ? QStringLiteral("info.upload") : QStringLiteral("error.upload");
        } else if (lowered.contains("print")) {
            key = ok ? QStringLiteral("info.print") : QStringLiteral("error.print");
        } else if (lowered.contains("quota")) {
            key = ok ? QStringLiteral("info.quota") : QStringLiteral("error.quota");
        } else if (lowered.contains("printer")) {
            key = ok ? QStringLiteral("info.printer") : QStringLiteral("error.printer");
        } else if (lowered.contains("file")) {
            key = ok ? QStringLiteral("info.file") : QStringLiteral("error.file");
        }
        out.insert("messageKey", key);
    }
    if (!out.contains("fallbackMessage")) {
        out.insert("fallbackMessage", message);
    }
    if (!out.contains("params")) {
        out.insert("params", QVariantMap{});
    }
    if (message.isEmpty()) {
        out.insert("message", out.value("fallbackMessage").toString());
    }
}

} // namespace accloud::cloud_bridge_support
