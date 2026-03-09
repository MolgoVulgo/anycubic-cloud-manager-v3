#include "CloudBridge.h"

#include "LocalCacheStore.h"
#include "app/usecases/cloud/DeleteCloudFileUseCase.h"
#include "app/usecases/cloud/FetchPrinterCompatibilityByExtUseCase.h"
#include "app/usecases/cloud/FetchPrinterCompatibilityByFileIdUseCase.h"
#include "app/usecases/cloud/FetchPrinterDetailsUseCase.h"
#include "app/usecases/cloud/FetchPrinterProjectsUseCase.h"
#include "app/usecases/cloud/FetchReasonCatalogUseCase.h"
#include "app/usecases/cloud/GetDownloadUrlUseCase.h"
#include "app/usecases/cloud/LoadCloudFilesUseCase.h"
#include "app/usecases/cloud/LoadCloudQuotaUseCase.h"
#include "app/usecases/cloud/LoadPrintersDashboardUseCase.h"
#include "app/usecases/cloud/SendPrintOrderUseCase.h"
#include "infra/cloud/HarImporter.h"
#include "infra/debug/DebugBuild.h"
#include "infra/logging/JsonlLogger.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QSaveFile>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantList>
#include <QCryptographicHash>

#include <cmath>
#include <string>
#include <thread>

namespace accloud {
namespace {

constexpr bool kDebugBuildEnabled = debug::kEnabled;

// ── Formatage taille ──────────────────────────────────────────────────────

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

// ── Conversion CloudFileInfo → QVariantMap ────────────────────────────────

QVariantMap fileInfoToMap(const cloud::CloudFileInfo& f) {
    const QString name = QString::fromStdString(f.name);
    const bool isPwmb  = name.endsWith(".pwmb", Qt::CaseInsensitive)
                      || name.endsWith(".pwmb", Qt::CaseInsensitive);
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
    m.insert("thumbnailUrl",  QString::fromStdString(f.thumbnailUrl));
    m.insert("gcodeId",       QString::fromStdString(f.gcodeId));
    return m;
}

QString normalizedThumbnailUrl(const QString& raw) {
    const QString value = raw.trimmed();
    if (value.isEmpty()) {
        return {};
    }
    if (value.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || value.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return value;
    }
    if (value.startsWith(QStringLiteral("//"))) {
        return QStringLiteral("https:") + value;
    }
    if (value.startsWith(QStringLiteral("/"))) {
        return QStringLiteral("https://cloud-universe.anycubic.com") + value;
    }
    if (value.contains(QStringLiteral(".jpg"), Qt::CaseInsensitive)
        || value.contains(QStringLiteral(".jpeg"), Qt::CaseInsensitive)
        || value.contains(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        return QStringLiteral("https://") + value;
    }
    return {};
}

QString thumbnailCacheDirPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        return {};
    }
    const QString dir = base + QStringLiteral("/thumbnails");
    QDir().mkpath(dir);
    return dir;
}

QString cacheBasePathForThumbnailUrl(const QString& url) {
    const QString dir = thumbnailCacheDirPath();
    if (dir.isEmpty()) {
        return {};
    }
    const QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    return dir + QStringLiteral("/") + QString::fromLatin1(hash);
}

QString detectImageExtension(const QByteArray& bytes, const QString& fallbackUrl) {
    if (bytes.size() >= 4
        && static_cast<unsigned char>(bytes[0]) == 0x89
        && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
        return QStringLiteral(".png");
    }
    if (bytes.size() >= 3
        && static_cast<unsigned char>(bytes[0]) == 0xFF
        && static_cast<unsigned char>(bytes[1]) == 0xD8
        && static_cast<unsigned char>(bytes[2]) == 0xFF) {
        return QStringLiteral(".jpg");
    }
    if (bytes.startsWith("RIFF") && bytes.size() >= 12 && bytes.mid(8, 4) == "WEBP") {
        return QStringLiteral(".webp");
    }
    const QString lowered = fallbackUrl.toLower();
    if (lowered.contains(QStringLiteral(".png"))) {
        return QStringLiteral(".png");
    }
    if (lowered.contains(QStringLiteral(".jpeg"))) {
        return QStringLiteral(".jpeg");
    }
    return QStringLiteral(".jpg");
}

QByteArray readHead(const QString& path, qint64 maxBytes = 32) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.read(maxBytes);
}

QString findReadableCachedImage(const QString& cacheBasePath) {
    static const QStringList exts = {
        QStringLiteral(".png"),
        QStringLiteral(".jpg"),
        QStringLiteral(".jpeg"),
        QStringLiteral(".webp")
    };
    for (const QString& ext : exts) {
        const QString path = cacheBasePath + ext;
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile() || info.size() <= 0) {
            continue;
        }
        const QByteArray head = readHead(path);
        const QString detectedExt = detectImageExtension(head, path);
        if (!detectedExt.isEmpty() && detectedExt != ext) {
            const QString migratedPath = cacheBasePath + detectedExt;
            if (!QFileInfo::exists(migratedPath)) {
                QFile::copy(path, migratedPath);
            }
            QImageReader migratedReader(migratedPath);
            if (migratedReader.canRead()) {
                return migratedPath;
            }
        }
        QImageReader reader(path);
        if (reader.canRead()) {
            return path;
        }
    }
    return {};
}

QString fetchThumbnailToCache(const QString& normalizedUrl, const QString& cacheBasePath) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(normalizedUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QEventLoop loop;
    QNetworkReply* reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray bytes = reply->readAll();
    const bool ok = (reply->error() == QNetworkReply::NoError) && !bytes.isEmpty();
    reply->deleteLater();
    if (!ok) {
        return {};
    }

    const QString ext = detectImageExtension(bytes, normalizedUrl);
    const QString finalPath = cacheBasePath + ext;
    QSaveFile file(finalPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        file.cancelWriting();
        return {};
    }
    QImageReader reader(finalPath);
    if (!reader.canRead()) {
        return {};
    }
    return QUrl::fromLocalFile(finalPath).toString();
}

QString resolveThumbnailLocalUrl(const QString& source, bool downloadMissing) {
    const QString normalized = normalizedThumbnailUrl(source);
    if (normalized.isEmpty()) {
        if (!source.trimmed().isEmpty()) {
            logging::warn("app", "thumbnail_cache", "unsupported_source",
                          "Unsupported thumbnail source",
                          {{"source", source.toStdString()}});
        }
        return {};
    }
    const QString cacheBasePath = cacheBasePathForThumbnailUrl(normalized);
    if (cacheBasePath.isEmpty()) {
        return normalized;
    }
    const QString cachedPath = findReadableCachedImage(cacheBasePath);
    if (!cachedPath.isEmpty()) {
        logging::info("app", "thumbnail_cache", "cache_hit",
                      "Thumbnail served from local cache",
                      {{"url", normalized.toStdString()},
                       {"path", cachedPath.toStdString()}});
        return QUrl::fromLocalFile(cachedPath).toString();
    }
    logging::info("app", "thumbnail_cache", "cache_miss",
                  "Thumbnail not found in local cache",
                  {{"url", normalized.toStdString()},
                   {"path", cacheBasePath.toStdString()}});
    if (!downloadMissing) {
        return normalized;
    }
    const QString localUrl = fetchThumbnailToCache(normalized, cacheBasePath);
    if (localUrl.isEmpty()) {
        logging::warn("app", "thumbnail_cache", "download_failed",
                      "Thumbnail download failed",
                      {{"url", normalized.toStdString()}});
        return normalized;
    }
    logging::info("app", "thumbnail_cache", "download_ok",
                  "Thumbnail downloaded and cached",
                  {{"url", normalized.toStdString()},
                   {"path", QUrl(localUrl).toLocalFile().toStdString()}});
    return localUrl;
}

void resolveThumbnailInMap(QVariantMap& map, bool downloadMissing) {
    const QString source = map.value(QStringLiteral("thumbnailUrl")).toString();
    const QString resolved = resolveThumbnailLocalUrl(source, downloadMissing);
    if (!resolved.isEmpty()) {
        const QString localPath = QUrl(resolved).toLocalFile();
        if (!localPath.isEmpty()) {
            QImageReader reader(localPath);
            logging::info("app", "thumbnail_cache", "qml_probe",
                          "Thumbnail path probe before QML bind",
                          {{"fileId", map.value(QStringLiteral("fileId")).toString().toStdString()},
                           {"path", localPath.toStdString()},
                           {"canRead", reader.canRead() ? "1" : "0"},
                           {"error", reader.errorString().toStdString()}});
        }
        map.insert(QStringLiteral("thumbnailUrl"), resolved);
    }
}

QVariantMap printerInfoToMap(const cloud::CloudPrinterInfo& p) {
    QVariantMap m;
    m.insert("id",          QString::fromStdString(p.id));
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
    return m;
}

QVariantMap printerCompatToMap(const cloud::CloudPrinterCompatItem& p) {
    QVariantMap m;
    m.insert("id",        QString::fromStdString(p.id));
    m.insert("available", p.available);
    m.insert("reason",    QString::fromStdString(p.reason));
    return m;
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
    m.insert("img", QString::fromStdString(item.img));
    return m;
}

QVariantList collectJobsFromPrinters(const QVariantList& printers) {
    QVariantList jobs;
    for (const QVariant& printerVariant : printers) {
        const QVariantMap printer = printerVariant.toMap();
        const QString printerId = printer.value(QStringLiteral("id")).toString();
        const QString printerName = printer.value(QStringLiteral("name")).toString();
        const QVariantList projects = printer.value(QStringLiteral("projects")).toList();
        for (const QVariant& projectVariant : projects) {
            QVariantMap job = projectVariant.toMap();
            if (job.value(QStringLiteral("printerId")).toString().trimmed().isEmpty()) {
                job.insert(QStringLiteral("printerId"), printerId);
            }
            if (job.value(QStringLiteral("printerName")).toString().trimmed().isEmpty()) {
                job.insert(QStringLiteral("printerName"), printerName);
            }
            jobs.append(job);
        }
    }
    return jobs;
}

void finalizeUiMessage(QVariantMap& out) {
    if (!out.contains("message")) {
        return;
    }
    const QString message = out.value("message").toString().trimmed();
    const QString lowered = message.toLower();
    const bool ok = out.value("ok").toBool();

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

} // namespace

// ── Constructeur / destructeur ────────────────────────────────────────────

CloudBridge::CloudBridge(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_cache(new LocalCacheStore()) {}

CloudBridge::~CloudBridge() {
    cleanupDownload();
    // m_cache intentionally kept alive until process teardown to avoid races
    // with detached refresh tasks during app shutdown.
}

bool CloudBridge::shouldRefresh(const QString& scope, int ttlSec, bool force) const {
    if (force || ttlSec <= 0 || m_cache == nullptr) {
        return true;
    }

    const auto state = m_cache->syncState(scope);
    if (!state.has_value() || !state->hasSuccess || state->lastSuccessAt <= 0) {
        return true;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return (now - state->lastSuccessAt) >= ttlSec;
}

QVariantList CloudBridge::fetchFilesWithRetry(int page, int limit, QString& message, bool& ok, bool downloadThumbnails) const {
    ok = false;
    QVariantList files;
    const usecases::cloud::LoadCloudFilesUseCase useCase;
    const usecases::cloud::LoadCloudFilesResult result = useCase.execute(page, limit);
    message = QString::fromStdString(result.message);
    ok = result.ok;
    if (!result.ok) {
        return files;
    }

    files.reserve(static_cast<qsizetype>(result.files.size()));
    for (const auto& f : result.files) {
        QVariantMap item = fileInfoToMap(f);
        resolveThumbnailInMap(item, downloadThumbnails);
        files.append(item);
    }
    return files;
}

QVariantList CloudBridge::fetchPrintersWithRetry(QString& message, bool& ok, QString& rawJson) const {
    ok = false;
    rawJson.clear();
    QVariantList printers;
    const usecases::cloud::LoadPrintersDashboardUseCase useCase;
    const usecases::cloud::LoadPrintersDashboardResult result = useCase.execute();
    message = QString::fromStdString(result.message);
    if constexpr (kDebugBuildEnabled) {
        rawJson = QString::fromStdString(result.rawJson);
    } else {
        rawJson.clear();
    }
    ok = result.ok;
    if (!result.ok) {
        return printers;
    }

    printers.reserve(static_cast<qsizetype>(result.printers.size()));
    for (const auto& p : result.printers) {
        printers.append(printerInfoToMap(p));
    }
    return printers;
}

QVariantMap CloudBridge::fetchQuotaWithRetry(QString& message, bool& ok) const {
    ok = false;
    QVariantMap out;
    const usecases::cloud::LoadCloudQuotaUseCase useCase;
    const cloud::CloudQuotaResult q = useCase.execute();

    message = QString::fromStdString(q.message);
    ok = q.ok;
    if (!q.ok) {
        finalizeUiMessage(out);
        return out;
    }

    out.insert("totalDisplay", QString::fromStdString(q.totalDisplay));
    out.insert("usedDisplay", QString::fromStdString(q.usedDisplay));
    out.insert("totalBytes", static_cast<qulonglong>(q.totalBytes));
    out.insert("usedBytes", static_cast<qulonglong>(q.usedBytes));
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::loadCachedFiles(int page, int limit) const {
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    out.insert("files", QVariantList{});
    out.insert("total", 0);

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    QVariantList files = m_cache->loadFiles(page, limit);
    for (int i = 0; i < files.size(); ++i) {
        QVariantMap item = files[i].toMap();
        resolveThumbnailInMap(item, false);
        files[i] = item;
    }
    out.insert("ok", true);
    out.insert("message", files.isEmpty()
                             ? QStringLiteral("Aucune donnée cache.")
                             : QStringLiteral("Fichiers chargés depuis le cache local."));
    out.insert("files", files);
    out.insert("total", files.size());
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::loadCachedPrinters() const {
    QVariantMap out;
    if constexpr (kDebugBuildEnabled) {
        out.insert("endpoint", QStringLiteral(
            "/p/p/workbench/api/work/printer/getPrinters + "
            "/p/p/workbench/api/work/project/getProjects?printer_id=<id>&print_status=1"));
    }
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString{});
    }
    out.insert("printers", QVariantList{});

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    const QVariantList cachedPrinters = m_cache->loadPrinters();
    QVariantList printers;
    printers.reserve(cachedPrinters.size());
    for (const QVariant& item : cachedPrinters) {
        QVariantMap printer = item.toMap();
        const QString printerId = printer.value(QStringLiteral("id")).toString();
        printer.insert(QStringLiteral("progress"), -1);
        printer.insert(QStringLiteral("elapsedSec"), -1);
        printer.insert(QStringLiteral("remainingSec"), -1);
        printer.insert(QStringLiteral("currentLayer"), -1);
        printer.insert(QStringLiteral("totalLayers"), -1);
        printer.insert(QStringLiteral("details"), QVariantMap{});
        const QVariantList cachedProjects = m_cache->loadJobsForPrinter(printerId, 1, 20);
        if (!cachedProjects.isEmpty()) {
            const QVariantMap firstProject = cachedProjects.first().toMap();
            const QString firstName = firstProject.value(QStringLiteral("currentFile")).toString().trimmed().isEmpty()
                    ? firstProject.value(QStringLiteral("gcodeName")).toString()
                    : firstProject.value(QStringLiteral("currentFile")).toString();
            if (!firstName.trimmed().isEmpty()) {
                printer.insert(QStringLiteral("currentFile"), firstName);
            }
            const int firstProgress = firstProject.value(QStringLiteral("progress"), -1).toInt();
            if (firstProgress >= 0) {
                printer.insert(QStringLiteral("progress"), firstProgress);
            }
            const int firstElapsedSec = firstProject.value(QStringLiteral("elapsedSec"), -1).toInt();
            if (firstElapsedSec >= 0) {
                printer.insert(QStringLiteral("elapsedSec"), firstElapsedSec);
            }
            const int firstRemainingSec = firstProject.value(QStringLiteral("remainingSec"), -1).toInt();
            if (firstRemainingSec >= 0) {
                printer.insert(QStringLiteral("remainingSec"), firstRemainingSec);
            }
            const int firstCurrentLayer = firstProject.value(QStringLiteral("currentLayer"), -1).toInt();
            if (firstCurrentLayer >= 0) {
                printer.insert(QStringLiteral("currentLayer"), firstCurrentLayer);
            }
            const int firstTotalLayers = firstProject.value(QStringLiteral("totalLayers"), -1).toInt();
            if (firstTotalLayers >= 0) {
                printer.insert(QStringLiteral("totalLayers"), firstTotalLayers);
            }
        }
        if constexpr (kDebugBuildEnabled) {
            printer.insert(QStringLiteral("detailsRawJson"), QString{});
            printer.insert(QStringLiteral("projectsRawJson"), QString{});
        }
        printer.insert(QStringLiteral("projects"), cachedProjects);
        printers.append(printer);
    }
    out.insert("ok", true);
    out.insert("message", printers.isEmpty()
                             ? QStringLiteral("Aucune imprimante en cache.")
                             : QStringLiteral("Imprimantes chargées depuis le cache local."));
    out.insert("printers", printers);
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::loadCachedQuota() const {
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache quota indisponible."));
    out.insert("totalDisplay", QString{});
    out.insert("usedDisplay", QString{});
    out.insert("totalBytes", static_cast<qulonglong>(0));
    out.insert("usedBytes", static_cast<qulonglong>(0));

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    const QVariantMap quota = m_cache->loadQuota();
    if (quota.isEmpty()) {
        out.insert("ok", true);
        out.insert("message", QStringLiteral("Aucun quota en cache."));
        finalizeUiMessage(out);
        return out;
    }

    out.insert("ok", true);
    out.insert("message", QStringLiteral("Quota chargé depuis le cache local."));
    out.insert("totalDisplay", quota.value("totalDisplay"));
    out.insert("usedDisplay", quota.value("usedDisplay"));
    out.insert("totalBytes", quota.value("totalBytes"));
    out.insert("usedBytes", quota.value("usedBytes"));
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::refreshFilesAsync(int page, int limit, bool force) {
    if (m_refreshFilesRunning.exchange(true)) {
        return;
    }

    std::thread([this, page, limit, force]() {
        QString message;
        bool ok = false;

        if (!shouldRefresh(QStringLiteral("files"), 120, force)) {
            m_refreshFilesRunning.store(false);
            return;
        }

        const QVariantList files = fetchFilesWithRetry(page, limit, message, ok, true);
        if (m_cache != nullptr) {
            m_cache->updateSyncState(QStringLiteral("files"), ok, message);
        }

        if (ok && m_cache != nullptr) {
            m_cache->replaceFiles(files);
            QMetaObject::invokeMethod(this, [this, files]() {
                emit filesUpdatedFromCloud(files, QStringLiteral("Cloud refresh terminé."));
            }, Qt::QueuedConnection);

            QString quotaMessage;
            bool quotaOk = false;
            QVariantMap quota = fetchQuotaWithRetry(quotaMessage, quotaOk);
            if (m_cache != nullptr) {
                m_cache->updateSyncState(QStringLiteral("quota"), quotaOk, quotaMessage);
            }
            if (quotaOk && m_cache != nullptr) {
                m_cache->saveQuota(quota);
                QMetaObject::invokeMethod(this, [this, quota]() {
                    emit quotaUpdatedFromCloud(quota, QStringLiteral("Quota rafraîchi depuis le cloud."));
                }, Qt::QueuedConnection);
            } else if (!quotaOk) {
                QMetaObject::invokeMethod(this, [this, quotaMessage]() {
                    emit syncFailed(QStringLiteral("quota"), quotaMessage);
                }, Qt::QueuedConnection);
            }
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("files"), message);
            }, Qt::QueuedConnection);
        }

        m_refreshFilesRunning.store(false);
    }).detach();
}

void CloudBridge::refreshPrintersAsync(bool force) {
    if (m_refreshPrintersRunning.exchange(true)) {
        return;
    }

    std::thread([this, force]() {
        QString message;
        QString rawJson;
        bool ok = false;

        if (!shouldRefresh(QStringLiteral("printers"), 30, force)) {
            m_refreshPrintersRunning.store(false);
            return;
        }

        const QVariantList printers = fetchPrintersWithRetry(message, ok, rawJson);
        if (m_cache != nullptr) {
            m_cache->updateSyncState(QStringLiteral("printers"), ok, message);
        }

        if (ok && m_cache != nullptr) {
            m_cache->replacePrinters(printers);
            m_cache->replaceJobs(collectJobsFromPrinters(printers));
            QMetaObject::invokeMethod(this, [this, printers]() {
                emit printersUpdatedFromCloud(printers, QStringLiteral("Cloud refresh imprimantes terminé."));
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("printers"), message);
            }, Qt::QueuedConnection);
        }

        m_refreshPrintersRunning.store(false);
    }).detach();
}

// ── fetchFiles ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchFiles(int page, int limit) const {
    QVariantMap out;
    QString message;
    bool ok = false;
    const QVariantList files = fetchFilesWithRetry(page, limit, message, ok, false);

    out.insert("ok", ok);
    out.insert("message", message);
    out.insert("total", files.size());
    out.insert("files", files);

    if (m_cache != nullptr) {
        m_cache->updateSyncState(QStringLiteral("files"), ok, message);
        if (ok) {
            m_cache->replaceFiles(files);
        }
    }
    finalizeUiMessage(out);
    return out;
}

// ── fetchQuota ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchQuota() const {
    QVariantMap out;
    QString message;
    bool ok = false;
    const QVariantMap quota = fetchQuotaWithRetry(message, ok);

    out.insert("ok", ok);
    out.insert("message", message);
    out.insert("totalDisplay", quota.value("totalDisplay"));
    out.insert("usedDisplay", quota.value("usedDisplay"));
    out.insert("totalBytes", quota.value("totalBytes", static_cast<qulonglong>(0)));
    out.insert("usedBytes", quota.value("usedBytes", static_cast<qulonglong>(0)));

    if (m_cache != nullptr) {
        m_cache->updateSyncState(QStringLiteral("quota"), ok, message);
        if (ok) {
            m_cache->saveQuota(quota);
        }
    }
    finalizeUiMessage(out);
    return out;
}

// ── deleteFile ────────────────────────────────────────────────────────────

QVariantMap CloudBridge::deleteFile(const QString& fileId) const {
    QVariantMap out;
    logging::info("app", "cloud_bridge", "delete_file_start", "Suppression fichier",
                  {{"file_id", fileId.toStdString()}});
    const usecases::cloud::DeleteCloudFileUseCase useCase;
    const auto r = useCase.execute(fileId.toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok && m_cache != nullptr) {
        m_cache->removeFile(fileId);
        m_cache->invalidateScope(QStringLiteral("files"));
    }
    finalizeUiMessage(out);
    return out;
}

// ── getDownloadUrl ────────────────────────────────────────────────────────

QVariantMap CloudBridge::getDownloadUrl(const QString& fileId) const {
    QVariantMap out;
    const usecases::cloud::GetDownloadUrlUseCase useCase;
    const auto r = useCase.execute(fileId.toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok)
        out.insert("url", QString::fromStdString(r.url));
    finalizeUiMessage(out);
    return out;
}

// ── fetchPrinters ─────────────────────────────────────────────────────────

QVariantMap CloudBridge::fetchPrinters() const {
    QVariantMap out;
    if constexpr (kDebugBuildEnabled) {
        out.insert("endpoint", QStringLiteral(
            "/p/p/workbench/api/work/printer/getPrinters + "
            "/p/p/workbench/api/work/project/getProjects?printer_id=<id>&print_status=1"));
    }
    QString message;
    QString rawJson;
    bool ok = false;
    const QVariantList printers = fetchPrintersWithRetry(message, ok, rawJson);
    out.insert("ok", ok);
    out.insert("message", message);
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", rawJson);
    }
    out.insert("printers", printers);

    if (m_cache != nullptr) {
        m_cache->updateSyncState(QStringLiteral("printers"), ok, message);
        if (ok) {
            m_cache->replacePrinters(printers);
            m_cache->replaceJobs(collectJobsFromPrinters(printers));
        }
    }
    finalizeUiMessage(out);
    return out;
}

// ── fetchCompatiblePrintersByExt ─────────────────────────────────────────

QVariantMap CloudBridge::fetchCompatiblePrintersByExt(const QString& fileExt) const {
    QVariantMap out;
    const usecases::cloud::FetchPrinterCompatibilityByExtUseCase useCase;
    const auto r = useCase.execute(fileExt.trimmed().toLower().toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok) {
        QVariantList printers;
        printers.reserve(static_cast<qsizetype>(r.printers.size()));
        for (const auto& p : r.printers)
            printers.append(printerCompatToMap(p));
        out.insert("printers", printers);
    }
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::fetchCompatiblePrintersByFileId(const QString& fileId) const {
    QVariantMap out;
    const usecases::cloud::FetchPrinterCompatibilityByFileIdUseCase useCase;
    const auto r = useCase.execute(fileId.trimmed().toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok) {
        QVariantList printers;
        printers.reserve(static_cast<qsizetype>(r.printers.size()));
        for (const auto& p : r.printers)
            printers.append(printerCompatToMap(p));
        out.insert("printers", printers);
    }
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::fetchPrinterDetails(const QString& printerId) const {
    QVariantMap out;
    const usecases::cloud::FetchPrinterDetailsUseCase useCase;
    const auto r = useCase.execute(printerId.trimmed().toStdString());
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString::fromStdString(r.rawJson));
    }
    if (r.ok)
        out.insert("details", printerDetailsToMap(r));
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::fetchReasonCatalog() const {
    QVariantMap out;
    const usecases::cloud::FetchReasonCatalogUseCase useCase;
    const auto r = useCase.execute();
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if (r.ok) {
        QVariantList reasons;
        reasons.reserve(static_cast<qsizetype>(r.reasons.size()));
        for (const auto& item : r.reasons)
            reasons.append(reasonCatalogItemToMap(item));
        out.insert("reasons", reasons);
    }
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::fetchPrinterProjects(const QString& printerId, int page, int limit) const {
    QVariantMap out;
    const QString normalizedPrinterId = printerId.trimmed();
    const usecases::cloud::FetchPrinterProjectsUseCase useCase;
    const auto r = useCase.execute(normalizedPrinterId.toStdString(), page, limit);
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString::fromStdString(r.rawJson));
    }
    if (r.ok) {
        QVariantList projects;
        projects.reserve(static_cast<qsizetype>(r.items.size()));
        for (const auto& item : r.items)
            projects.append(printerProjectToMap(item));
        out.insert("projects", projects);
        if (m_cache != nullptr && !normalizedPrinterId.isEmpty()) {
            m_cache->replaceJobsForPrinter(normalizedPrinterId, projects);
        }
    } else if (m_cache != nullptr && !normalizedPrinterId.isEmpty()) {
        const QVariantList cached = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
        if (!cached.isEmpty()) {
            out.insert("ok", true);
            out.insert("message", QStringLiteral("Jobs chargés depuis le cache local."));
            out.insert("projects", cached);
        }
    }
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::loadCachedPrinterProjects(const QString& printerId, int page, int limit) const {
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    out.insert("projects", QVariantList{});
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString{});
    }

    const QString normalizedPrinterId = printerId.trimmed();
    if (normalizedPrinterId.isEmpty()) {
        out.insert("message", QStringLiteral("printer_id requis."));
        finalizeUiMessage(out);
        return out;
    }

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    const QVariantList projects = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
    out.insert("ok", true);
    out.insert("message", projects.isEmpty()
                             ? QStringLiteral("Aucun job en cache pour cette imprimante.")
                             : QStringLiteral("Jobs chargés depuis le cache local."));
    out.insert("projects", projects);
    finalizeUiMessage(out);
    return out;
}

// ── sendPrintOrder ────────────────────────────────────────────────────────

QVariantMap CloudBridge::sendPrintOrder(const QString& printerId,
                                        const QString& fileId,
                                        bool deleteAfterPrint,
                                        bool dryRun) const {
    QVariantMap out;
    if (dryRun) {
        out.insert("ok", true);
        out.insert("message", QString("Dry-run: print order payload generated."));
        out.insert("taskId", QString());
        out.insert("msgId", QString());
        out.insert("correlationTicket", QString());
        out.insert("correlationStatus", QStringLiteral("Pending"));
        finalizeUiMessage(out);
        return out;
    }

    const usecases::cloud::SendPrintOrderUseCase useCase;
    const auto r = useCase.execute(
        printerId.toStdString(), fileId.toStdString(), deleteAfterPrint);
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("taskId",  QString::fromStdString(r.taskId));
    out.insert("msgId", QString::fromStdString(r.msgId));
    out.insert("correlationTicket", QString::fromStdString(r.correlationTicket));
    out.insert("correlationStatus", QString::fromStdString(r.correlationStatus));
    if (r.ok && m_cache != nullptr) {
        m_cache->invalidateScope(QStringLiteral("printers"));
    }
    finalizeUiMessage(out);
    return out;
}

// ── startDownload (async) ─────────────────────────────────────────────────

void CloudBridge::startDownload(const QString& signedUrl, const QString& savePath) {
    if (m_dlReply) {
        logging::warn("app", "cloud_bridge", "download_already_running",
                      "Un téléchargement est déjà en cours");
        return;
    }

    m_dlPath = savePath;
    m_dlFile = new QFile(savePath, this);
    if (!m_dlFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logging::error("app", "cloud_bridge", "download_open_failed",
                       "Impossible d'ouvrir le fichier de destination",
                       {{"path", savePath.toStdString()}});
        emit downloadFinished(false, "Impossible d'ouvrir : " + savePath, {});
        delete m_dlFile;
        m_dlFile = nullptr;
        return;
    }

    logging::info("app", "cloud_bridge", "download_start",
                  "Démarrage téléchargement",
                  {{"dest", savePath.toStdString()}});

    // GET direct sur l'URL signée — SANS Authorization ni XX-* (section 4.3 / 6.2)
    const QUrl dlUrl(signedUrl);
    QNetworkRequest req(dlUrl);
    req.setTransferTimeout(0);  // pas de timeout pour les gros fichiers

    m_dlReply = m_nam->get(req);

    connect(m_dlReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_dlFile) m_dlFile->write(m_dlReply->readAll());
    });

    connect(m_dlReply, &QNetworkReply::downloadProgress,
            this, [this](qint64 recv, qint64 total) {
                emit downloadProgress(recv, total);
            });

    connect(m_dlReply, &QNetworkReply::finished, this, [this]() {
        const bool netOk = (m_dlReply->error() == QNetworkReply::NoError);
        const QString errStr = m_dlReply->errorString();
        const QString path   = m_dlPath;

        if (m_dlFile) {
            m_dlFile->flush();
            m_dlFile->close();
        }

        if (netOk) {
            logging::info("app", "cloud_bridge", "download_finished_ok",
                          "Téléchargement terminé", {{"dest", path.toStdString()}});
            emit downloadFinished(true, "Téléchargement terminé", path);
        } else {
            logging::warn("app", "cloud_bridge", "download_failed",
                          "Échec téléchargement", {{"error", errStr.toStdString()}});
            if (m_dlFile) m_dlFile->remove();
            emit downloadFinished(false, "Erreur: " + errStr, {});
        }
        cleanupDownload();
    });
}

// ── cancelDownload ────────────────────────────────────────────────────────

void CloudBridge::cancelDownload() {
    if (m_dlReply) {
        logging::info("app", "cloud_bridge", "download_cancelled",
                      "Annulation téléchargement");
        m_dlReply->abort();
        // cleanupDownload() sera appelé via le signal finished
    }
}

// ── cleanupDownload ───────────────────────────────────────────────────────

void CloudBridge::cleanupDownload() {
    if (m_dlReply) {
        m_dlReply->deleteLater();
        m_dlReply = nullptr;
    }
    if (m_dlFile) {
        delete m_dlFile;
        m_dlFile = nullptr;
    }
    m_dlPath.clear();
}

} // namespace accloud
