#include "CloudBridge.h"

#include "LocalCacheStore.h"
#include "ThumbnailCachePolicy.h"
#include "UiPerfTrace.h"
#include "app/realtime/PrinterRealtimeStore.h"
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
#include "app/usecases/cloud/SendPrinterOrderUseCase.h"
#include "app/usecases/cloud/SendPrintOrderUseCase.h"
#include "app/usecases/cloud/UploadLocalFileUseCase.h"
#include "app/usecases/cloud/UpdateCloudPwszPreviewsUseCase.h"
#include "infra/cloud/archive/PwszPreviewArchive.h"
#include "infra/cloud/thumbnail/ThumbnailValidation.h"
#include "infra/cloud/HarImporter.h"
#include "infra/config/AppPaths.h"
#include "infra/debug/DebugBuild.h"
#include "infra/logging/JsonlLogger.h"
#include "infra/logging/RawTrafficLogger.h"
#include "infra/logging/Redactor.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QStringList>
#include <QMutex>
#include <QMutexLocker>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QTimer>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <map>
#include <string>
#include <utility>

namespace accloud {
namespace {

constexpr bool kDebugBuildEnabled = debug::kEnabled;

logging::raw::HeaderList rawRequestHeaders(const QNetworkRequest& request) {
    logging::raw::HeaderList headers;
    const auto names = request.rawHeaderList();
    headers.reserve(static_cast<std::size_t>(names.size()));
    for (const QByteArray& name : names) {
        headers.emplace_back(name.toStdString(), request.rawHeader(name).toStdString());
    }
    return headers;
}

logging::raw::HeaderList rawResponseHeaders(const QNetworkReply& reply) {
    logging::raw::HeaderList headers;
    const auto pairs = reply.rawHeaderPairs();
    headers.reserve(static_cast<std::size_t>(pairs.size()));
    for (const auto& pair : pairs) {
        headers.emplace_back(pair.first.toStdString(), pair.second.toStdString());
    }
    return headers;
}

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

QString fileExtension(const QString& fileName) {
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot < 0 || dot + 1 >= fileName.size()) {
        return {};
    }
    return fileName.mid(dot + 1).toLower();
}

bool isKnownCloudSliceExtension(const QString& ext) {
    static const QStringList known = {
        QStringLiteral("photon"), QStringLiteral("pws"), QStringLiteral("pwsz"),
        QStringLiteral("photons"), QStringLiteral("pw0"), QStringLiteral("pwx"),
        QStringLiteral("pwmo"), QStringLiteral("pwma"), QStringLiteral("pwms"),
        QStringLiteral("pwmx"), QStringLiteral("pmx2"), QStringLiteral("pmsq"),
        QStringLiteral("dlp"), QStringLiteral("dl2p"), QStringLiteral("pwmb"),
        QStringLiteral("pm3"), QStringLiteral("pm3m"), QStringLiteral("pm3r"),
        QStringLiteral("pm3n"), QStringLiteral("px6s"), QStringLiteral("pm5"),
        QStringLiteral("pm5s"), QStringLiteral("m5sp")
    };
    const QString value = ext.trimmed().toLower();
    return !value.isEmpty() && known.contains(value);
}

QString normalizedCompatText(const QVariant& value) {
    QString out;
    const QString raw = value.toString().trimmed().toLower();
    out.reserve(raw.size());
    bool previousSpace = false;
    for (const QChar ch : raw) {
        const bool separator = ch == QLatin1Char('_')
                || ch == QLatin1Char('-')
                || ch == QLatin1Char('.')
                || ch == QLatin1Char('/');
        const bool space = ch.isSpace() || separator;
        if (space) {
            if (!previousSpace && !out.isEmpty()) {
                out.append(QLatin1Char(' '));
            }
            previousSpace = true;
            continue;
        }
        out.append(ch);
        previousSpace = false;
    }
    return out.trimmed();
}

QStringList compatTokens(const QString& value) {
    static const QStringList stopTokens = {
        QStringLiteral("anycubic"), QStringLiteral("photon"), QStringLiteral("mono"),
        QStringLiteral("printer"), QStringLiteral("printers"), QStringLiteral("series"),
        QStringLiteral("resin"), QStringLiteral("lcd")
    };
    QStringList out;
    const QStringList rawTokens = normalizedCompatText(value).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& rawToken : rawTokens) {
        const QString token = rawToken.trimmed();
        if (token.size() <= 1 || stopTokens.contains(token) || out.contains(token)) {
            continue;
        }
        out.push_back(token);
    }
    return out;
}

int compatTokenOverlapCount(const QStringList& a, const QStringList& b) {
    int count = 0;
    for (const QString& token : b) {
        if (a.contains(token)) {
            ++count;
        }
    }
    return count;
}

bool compatTextContains(const QString& haystack, const QString& needle) {
    const QString h = normalizedCompatText(haystack);
    const QString n = normalizedCompatText(needle);
    return !h.isEmpty() && !n.isEmpty() && h.contains(n);
}

QVariantMap localCompatResult(bool ok, int score, const QString& reason) {
    QVariantMap out;
    out.insert(QStringLiteral("ok"), ok);
    out.insert(QStringLiteral("score"), score);
    out.insert(QStringLiteral("reason"), reason);
    return out;
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

QString normalizedThumbnailUrl(const QString& raw) {
    const QString value = raw.trimmed();
    if (value.isEmpty()) {
        return {};
    }
    const QFileInfo localInfo(value);
    if (localInfo.isAbsolute() && localInfo.exists() && localInfo.isFile()) {
        return QUrl::fromLocalFile(localInfo.absoluteFilePath()).toString();
    }
    const QUrl parsed(value);
    if (parsed.isValid() && !parsed.scheme().isEmpty()) {
        const QString scheme = parsed.scheme().toLower();
        if (scheme == QStringLiteral("http")
            || scheme == QStringLiteral("https")
            || scheme == QStringLiteral("file")) {
            return value;
        }
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

QString safeThumbnailUrlForLogs(const QString& value) {
    return QString::fromStdString(logging::safeUrlForLogs(value.toStdString()));
}

QString thumbnailSourceForPersistence(const QString& value) {
    const QString normalized = normalizedThumbnailUrl(value);
    if (normalized.isEmpty()) {
        return {};
    }
    const QUrl url(normalized);
    if (url.isLocalFile()) {
        return normalized;
    }
    // Query strings may contain temporary credentials. Keep them in memory only
    // for the current fetch and never persist them in the SQLite cache.
    if (!url.userInfo().isEmpty() || !url.query().isEmpty() || !url.fragment().isEmpty()) {
        return {};
    }
    return normalized;
}

QString thumbnailCacheDirPath() {
    const std::filesystem::path configured = accloud::config::thumbnailDir();
    if (configured.empty()) {
        return {};
    }
    const QString dir = QString::fromStdString(configured.string());
    QDir().mkpath(dir);
    return dir;
}

QString cacheBasePathForThumbnailUrl(const QString& url) {
    const QString dir = thumbnailCacheDirPath();
    if (dir.isEmpty()) {
        return {};
    }
    const QByteArray hash = thumbnail_cache::stableCacheKey(url);
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

QByteArray readFileBytes(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

struct LocalThumbnailValidationMemo {
    qint64 size{-1};
    qint64 modifiedAtMs{-1};
    bool valid{false};
    bool tooSmall{false};
    QString detectedExtension;
};

struct LocalThumbnailValidationResult {
    bool valid{false};
    bool tooSmall{false};
    bool reused{false};
    QString detectedExtension;
};

QMutex g_localThumbnailValidationMutex;
QHash<QString, LocalThumbnailValidationMemo> g_localThumbnailValidationMemo;

void forgetLocalThumbnailValidation(const QString& path) {
    QMutexLocker lock(&g_localThumbnailValidationMutex);
    g_localThumbnailValidationMemo.remove(path);
}

LocalThumbnailValidationResult validateLocalThumbnailPath(const QString& path) {
    QMutexLocker lock(&g_localThumbnailValidationMutex);

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        g_localThumbnailValidationMemo.remove(path);
        return {};
    }

    const qint64 size = info.size();
    const qint64 modifiedAtMs = info.lastModified().toMSecsSinceEpoch();
    const auto existing = g_localThumbnailValidationMemo.constFind(path);
    if (existing != g_localThumbnailValidationMemo.cend()
        && existing->size == size
        && existing->modifiedAtMs == modifiedAtMs) {
        return {existing->valid, existing->tooSmall, true, existing->detectedExtension};
    }

    LocalThumbnailValidationMemo memo;
    memo.size = size;
    memo.modifiedAtMs = modifiedAtMs;
    if (size < static_cast<qint64>(cloud::thumbnail::kMinimumValidThumbnailBytes)) {
        memo.tooSmall = true;
        g_localThumbnailValidationMemo.insert(path, memo);
        return {false, true, false, {}};
    }

    const QByteArray bytes = readFileBytes(path);
    const auto byteValidation = cloud::thumbnail::validateThumbnailBytes(bytes);
    memo.tooSmall = byteValidation.tooSmall;
    memo.detectedExtension = detectImageExtension(bytes.left(32), path);
    if (byteValidation.valid) {
        QImageReader reader(path);
        memo.valid = reader.canRead();
    }
    g_localThumbnailValidationMemo.insert(path, memo);
    return {memo.valid, memo.tooSmall, false, memo.detectedExtension};
}

bool validatedLocalThumbnailCanBeReused(const QString& source, bool forceDownload) {
    const QUrl url(source.trimmed());
    if (!url.isLocalFile()) {
        return false;
    }
    const QString path = url.toLocalFile();
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return false;
    }

    QMutexLocker lock(&g_localThumbnailValidationMutex);
    const auto existing = g_localThumbnailValidationMemo.constFind(path);
    const bool validationKnownValid = existing != g_localThumbnailValidationMemo.cend()
        && existing->valid
        && existing->size == info.size()
        && existing->modifiedAtMs == info.lastModified().toMSecsSinceEpoch();
    return thumbnail_cache::shouldReuseValidatedLocalCandidate(
        source, validationKnownValid, forceDownload);
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
        if (!info.exists() || !info.isFile()) {
            continue;
        }
        const LocalThumbnailValidationResult validation = validateLocalThumbnailPath(path);
        if (!validation.valid) {
            QFile::remove(path);
            forgetLocalThumbnailValidation(path);
            continue;
        }
        const QString detectedExt = validation.detectedExtension;
        if (!detectedExt.isEmpty() && detectedExt != ext) {
            const QString migratedPath = cacheBasePath + detectedExt;
            if (!QFileInfo::exists(migratedPath)) {
                QFile::copy(path, migratedPath);
            }
            if (validateLocalThumbnailPath(migratedPath).valid) {
                return migratedPath;
            }
        }
        return path;
    }
    return {};
}

struct ThumbnailFetchResult {
    QString localUrl;
    int httpStatus{0};
    int networkErrorCode{0};
    QString failureCategory;
    bool retryable{false};
    bool tooSmall{false};
    bool cancelled{false};
};

QMutex g_thumbnailFailureMutex;
QHash<QString, qint64> g_thumbnailRetryAfter;
constexpr int kThumbnailTimeoutMs = 12000;
constexpr qint64 kThumbnailPermanentRetryDelaySec = 300;
constexpr qint64 kThumbnailTransientRetryDelaySec = 15;

bool thumbnailRetryIsDeferred(const QString& normalizedUrl) {
    QMutexLocker lock(&g_thumbnailFailureMutex);
    const qint64 retryAfter = g_thumbnailRetryAfter.value(normalizedUrl, 0);
    if (retryAfter <= QDateTime::currentSecsSinceEpoch()) {
        g_thumbnailRetryAfter.remove(normalizedUrl);
        return false;
    }
    return true;
}

void rememberThumbnailFailure(const QString& normalizedUrl, qint64 delaySec) {
    QMutexLocker lock(&g_thumbnailFailureMutex);
    g_thumbnailRetryAfter.insert(normalizedUrl,
                                 QDateTime::currentSecsSinceEpoch() + delaySec);
}

void clearThumbnailFailure(const QString& normalizedUrl) {
    QMutexLocker lock(&g_thumbnailFailureMutex);
    g_thumbnailRetryAfter.remove(normalizedUrl);
}

ThumbnailFetchResult fetchThumbnailToCache(
    const QString& normalizedUrl,
    const QString& cacheBasePath,
    const usecases::cloud::CancellationCallback& shouldCancel = {}) {
    ThumbnailFetchResult result;
    if (usecases::cloud::detail::cancellationRequested(shouldCancel)) {
        result.cancelled = true;
        result.failureCategory = QStringLiteral("cancelled");
        return result;
    }
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(normalizedUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QEventLoop loop;
    QTimer timeout;
    QTimer cancellationTimer;
    timeout.setSingleShot(true);
    cancellationTimer.setInterval(100);

    bool timedOut = false;
    bool cancelled = false;
    const std::string rawCorrelationId = logging::raw::nextCorrelationId("http");
    logging::raw::logHttpRequest(rawCorrelationId,
                                 "GET",
                                 normalizedUrl.toStdString(),
                                 rawRequestHeaders(req),
                                 {});
    QNetworkReply* reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::sslErrors, reply,
                     [reply](const QList<QSslError>&) {
                         // Thumbnail preview is non-critical; tolerate SSL chain issues
                         // only for this local image-cache request.
                         reply->ignoreSslErrors();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    if (shouldCancel) {
        QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
            if (!usecases::cloud::detail::cancellationRequested(shouldCancel)) {
                return;
            }
            cancelled = true;
            reply->abort();
            loop.quit();
        });
        cancellationTimer.start();
    }
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(kThumbnailTimeoutMs);
    loop.exec();
    timeout.stop();
    cancellationTimer.stop();

    result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.networkErrorCode = static_cast<int>(reply->error());
    const QByteArray bytes = reply->readAll();
    const bool networkOk = !timedOut && !cancelled
                        && reply->error() == QNetworkReply::NoError;
    const std::string replyError = reply->errorString().toStdString();
    logging::raw::logHttpResponse(
        rawCorrelationId,
        result.httpStatus,
        reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString(),
        rawResponseHeaders(*reply),
        bytes.toStdString(),
        networkOk ? std::string{} : replyError);
    reply->deleteLater();

    if (cancelled || usecases::cloud::detail::cancellationRequested(shouldCancel)) {
        result.cancelled = true;
        result.failureCategory = QStringLiteral("cancelled");
        return result;
    }
    if (!networkOk || bytes.isEmpty()) {
        if (timedOut) {
            result.failureCategory = QStringLiteral("timeout");
            result.retryable = true;
        } else if (result.httpStatus == 403) {
            result.failureCategory = QStringLiteral("http_forbidden");
            result.retryable = true;
        } else if (result.httpStatus == 404) {
            result.failureCategory = QStringLiteral("http_not_found");
            result.retryable = true;
        } else if (result.httpStatus == 429 || result.httpStatus >= 500) {
            result.failureCategory = QStringLiteral("http_transient");
            result.retryable = true;
        } else if (bytes.isEmpty() && networkOk) {
            result.failureCategory = QStringLiteral("empty_body");
        } else {
            result.failureCategory = QStringLiteral("network_error");
            result.retryable = true;
        }
        return result;
    }

    const auto validation = cloud::thumbnail::validateThumbnailBytes(bytes);
    if (!validation.valid) {
        result.tooSmall = validation.tooSmall;
        result.failureCategory = validation.tooSmall
                                     ? QStringLiteral("placeholder_too_small")
                                     : QStringLiteral("invalid_image");
        return result;
    }

    const QString ext = detectImageExtension(bytes, normalizedUrl);
    const QString finalPath = cacheBasePath + ext;
    QSaveFile file(finalPath);
    if (!file.open(QIODevice::WriteOnly)) {
        result.failureCategory = QStringLiteral("cache_open_failed");
        return result;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        file.cancelWriting();
        result.failureCategory = QStringLiteral("cache_write_failed");
        return result;
    }
    QImageReader reader(finalPath);
    if (!reader.canRead()) {
        QFile::remove(finalPath);
        result.failureCategory = QStringLiteral("invalid_image");
        return result;
    }
    result.localUrl = QUrl::fromLocalFile(finalPath).toString();
    return result;
}

struct ThumbnailResolveResult {
    QString localUrl;
    bool tooSmall{false};
    bool cancelled{false};
    bool localValidationReused{false};
    QString failureCategory;
};

void removeCachedThumbnailFiles(const QString& cacheBasePath) {
    static const QStringList exts = {
        QStringLiteral(".png"), QStringLiteral(".jpg"),
        QStringLiteral(".jpeg"), QStringLiteral(".webp")
    };
    for (const QString& ext : exts) {
        const QString path = cacheBasePath + ext;
        QFile::remove(path);
        forgetLocalThumbnailValidation(path);
    }
}

ThumbnailResolveResult resolveThumbnailLocalUrl(
    const QString& source,
    bool downloadMissing,
    bool forceDownload = false,
    const usecases::cloud::CancellationCallback& shouldCancel = {}) {
    ThumbnailResolveResult out;
    if (usecases::cloud::detail::cancellationRequested(shouldCancel)) {
        out.cancelled = true;
        out.failureCategory = QStringLiteral("cancelled");
        return out;
    }
    const QString normalized = normalizedThumbnailUrl(source);
    if (normalized.isEmpty()) {
        if (!source.trimmed().isEmpty()) {
            logging::warn("app", "thumbnail_cache", "unsupported_source",
                          "Unsupported thumbnail source",
                          {{"source", safeThumbnailUrlForLogs(source).toStdString()}});
        }
        out.failureCategory = QStringLiteral("unsupported_source");
        return out;
    }

    const QUrl normalizedUrl(normalized);
    if (normalizedUrl.isLocalFile()) {
        const QString localPath = normalizedUrl.toLocalFile();
        const LocalThumbnailValidationResult validation = validateLocalThumbnailPath(localPath);
        out.localValidationReused = validation.reused;
        if (validation.valid) {
            out.localUrl = normalized;
            return out;
        }
        out.tooSmall = validation.tooSmall;
        out.failureCategory = validation.tooSmall
                                  ? QStringLiteral("placeholder_too_small")
                                  : QStringLiteral("local_source_unreadable");
        return out;
    }

    const QString cacheBasePath = cacheBasePathForThumbnailUrl(normalized);
    if (cacheBasePath.isEmpty()) {
        out.failureCategory = QStringLiteral("cache_path_unavailable");
        return out;
    }
    if (forceDownload) {
        removeCachedThumbnailFiles(cacheBasePath);
        clearThumbnailFailure(normalized);
    } else {
        const QString cachedPath = findReadableCachedImage(cacheBasePath);
        if (!cachedPath.isEmpty()) {
            clearThumbnailFailure(normalized);
            out.localUrl = QUrl::fromLocalFile(cachedPath).toString();
            logging::info("app", "thumbnail_cache", "cache_hit",
                          "Thumbnail served from local cache",
                          {{"url", safeThumbnailUrlForLogs(normalized).toStdString()},
                           {"path", cachedPath.toStdString()}});
            return out;
        }
    }

    if (!downloadMissing) {
        out.failureCategory = QStringLiteral("cache_miss");
        return out;
    }
    if (!forceDownload && thumbnailRetryIsDeferred(normalized)) {
        out.failureCategory = QStringLiteral("retry_deferred");
        return out;
    }

    const ThumbnailFetchResult fetch = fetchThumbnailToCache(normalized, cacheBasePath, shouldCancel);
    if (fetch.cancelled) {
        out.cancelled = true;
        out.failureCategory = QStringLiteral("cancelled");
        return out;
    }
    if (fetch.localUrl.isEmpty()) {
        const qint64 retryDelay = fetch.retryable
                                      ? kThumbnailTransientRetryDelaySec
                                      : kThumbnailPermanentRetryDelaySec;
        rememberThumbnailFailure(normalized, retryDelay);
        out.tooSmall = fetch.tooSmall;
        out.failureCategory = fetch.failureCategory;
        logging::warn("app", "thumbnail_cache", "download_failed",
                      "Thumbnail download failed",
                      {{"url", safeThumbnailUrlForLogs(normalized).toStdString()},
                       {"http", std::to_string(fetch.httpStatus)},
                       {"category", fetch.failureCategory.toStdString()},
                       {"retryable", fetch.retryable ? "1" : "0"},
                       {"network_error", std::to_string(fetch.networkErrorCode)}});
        return out;
    }
    clearThumbnailFailure(normalized);
    out.localUrl = fetch.localUrl;
    logging::info("app", "thumbnail_cache", "download_ok",
                  "Thumbnail downloaded and cached",
                  {{"url", safeThumbnailUrlForLogs(normalized).toStdString()},
                   {"path", QUrl(fetch.localUrl).toLocalFile().toStdString()}});
    return out;
}

QString normalizeFileNameForMatch(const QString& value) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return QFileInfo(trimmed).fileName().toLower();
}

QString normalizeFileStemForMatch(const QString& value) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return QFileInfo(trimmed).completeBaseName().toLower();
}

bool localImageIsVisuallyUsable(const QString& sourceUrl) {
    const QUrl url(sourceUrl);
    if (!url.isLocalFile()) {
        return true;
    }
    const QString path = url.toLocalFile();
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.size() <= 0) {
        return false;
    }
    if (info.size() < static_cast<qint64>(cloud::thumbnail::kMinimumValidThumbnailBytes)) {
        return false;
    }

    QImageReader reader(path);
    if (!reader.canRead()) {
        return false;
    }
    QImage image = reader.read();
    if (image.isNull()) {
        return false;
    }
    if (!image.hasAlphaChannel()) {
        return true;
    }

    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    const int w = argb.width();
    const int h = argb.height();
    if (w <= 0 || h <= 0) {
        return false;
    }

    for (int y = 0; y < h; ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (qAlpha(row[x]) > 0) {
                return true;
            }
        }
    }
    return false;
}

QString resolveProjectImageFromFilesCache(const LocalCacheStore* cache,
                                          const QString& currentFile,
                                          const QString& gcodeName) {
    if (cache == nullptr) {
        return {};
    }

    const QString targetName = normalizeFileNameForMatch(currentFile);
    const QString targetStem = normalizeFileStemForMatch(currentFile);
    const QString altName = normalizeFileNameForMatch(gcodeName);
    const QString altStem = normalizeFileStemForMatch(gcodeName);
    if (targetName.isEmpty() && targetStem.isEmpty() && altName.isEmpty() && altStem.isEmpty()) {
        return {};
    }

    const QVariantList files = cache->loadFiles(1, 1500);
    QString stemMatchCandidate;
    for (const QVariant& fileVar : files) {
        const QVariantMap fileMap = fileVar.toMap();
        const QString fileName = fileMap.value(QStringLiteral("fileName")).toString();
        const QString fileKey = normalizeFileNameForMatch(fileName);
        const QString fileStem = normalizeFileStemForMatch(fileName);
        const QString thumb = fileMap.value(QStringLiteral("thumbnailUrl")).toString().trimmed();
        if (thumb.isEmpty()) {
            continue;
        }

        const bool exactNameMatch = (!targetName.isEmpty() && fileKey == targetName)
                                 || (!altName.isEmpty() && fileKey == altName);
        if (exactNameMatch) {
            return resolveThumbnailLocalUrl(thumb, true).localUrl;
        }

        const bool stemMatch = (!targetStem.isEmpty() && fileStem == targetStem)
                            || (!altStem.isEmpty() && fileStem == altStem);
        if (stemMatch && stemMatchCandidate.isEmpty()) {
            stemMatchCandidate = resolveThumbnailLocalUrl(thumb, true).localUrl;
        }
    }
    return stemMatchCandidate;
}

void resolveThumbnailInMap(QVariantMap& map, bool downloadMissing, bool forceDownload = false) {
    const QString persistedSource =
        map.value(QStringLiteral("thumbnailSourceUrl")).toString().trimmed();
    const QString displayedSource =
        map.value(QStringLiteral("thumbnailUrl")).toString().trimmed();
    const QStringList candidates = thumbnail_cache::orderedCandidates(
        map.value(QStringLiteral("thumbnailCandidates")).toStringList(),
        persistedSource,
        displayedSource,
        forceDownload);

    const int statusCode = map.value(QStringLiteral("statusCode"), 0).toInt();
    const QString status = map.value(QStringLiteral("status")).toString().trimmed().toUpper();
    if (statusCode == 2 || status == QStringLiteral("PROCESSING")) {
        map.insert(QStringLiteral("thumbnailUrl"), QString{});
        map.insert(QStringLiteral("thumbnailState"), QStringLiteral("processing"));
        logging::info("app", "thumbnail_cache", "processing_deferred",
                      "Thumbnail download deferred while cloud processing is active",
                      {{"fileId", map.value(QStringLiteral("fileId")).toString().toStdString()},
                       {"candidate_count", std::to_string(candidates.size())}});
        return;
    }

    QString resolved;
    QString resolvedSource;
    bool reusedValidatedLocal = false;
    bool sawTooSmallCandidate = false;
    if (!forceDownload) {
        for (const QString& candidate : candidates) {
            const QString source = candidate.trimmed();
            if (!validatedLocalThumbnailCanBeReused(source, forceDownload)) {
                continue;
            }
            resolved = normalizedThumbnailUrl(source);
            resolvedSource = source;
            reusedValidatedLocal = !resolved.isEmpty();
            if (reusedValidatedLocal) {
                break;
            }
        }
    }

    for (qsizetype index = 0; resolved.isEmpty() && index < candidates.size(); ++index) {
        const QString source = candidates.at(index).trimmed();
        if (source.isEmpty()) continue;

        const bool localSource = QUrl(normalizedThumbnailUrl(source)).isLocalFile();
        if (!localSource) {
            logging::info("app", "thumbnail_cache", "candidate_attempt",
                          "Trying thumbnail candidate",
                          {{"fileId", map.value(QStringLiteral("fileId")).toString().toStdString()},
                           {"candidate_index", std::to_string(index)},
                           {"candidate_count", std::to_string(candidates.size())},
                           {"url", safeThumbnailUrlForLogs(source).toStdString()}});
        }

        const ThumbnailResolveResult candidateResult =
            resolveThumbnailLocalUrl(source, downloadMissing, forceDownload);
        resolved = candidateResult.localUrl;
        reusedValidatedLocal = candidateResult.localValidationReused;
        sawTooSmallCandidate = sawTooSmallCandidate || candidateResult.tooSmall;
        if (localSource && !candidateResult.localValidationReused) {
            logging::info("app", "thumbnail_cache", "candidate_attempt",
                          "Trying thumbnail candidate",
                          {{"fileId", map.value(QStringLiteral("fileId")).toString().toStdString()},
                           {"candidate_index", std::to_string(index)},
                           {"candidate_count", std::to_string(candidates.size())},
                           {"url", safeThumbnailUrlForLogs(source).toStdString()}});
        }
        if (!resolved.isEmpty()) {
            resolvedSource = source;
            if (!candidateResult.localValidationReused) {
                logging::info("app", "thumbnail_cache", "candidate_selected",
                              "Thumbnail candidate selected",
                              {{"fileId", map.value(QStringLiteral("fileId")).toString().toStdString()},
                               {"candidate_index", std::to_string(index)},
                               {"url", safeThumbnailUrlForLogs(source).toStdString()}});
            }
            break;
        }

        if (!candidateResult.localValidationReused) {
            logging::warn("app", "thumbnail_cache", "candidate_failed",
                          "Thumbnail candidate did not produce a usable local image",
                          {{"fileId", map.value(QStringLiteral("fileId")).toString().toStdString()},
                           {"candidate_index", std::to_string(index)},
                           {"url", safeThumbnailUrlForLogs(source).toStdString()}});
        }
    }

    QString persistedCandidate = thumbnailSourceForPersistence(resolvedSource);
    if (persistedCandidate.isEmpty()) {
        for (const QString& candidate : candidates) {
            persistedCandidate = thumbnailSourceForPersistence(candidate);
            if (!persistedCandidate.isEmpty()) break;
        }
    }
    map.insert(QStringLiteral("thumbnailSourceUrl"), persistedCandidate);
    map.insert(QStringLiteral("thumbnailUrl"), resolved);
    map.insert(QStringLiteral("thumbnailState"),
               resolved.isEmpty() ? QStringLiteral("unavailable") : QStringLiteral("ready"));
    const bool pwszFile = fileExtension(map.value(QStringLiteral("fileName")).toString())
                              == QStringLiteral("pwsz");
    const bool updateCandidate = resolved.isEmpty() && sawTooSmallCandidate && pwszFile;
    map.insert(QStringLiteral("thumbnailUpdateCandidate"), updateCandidate);
    map.insert(QStringLiteral("thumbnailInvalidReason"),
               updateCandidate ? QStringLiteral("placeholder_too_small") : QString{});
    if (!resolved.isEmpty() && !reusedValidatedLocal) {
        const QString localPath = QUrl(resolved).toLocalFile();
        if (!localPath.isEmpty()) {
            QImageReader reader(localPath);
            const bool canRead = reader.canRead();
            const std::string readError =
                canRead ? std::string{} : reader.errorString().toStdString();
            logging::info("app", "thumbnail_cache", "qml_probe",
                          "Thumbnail path probe before QML bind",
                          {{"fileId", map.value(QStringLiteral("fileId")).toString().toStdString()},
                           {"path", localPath.toStdString()},
                           {"canRead", canRead ? "1" : "0"},
                           {"error", readError}});
        }
    }
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
    const QString resolvedImg = resolveThumbnailLocalUrl(rawImg, true).localUrl;
    m.insert("img", resolvedImg);
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

} // namespace

// ── Constructeur / destructeur ────────────────────────────────────────────

CloudBridge::CloudBridge(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_cache(new LocalCacheStore()) {}

CloudBridge::~CloudBridge() {
    m_shuttingDown.store(true);
    m_pwszCloudUpdateCancelRequested.store(true);
    waitBackgroundTasks();
    cleanupDownload();
    delete m_cache;
    m_cache = nullptr;
}

void CloudBridge::reapFinishedBackgroundTasksLocked() {
    auto it = m_backgroundTasks.begin();
    while (it != m_backgroundTasks.end()) {
        if (it->valid() && it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                it->get();
            } catch (...) {
                // Best effort cleanup of async tasks.
            }
            it = m_backgroundTasks.erase(it);
        } else {
            ++it;
        }
    }
}

void CloudBridge::launchBackgroundTask(std::function<void()> task) {
    if (m_shuttingDown.load()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_backgroundTasksMutex);
    reapFinishedBackgroundTasksLocked();
    m_backgroundTasks.emplace_back(std::async(std::launch::async, [this, task = std::move(task)]() mutable {
        if (m_shuttingDown.load()) {
            return;
        }
        task();
    }));
}

void CloudBridge::waitBackgroundTasks() {
    std::vector<std::future<void>> tasks;
    {
        std::lock_guard<std::mutex> lock(m_backgroundTasksMutex);
        tasks.swap(m_backgroundTasks);
    }
    for (auto& task : tasks) {
        if (!task.valid()) {
            continue;
        }
        try {
            task.wait();
            task.get();
        } catch (...) {
            // Ignore task exceptions during shutdown.
        }
    }
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

namespace {

QHash<QString, QString> cachedThumbnailUrlsByFileId(const LocalCacheStore* cache) {
    QHash<QString, QString> thumbnails;
    if (cache == nullptr || !cache->isAvailable()) {
        return thumbnails;
    }

    const QVariantList cachedFiles = cache->loadFiles(1, 1500);
    thumbnails.reserve(cachedFiles.size());
    for (const QVariant& cachedFile : cachedFiles) {
        const QVariantMap map = cachedFile.toMap();
        const QString fileId = map.value(QStringLiteral("fileId")).toString().trimmed();
        const QString thumbnailUrl =
            map.value(QStringLiteral("thumbnailUrl")).toString().trimmed();
        if (!fileId.isEmpty() && QUrl(thumbnailUrl).isLocalFile()) {
            thumbnails.insert(fileId, thumbnailUrl);
        }
    }
    return thumbnails;
}

void reuseCachedThumbnailCandidate(QVariantMap& item,
                                   const QHash<QString, QString>& cachedThumbnails,
                                   bool forceThumbnails) {
    if (forceThumbnails) {
        return;
    }

    const QString fileId = item.value(QStringLiteral("fileId")).toString().trimmed();
    const QString cachedUrl = cachedThumbnails.value(fileId).trimmed();
    if (cachedUrl.isEmpty()) {
        return;
    }

    QStringList candidates = item.value(QStringLiteral("thumbnailCandidates")).toStringList();
    candidates.removeAll(cachedUrl);
    candidates.prepend(cachedUrl);
    item.insert(QStringLiteral("thumbnailCandidates"), candidates);
}

} // namespace

QVariantList CloudBridge::fetchFilesWithRetry(int page, int limit, QString& message, bool& ok, bool downloadThumbnails, bool forceThumbnails) const {
    ok = false;
    QVariantList files;
    const usecases::cloud::LoadCloudFilesUseCase useCase;
    const usecases::cloud::LoadCloudFilesResult result = useCase.execute(page, limit);
    message = QString::fromStdString(result.message);
    ok = result.ok;
    if (!result.ok) {
        return files;
    }

    const QHash<QString, QString> cachedThumbnails =
        forceThumbnails ? QHash<QString, QString>{} : cachedThumbnailUrlsByFileId(m_cache);
    files.reserve(static_cast<qsizetype>(result.files.size()));
    for (const auto& f : result.files) {
        QVariantMap item = fileInfoToMap(f);
        reuseCachedThumbnailCandidate(item, cachedThumbnails, forceThumbnails);
        resolveThumbnailInMap(item, downloadThumbnails, forceThumbnails);
        files.append(item);
    }
    return files;
}

QVariantList CloudBridge::fetchAllFilesWithRetry(int pageSize,
                                                   QString& message,
                                                   bool& ok,
                                                   bool downloadThumbnails,
                                                   bool forceThumbnails) const {
    ok = false;
    QVariantList files;
    const usecases::cloud::LoadCloudFilesUseCase useCase;
    const usecases::cloud::LoadAllCloudFilesResult result = useCase.executeAll(pageSize, 100);
    message = QString::fromStdString(result.message);
    ok = result.ok && result.complete;
    if (!ok) {
        return files;
    }

    const QHash<QString, QString> cachedThumbnails =
        forceThumbnails ? QHash<QString, QString>{} : cachedThumbnailUrlsByFileId(m_cache);
    files.reserve(static_cast<qsizetype>(result.files.size()));
    for (const auto& file : result.files) {
        QVariantMap item = fileInfoToMap(file);
        reuseCachedThumbnailCandidate(item, cachedThumbnails, forceThumbnails);
        resolveThumbnailInMap(item, downloadThumbnails, forceThumbnails);
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
    UiPerfTrace perf("cloud_bridge.load_cached_files");
    perf.setField("page", std::to_string(page));
    perf.setField("limit", std::to_string(limit));
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    out.insert("messageKey", QStringLiteral("cache.files.unavailable"));
    out.insert("fallbackMessage", QStringLiteral("Local file cache is unavailable."));
    out.insert("files", QVariantList{});
    out.insert("total", 0);

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    // Keep startup path fast while ensuring QML only receives validated local images.
    QVariantList files = m_cache->loadFiles(page, limit);
    for (QVariant& file : files) {
        QVariantMap map = file.toMap();
        resolveThumbnailInMap(map, false);
        file = map;
    }
    perf.setCount("items", files.size());
    out.insert("ok", true);
    out.insert("message", files.isEmpty()
                             ? QStringLiteral("Aucune donnée cache.")
                             : QStringLiteral("Fichiers chargés depuis le cache local."));
    out.insert("messageKey", files.isEmpty()
                             ? QStringLiteral("cache.files.empty")
                             : QStringLiteral("cache.files.loaded"));
    out.insert("fallbackMessage", files.isEmpty()
                                  ? QStringLiteral("No file available in local cache.")
                                  : QStringLiteral("Files loaded from local cache."));
    out.insert("params", QVariantMap{
        {QStringLiteral("count"), files.size()},
        {QStringLiteral("page"), page},
        {QStringLiteral("limit"), limit}
    });
    out.insert("files", files);
    out.insert("total", files.size());
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::loadCachedFilesAsync(int page, int limit) {
    if (m_shuttingDown.load()) {
        return;
    }
    launchBackgroundTask([this, page, limit]() {
        const QVariantMap result = loadCachedFiles(page, limit);
        QMetaObject::invokeMethod(this, [this, result]() {
            emit cachedFilesLoaded(result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::loadCachedPrinters() const {
    UiPerfTrace perf("cloud_bridge.load_cached_printers");
    QVariantMap out;
    if constexpr (kDebugBuildEnabled) {
        out.insert("endpoint", QStringLiteral(
            "/p/p/workbench/api/work/printer/getPrinters + "
            "/p/p/workbench/api/work/project/getProjects?printer_id=<id>&print_status=1"));
    }
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    out.insert("messageKey", QStringLiteral("cache.printers.unavailable"));
    out.insert("fallbackMessage", QStringLiteral("Local printer cache is unavailable."));
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString{});
    }
    out.insert("printers", QVariantList{});

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    const QVariantList cachedPrinters = m_cache->loadPrinters();
    perf.setCount("cached_printers", cachedPrinters.size());
    QStringList printerIds;
    printerIds.reserve(cachedPrinters.size());
    for (const QVariant& item : cachedPrinters) {
        const QString printerId = item.toMap().value(QStringLiteral("id")).toString().trimmed();
        if (!printerId.isEmpty() && !printerIds.contains(printerId)) {
            printerIds.push_back(printerId);
        }
    }
    const QVariantMap cachedProjectsByPrinter = m_cache->loadRecentJobsForPrinters(printerIds, 20);
    perf.setCount("cached_jobs_printers", cachedProjectsByPrinter.size());
    const auto realtimeSnapshots = accloud::realtime::PrinterRealtimeStore::instance().snapshotAll();
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
        if (!printer.contains(QStringLiteral("details"))) {
            printer.insert(QStringLiteral("details"), QVariantMap{});
        }
        const QVariantList cachedProjects = cachedProjectsByPrinter.value(printerId).toList();
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
        applyRealtimeOverlayToPrinterMap(printer, realtimeSnapshots);
        printers.append(printer);
    }
    perf.setCount("items", printers.size());
    out.insert("ok", true);
    out.insert("message", printers.isEmpty()
                             ? QStringLiteral("Aucune imprimante en cache.")
                             : QStringLiteral("Imprimantes chargées depuis le cache local."));
    out.insert("messageKey", printers.isEmpty()
                             ? QStringLiteral("cache.printers.empty")
                             : QStringLiteral("cache.printers.loaded"));
    out.insert("fallbackMessage", printers.isEmpty()
                                  ? QStringLiteral("No printer available in local cache.")
                                  : QStringLiteral("Printers loaded from local cache."));
    out.insert("params", QVariantMap{{QStringLiteral("count"), printers.size()}});
    out.insert("printers", printers);
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::loadCachedPrintersAsync() {
    if (m_shuttingDown.load()) {
        return;
    }
    launchBackgroundTask([this]() {
        const QVariantMap result = loadCachedPrinters();
        QMetaObject::invokeMethod(this, [this, result]() {
            emit cachedPrintersLoaded(result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::loadCachedQuota() const {
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache quota indisponible."));
    out.insert("messageKey", QStringLiteral("cache.quota.unavailable"));
    out.insert("fallbackMessage", QStringLiteral("Local quota cache is unavailable."));
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
        out.insert("messageKey", QStringLiteral("cache.quota.empty"));
        out.insert("fallbackMessage", QStringLiteral("No cached quota available."));
        finalizeUiMessage(out);
        return out;
    }

    out.insert("ok", true);
    out.insert("message", QStringLiteral("Quota chargé depuis le cache local."));
    out.insert("messageKey", QStringLiteral("cache.quota.loaded"));
    out.insert("fallbackMessage", QStringLiteral("Quota loaded from local cache."));
    out.insert("totalDisplay", quota.value("totalDisplay"));
    out.insert("usedDisplay", quota.value("usedDisplay"));
    out.insert("totalBytes", quota.value("totalBytes"));
    out.insert("usedBytes", quota.value("usedBytes"));
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::loadCachedQuotaAsync() {
    if (m_shuttingDown.load()) {
        return;
    }
    launchBackgroundTask([this]() {
        const QVariantMap result = loadCachedQuota();
        QMetaObject::invokeMethod(this, [this, result]() {
            emit cachedQuotaLoaded(result);
        }, Qt::QueuedConnection);
    });
}

void CloudBridge::refreshFilesAsync(int page, int limit, bool force) {
    refreshFilesAsyncWithPolicy(page, limit, force, false);
}

void CloudBridge::refreshFilesAndThumbnailsAsync(int page, int limit, bool force) {
    refreshFilesAsyncWithPolicy(page, limit, force, true);
}

void CloudBridge::refreshFilesAsyncWithPolicy(int page,
                                              int limit,
                                              bool force,
                                              bool forceThumbnails) {
    if (m_shuttingDown.load()) {
        return;
    }
    if (m_refreshFilesRunning.exchange(true)) {
        return;
    }

    launchBackgroundTask([this, page, limit, force, forceThumbnails]() {
        if (m_shuttingDown.load()) {
            m_refreshFilesRunning.store(false);
            return;
        }

        if (!shouldRefresh(QStringLiteral("files"), 120, force)) {
            m_refreshFilesRunning.store(false);
            return;
        }

        const int scanPageSize = std::max(100, std::min(limit, 500));
        QString inventoryMessage;
        bool inventoryComplete = false;
        QVariantList files = fetchAllFilesWithRetry(
            scanPageSize, inventoryMessage, inventoryComplete, true, forceThumbnails);

        QString message = inventoryMessage;
        bool ok = inventoryComplete;
        if (!inventoryComplete) {
            QString fallbackMessage;
            bool fallbackOk = false;
            files = fetchFilesWithRetry(
                page, limit, fallbackMessage, fallbackOk, true, forceThumbnails);
            message = fallbackMessage;
            ok = fallbackOk;
        }

        if (m_cache != nullptr) {
            m_cache->updateSyncState(QStringLiteral("files"), ok, message);
        }

        if (ok) {
            if (m_cache != nullptr) {
                m_cache->replaceFiles(files);
            }

            QVariantList updateCandidates;
            qulonglong updateBytes = 0;
            if (inventoryComplete) {
                for (const QVariant& file : files) {
                    const QVariantMap map = file.toMap();
                    if (!map.value(QStringLiteral("thumbnailUpdateCandidate"), false).toBool()) {
                        continue;
                    }
                    QVariantMap candidate;
                    candidate.insert(QStringLiteral("fileId"), map.value(QStringLiteral("fileId")));
                    candidate.insert(QStringLiteral("fileName"), map.value(QStringLiteral("fileName")));
                    candidate.insert(QStringLiteral("sizeBytes"), map.value(QStringLiteral("sizeBytes")));
                    candidate.insert(QStringLiteral("time"), map.value(QStringLiteral("createTimeEpoch")));
                    updateCandidates.append(candidate);
                    updateBytes += map.value(QStringLiteral("sizeBytes")).toULongLong();
                }
            }

            QMetaObject::invokeMethod(this,
                                      [this, files, updateCandidates, updateBytes,
                                       inventoryComplete, inventoryMessage]() {
                emit filesUpdatedFromCloud(files, QStringLiteral("Cloud refresh terminé."));
                if (inventoryComplete && !updateCandidates.isEmpty()) {
                    emit pwszCloudPreviewUpdateSuggested(updateCandidates, updateBytes);
                }
                if (!inventoryComplete) {
                    emit syncFailed(QStringLiteral("pwsz_preview_candidates"),
                                    inventoryMessage);
                }
            },
                                      Qt::QueuedConnection);

            QString quotaMessage;
            bool quotaOk = false;
            QVariantMap quota = fetchQuotaWithRetry(quotaMessage, quotaOk);
            if (m_cache != nullptr) {
                m_cache->updateSyncState(QStringLiteral("quota"), quotaOk, quotaMessage);
            }
            if (quotaOk) {
                if (m_cache != nullptr) {
                    m_cache->saveQuota(quota);
                }
                QMetaObject::invokeMethod(this, [this, quota]() {
                    emit quotaUpdatedFromCloud(quota, QStringLiteral("Quota rafraîchi depuis le cloud."));
                }, Qt::QueuedConnection);
            } else {
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
    });
}

void CloudBridge::refreshPrintersAsync(bool force) {
    if (m_shuttingDown.load()) {
        return;
    }
    if (m_refreshPrintersRunning.exchange(true)) {
        return;
    }

    launchBackgroundTask([this, force]() {
        if (m_shuttingDown.load()) {
            m_refreshPrintersRunning.store(false);
            return;
        }
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
            QMetaObject::invokeMethod(this, [this, printers]() {
                emit printersUpdatedFromCloud(printers, QStringLiteral("Cloud refresh imprimantes terminé."));
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("printers"), message);
            }, Qt::QueuedConnection);
        }

        m_refreshPrintersRunning.store(false);
    });
}

void CloudBridge::refreshReasonCatalogAsync(bool force) {
    if (m_shuttingDown.load()) {
        return;
    }
    if (m_refreshReasonCatalogRunning.exchange(true)) {
        return;
    }

    launchBackgroundTask([this, force]() {
        if (m_shuttingDown.load()) {
            m_refreshReasonCatalogRunning.store(false);
            return;
        }
        const QString scope = QStringLiteral("reason_catalog");
        if (!shouldRefresh(scope, 3600, force)) {
            m_refreshReasonCatalogRunning.store(false);
            return;
        }

        const usecases::cloud::FetchReasonCatalogUseCase useCase;
        const auto r = useCase.execute();
        const bool ok = r.ok;
        const QString message = QString::fromStdString(r.message);

        if (m_cache != nullptr) {
            m_cache->updateSyncState(scope, ok, message);
        }

        if (ok) {
            QVariantList reasons;
            reasons.reserve(static_cast<qsizetype>(r.reasons.size()));
            for (const auto& item : r.reasons) {
                reasons.append(reasonCatalogItemToMap(item));
            }
            QMetaObject::invokeMethod(this, [this, reasons, message]() {
                emit reasonCatalogUpdatedFromCloud(reasons, message);
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("reason_catalog"), message);
            }, Qt::QueuedConnection);
        }

        m_refreshReasonCatalogRunning.store(false);
    });
}

void CloudBridge::refreshPrinterInsightsAsync(const QString& printerId, int page, int limit, bool force) {
    if (m_shuttingDown.load()) {
        return;
    }
    launchBackgroundTask([this, printerId, page, limit, force]() {
        if (m_shuttingDown.load()) {
            return;
        }
        const QString normalizedPrinterId = printerId.trimmed();
        if (normalizedPrinterId.isEmpty()) {
            return;
        }

        const QString scope = QStringLiteral("printer_insights_%1").arg(normalizedPrinterId);
        if (!shouldRefresh(scope, 15, force)) {
            return;
        }

        const usecases::cloud::FetchPrinterDetailsUseCase detailsUseCase;
        const auto detailsResult = detailsUseCase.execute(normalizedPrinterId.toStdString());
        const usecases::cloud::FetchPrinterProjectsUseCase projectsUseCase;
        const auto projectsResult = projectsUseCase.execute(normalizedPrinterId.toStdString(), page, limit);

        QVariantMap detailsMap;
        QString detailsRawJson;
        if constexpr (kDebugBuildEnabled) {
            detailsRawJson = QString::fromStdString(detailsResult.rawJson);
        }
        if (detailsResult.ok) {
            detailsMap = printerDetailsToMap(detailsResult);
            if (m_cache != nullptr) {
                m_cache->savePrinterDetails(normalizedPrinterId, detailsMap);
            }
        }

        QVariantList projects;
        QString projectsRawJson;
        if constexpr (kDebugBuildEnabled) {
            projectsRawJson = QString::fromStdString(projectsResult.rawJson);
        }
        if (projectsResult.ok) {
            projects.reserve(static_cast<qsizetype>(projectsResult.items.size()));
            for (const auto& item : projectsResult.items) {
                QVariantMap projectMap = printerProjectToMap(item);
                QString currentImage = projectMap.value(QStringLiteral("img")).toString();
                if (!localImageIsVisuallyUsable(currentImage)) {
                    const QString fallbackImage = resolveProjectImageFromFilesCache(
                        m_cache,
                        projectMap.value(QStringLiteral("currentFile")).toString(),
                        projectMap.value(QStringLiteral("gcodeName")).toString());
                    if (!fallbackImage.isEmpty()) {
                        projectMap.insert(QStringLiteral("img"), fallbackImage);
                    }
                }
                projects.append(projectMap);
            }
            if (m_cache != nullptr) {
                m_cache->upsertJobsForPrinter(normalizedPrinterId, projects);
                projects = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
            }
        } else if (m_cache != nullptr) {
            projects = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
            for (int i = 0; i < projects.size(); ++i) {
                QVariantMap projectMap = projects[i].toMap();
                const QString currentImage = projectMap.value(QStringLiteral("img")).toString();
                if (localImageIsVisuallyUsable(currentImage)) {
                    continue;
                }
                const QString fallbackImage = resolveProjectImageFromFilesCache(
                    m_cache,
                    projectMap.value(QStringLiteral("currentFile")).toString(),
                    projectMap.value(QStringLiteral("gcodeName")).toString());
                if (!fallbackImage.isEmpty()) {
                    projectMap.insert(QStringLiteral("img"), fallbackImage);
                    projects[i] = projectMap;
                }
            }
        }

        const bool ok = detailsResult.ok || projectsResult.ok || !projects.isEmpty();
        QString message;
        if (detailsResult.ok && projectsResult.ok) {
            message = QStringLiteral("Printer insights refreshed from cloud.");
        } else if (ok) {
            message = detailsResult.ok
                    ? QStringLiteral("Printer details refreshed; projects loaded from cache.")
                    : QStringLiteral("Printer projects refreshed; details unavailable.");
        } else {
            message = QString::fromStdString(!projectsResult.message.empty()
                    ? projectsResult.message
                    : detailsResult.message);
        }

        if (m_cache != nullptr) {
            m_cache->updateSyncState(scope, ok, message);
        }

        if (ok) {
            QMetaObject::invokeMethod(this, [this,
                                             normalizedPrinterId,
                                             detailsMap,
                                             projects,
                                             detailsRawJson,
                                             projectsRawJson,
                                             message]() {
                emit printerInsightsUpdatedFromCloud(normalizedPrinterId,
                                                     detailsMap,
                                                     projects,
                                                     detailsRawJson,
                                                     projectsRawJson,
                                                     message);
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit syncFailed(QStringLiteral("printer_insights"), message);
            }, Qt::QueuedConnection);
        }
    });
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
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.file.delete.ok")
                             : QStringLiteral("cloud.file.delete.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("Cloud file deleted.")
                                  : QStringLiteral("Failed to delete cloud file."));
    out.insert("params", QVariantMap{{QStringLiteral("fileId"), fileId}});
    if (r.ok && m_cache != nullptr) {
        m_cache->removeFile(fileId);
        m_cache->invalidateScope(QStringLiteral("files"));
    }
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::deleteFileAsync(const QString& fileId) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedFileId = fileId.trimmed();
    launchBackgroundTask([this, normalizedFileId]() {
        const QVariantMap result = deleteFile(normalizedFileId);
        QMetaObject::invokeMethod(this, [this, normalizedFileId, result]() {
            emit deleteFileFinished(normalizedFileId, result);
        }, Qt::QueuedConnection);
    });
}

// ── getDownloadUrl ────────────────────────────────────────────────────────

QVariantMap CloudBridge::getDownloadUrl(const QString& fileId) const {
    QVariantMap out;
    const usecases::cloud::GetDownloadUrlUseCase useCase;
    const auto r = useCase.execute(fileId.toStdString());
    out.insert("ok",      r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.file.download_url.ok")
                             : QStringLiteral("cloud.file.download_url.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("Download URL retrieved.")
                                  : QStringLiteral("Unable to retrieve download URL."));
    out.insert("params", QVariantMap{{QStringLiteral("fileId"), fileId}});
    if (r.ok)
        out.insert("url", QString::fromStdString(r.url));
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::getDownloadUrlAsync(const QString& fileId) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedFileId = fileId.trimmed();
    launchBackgroundTask([this, normalizedFileId]() {
        const QVariantMap result = getDownloadUrl(normalizedFileId);
        QMetaObject::invokeMethod(this, [this, normalizedFileId, result]() {
            emit downloadUrlReady(normalizedFileId, result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::inspectPwszPreview(const QString& localPath) const {
    QVariantMap out;
    const QString normalizedPath = normalizeUploadLocalPath(localPath);
    const QFileInfo info(normalizedPath);
    if (info.suffix().compare(QStringLiteral("pwsz"), Qt::CaseInsensitive) != 0) {
        out.insert("ok", true);
        out.insert("isPwsz", false);
        out.insert("needsCompletion", false);
        return out;
    }
    const auto inspection = accloud::cloud::archive::inspectPwszPreviewArchive(
        std::filesystem::path(normalizedPath.toStdString()));
    out.insert("ok", inspection.ok);
    out.insert("isPwsz", true);
    out.insert("hasPreview1", inspection.hasPreview1);
    out.insert("hasPreview2", inspection.hasPreview2);
    out.insert("needsCompletion", inspection.needsCompletion);
    out.insert("message", QString::fromStdString(inspection.message));
    return out;
}

QVariantMap CloudBridge::uploadLocalFile(const QString& localPath, bool completePwszPreview2) const {
    QVariantMap out;
    const QString normalizedPath = normalizeUploadLocalPath(localPath);
    const QFileInfo localFileInfo(normalizedPath);
    const QString localFileName = localFileInfo.fileName().trimmed().isEmpty()
                                      ? normalizedPath
                                      : localFileInfo.fileName();
    logging::info("app", "cloud_bridge", "upload_local_file_start",
                  "uploadLocalFile called",
                  {{"file_name", localFileName.toStdString()}});
    if (normalizedPath.isEmpty()) {
        out.insert("ok", false);
        out.insert("message", QStringLiteral("Chemin fichier vide."));
        out.insert("messageKey", QStringLiteral("cloud.upload.path_required"));
        out.insert("fallbackMessage", QStringLiteral("Local file path is required."));
        logging::warn("app", "cloud_bridge", "upload_local_file_invalid_path",
                      "uploadLocalFile aborted: empty path");
        finalizeUiMessage(out);
        return out;
    }

    const usecases::cloud::UploadLocalFileUseCase useCase;
    const auto r = useCase.execute(normalizedPath.toStdString(), completePwszPreview2);
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.upload.ok")
                             : QStringLiteral("cloud.upload.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("File uploaded to cloud.")
                                  : QStringLiteral("Failed to upload local file."));
    out.insert("params", QVariantMap{
        {QStringLiteral("fileName"), localFileName},
        {QStringLiteral("uploadStatus"), r.uploadStatus},
        {QStringLiteral("unlockOk"), r.unlockOk}
    });
    out.insert("fileId", QString::fromStdString(r.fileId));
    out.insert("gcodeId", QString::fromStdString(r.gcodeId));
    out.insert("uploadStatus", r.uploadStatus);
    out.insert("unlockOk", r.unlockOk);
    out.insert("previewAdded", r.previewAdded);
    out.insert("localFileSynchronized", r.localFileSynchronized);
    out.insert("recoveryPath", QString::fromStdString(r.recoveryPath));

    logging::info("app", "cloud_bridge", "upload_local_file_result",
                  "uploadLocalFile finished",
                  {{"ok", r.ok ? "1" : "0"},
                   {"file_name", localFileName.toStdString()},
                   {"file_id", r.fileId},
                   {"gcode_id", r.gcodeId.empty() ? "0" : r.gcodeId},
                   {"status", std::to_string(r.uploadStatus)},
                   {"unlock_ok", r.unlockOk ? "1" : "0"}});

    if (r.ok && m_cache != nullptr) {
        m_cache->invalidateScope(QStringLiteral("files"));
        m_cache->invalidateScope(QStringLiteral("quota"));
    }

    finalizeUiMessage(out);
    return out;
}

void CloudBridge::startUploadLocalFile(const QString& localPath, bool completePwszPreview2) {
    const QString normalizedPath = normalizeUploadLocalPath(localPath);
    if (normalizedPath.isEmpty()) {
        emit uploadFinished(false,
                            QStringLiteral("Chemin fichier vide."),
                            QString(),
                            QString(),
                            0,
                            false,
                            true);
        return;
    }

    emit uploadProgressChanged(0.0, QStringLiteral("Demarrage upload"));
    logging::info("app", "cloud_bridge", "upload_async_start",
                  "startUploadLocalFile called",
                  {{"file_path", normalizedPath.toStdString()}});

    launchBackgroundTask([this, normalizedPath, completePwszPreview2]() {
        const usecases::cloud::UploadLocalFileUseCase useCase;
        const auto result = useCase.execute(
            normalizedPath.toStdString(),
            completePwszPreview2,
            [this](double progress, const std::string& phase) {
                double clamped = progress;
                if (clamped < 0.0)
                    clamped = 0.0;
                if (clamped > 1.0)
                    clamped = 1.0;
                const QString phaseText = QString::fromStdString(phase);
                QMetaObject::invokeMethod(this, [this, clamped, phaseText]() {
                    emit uploadProgressChanged(clamped, phaseText);
                }, Qt::QueuedConnection);
            });

        QMetaObject::invokeMethod(this, [this, result]() {
            if (result.ok && m_cache != nullptr) {
                m_cache->invalidateScope(QStringLiteral("files"));
                m_cache->invalidateScope(QStringLiteral("quota"));
            }
            logging::info("app", "cloud_bridge", "upload_async_result",
                          "startUploadLocalFile finished",
                          {{"ok", result.ok ? "1" : "0"},
                           {"file_id", result.fileId},
                           {"gcode_id", result.gcodeId.empty() ? "0" : result.gcodeId},
                           {"status", std::to_string(result.uploadStatus)},
                           {"unlock_ok", result.unlockOk ? "1" : "0"}});
            emit uploadFinished(result.ok,
                                QString::fromStdString(result.message),
                                QString::fromStdString(result.fileId),
                                QString::fromStdString(result.gcodeId),
                                result.uploadStatus,
                                result.unlockOk,
                                result.localFileSynchronized);

            const bool ready = usecases::cloud::UploadLocalFileUseCase::isUploadReady(
                result.uploadStatus, result.gcodeId);
            if (result.ok && !ready && !m_shuttingDown.load()) {
                // Two bounded follow-up refreshes allow the cloud-generated preview to
                // appear after PROCESSING without an aggressive global polling loop.
                QTimer::singleShot(10000, this, [this]() {
                    if (!m_shuttingDown.load()) {
                        refreshFilesAsync(1, 20, true);
                    }
                });
                QTimer::singleShot(30000, this, [this]() {
                    if (!m_shuttingDown.load()) {
                        refreshFilesAsync(1, 20, true);
                    }
                });
            }
        }, Qt::QueuedConnection);
    });
}

void CloudBridge::startPwszCloudPreviewUpdate(const QVariantList& files) {
    if (m_shuttingDown.load() || m_pwszCloudUpdateRunning.exchange(true)) {
        return;
    }
    m_pwszCloudUpdateCancelRequested.store(false);

    std::vector<usecases::cloud::CloudPwszPreviewUpdateItem> items;
    items.reserve(static_cast<std::size_t>(files.size()));
    for (const QVariant& value : files) {
        const QVariantMap map = value.toMap();
        usecases::cloud::CloudPwszPreviewUpdateItem item;
        item.fileId = map.value(QStringLiteral("fileId")).toString().trimmed().toStdString();
        item.fileName = map.value(QStringLiteral("fileName")).toString().trimmed().toStdString();
        item.sizeBytes = map.value(QStringLiteral("sizeBytes")).toULongLong();
        item.createTime = map.value(QStringLiteral("time"),
                                    map.value(QStringLiteral("createTimeEpoch"))).toLongLong();
        if (!item.fileId.empty() && !item.fileName.empty()) {
            items.push_back(std::move(item));
        }
    }

    if (items.empty()) {
        m_pwszCloudUpdateRunning.store(false);
        QVariantMap summary;
        summary.insert(QStringLiteral("ok"), false);
        summary.insert(QStringLiteral("cancelled"), false);
        summary.insert(QStringLiteral("cancelledItems"), 0);
        summary.insert(QStringLiteral("modified"), 0);
        summary.insert(QStringLiteral("skipped"), 0);
        summary.insert(QStringLiteral("failed"), 0);
        summary.insert(QStringLiteral("partial"), 0);
        summary.insert(QStringLiteral("message"), QStringLiteral("Aucun fichier PWSZ sélectionné."));
        emit pwszCloudPreviewUpdateFinished(summary);
        return;
    }

    launchBackgroundTask([this, items = std::move(items)]() {
        const usecases::cloud::UpdateCloudPwszPreviewsUseCase useCase;
        const auto result = useCase.execute(
            items,
            [this](int current, int total, const std::string& fileName, const std::string& phase) {
                if (m_shuttingDown.load()) {
                    return;
                }
                const QString qFileName = QString::fromStdString(fileName);
                const QString qPhase = QString::fromStdString(phase);
                QMetaObject::invokeMethod(this, [this, current, total, qFileName, qPhase]() {
                    if (!m_shuttingDown.load()) {
                        emit pwszCloudPreviewUpdateProgress(current, total, qFileName, qPhase);
                    }
                }, Qt::QueuedConnection);
            },
            [this]() {
                return m_shuttingDown.load() || m_pwszCloudUpdateCancelRequested.load();
            },
            [](const std::string& url,
               const usecases::cloud::CancellationCallback& shouldCancel) {
                const ThumbnailResolveResult resolved = resolveThumbnailLocalUrl(
                    QString::fromStdString(url), true, true, shouldCancel);
                usecases::cloud::CloudPwszThumbnailValidationResult validation;
                validation.valid = !resolved.localUrl.isEmpty();
                validation.tooSmall = resolved.tooSmall;
                validation.cancelled = resolved.cancelled;
                validation.message = resolved.failureCategory.toStdString();
                return validation;
            });

        QVariantList itemResults;
        itemResults.reserve(static_cast<qsizetype>(result.items.size()));
        for (const auto& item : result.items) {
            QVariantMap map;
            map.insert(QStringLiteral("originalFileId"), QString::fromStdString(item.originalFileId));
            map.insert(QStringLiteral("newFileId"), QString::fromStdString(item.newFileId));
            map.insert(QStringLiteral("fileName"), QString::fromStdString(item.fileName));
            map.insert(QStringLiteral("status"), QString::fromStdString(item.status));
            map.insert(QStringLiteral("message"), QString::fromStdString(item.message));
            itemResults.append(map);
        }

        QVariantMap summary;
        summary.insert(QStringLiteral("ok"), result.ok);
        summary.insert(QStringLiteral("cancelled"), result.cancelled);
        summary.insert(QStringLiteral("cancelledItems"), result.cancelledItems);
        summary.insert(QStringLiteral("modified"), result.modified);
        summary.insert(QStringLiteral("skipped"), result.skipped);
        summary.insert(QStringLiteral("failed"), result.failed);
        summary.insert(QStringLiteral("partial"), result.partial);
        summary.insert(QStringLiteral("items"), itemResults);

        if (m_shuttingDown.load()) {
            m_pwszCloudUpdateRunning.store(false);
            return;
        }
        QMetaObject::invokeMethod(this, [this, summary]() {
            m_pwszCloudUpdateRunning.store(false);
            m_pwszCloudUpdateCancelRequested.store(false);
            if (m_cache != nullptr) {
                m_cache->invalidateScope(QStringLiteral("files"));
                m_cache->invalidateScope(QStringLiteral("quota"));
            }
            emit pwszCloudPreviewUpdateFinished(summary);
            if (!m_shuttingDown.load()) {
                refreshFilesAsync(1, 20, true);
            }
        }, Qt::QueuedConnection);
    });
}

void CloudBridge::cancelPwszCloudPreviewUpdate() {
    if (m_pwszCloudUpdateRunning.load()) {
        m_pwszCloudUpdateCancelRequested.store(true);
    }
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

void CloudBridge::fetchCompatiblePrintersByExtAsync(const QString& fileExt,
                                                    const QString& requestId) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedExt = fileExt.trimmed().toLower();
    const QString normalizedRequestId = requestId.trimmed();
    launchBackgroundTask([this, normalizedExt, normalizedRequestId]() {
        const QVariantMap result = fetchCompatiblePrintersByExt(normalizedExt);
        QMetaObject::invokeMethod(this, [this, normalizedRequestId, normalizedExt, result]() {
            emit compatiblePrintersByExtReady(normalizedRequestId, normalizedExt, result);
        }, Qt::QueuedConnection);
    });
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

void CloudBridge::fetchCompatiblePrintersByFileIdAsync(const QString& fileId,
                                                       const QString& requestId) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedFileId = fileId.trimmed();
    const QString normalizedRequestId = requestId.trimmed();
    launchBackgroundTask([this, normalizedFileId, normalizedRequestId]() {
        const QVariantMap result = fetchCompatiblePrintersByFileId(normalizedFileId);
        QMetaObject::invokeMethod(this, [this, normalizedRequestId, normalizedFileId, result]() {
            emit compatiblePrintersByFileIdReady(normalizedRequestId, normalizedFileId, result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::evaluateLocalPrinterFileCompatibility(const QVariantMap& printer,
                                                               const QVariantMap& file) const {
    if (printer.isEmpty()) {
        return localCompatResult(false, 0, QStringLiteral("Select a printer first."));
    }
    if (file.isEmpty()) {
        return localCompatResult(false, 0, QStringLiteral("Select a cloud file first."));
    }

    const QString ext = fileExtension(file.value(QStringLiteral("fileName")).toString());
    if (!isKnownCloudSliceExtension(ext)) {
        return localCompatResult(false, 0, QStringLiteral("Unsupported file format."));
    }

    const QString printerMachineType = normalizedCompatText(printer.value(QStringLiteral("machineType")));
    QString fileMachineType = normalizedCompatText(file.value(QStringLiteral("machineType")));
    if (fileMachineType.isEmpty()) {
        fileMachineType = normalizedCompatText(file.value(QStringLiteral("machineTypeId")));
    }

    if (!fileMachineType.isEmpty() && !printerMachineType.isEmpty()) {
        if (fileMachineType == printerMachineType) {
            return localCompatResult(true, 500, {});
        }
        return localCompatResult(false, 0, QStringLiteral("Slice file does not match machine type."));
    }

    const QString machineText = normalizedCompatText(file.value(QStringLiteral("machine")));
    const QString printersText = normalizedCompatText(file.value(QStringLiteral("printers")));
    const QString metadataText = (machineText + QLatin1Char(' ') + printersText).trimmed();
    if (metadataText.isEmpty()) {
        return localCompatResult(false, 0, QStringLiteral("Missing local compatibility metadata."));
    }

    const QString printerModel = normalizedCompatText(printer.value(QStringLiteral("model")));
    const QString printerName = normalizedCompatText(printer.value(QStringLiteral("name")));

    if (!machineText.isEmpty() && (machineText == printerModel || machineText == printerName)) {
        return localCompatResult(true, 420, {});
    }
    if (compatTextContains(machineText, printerModel)
        || compatTextContains(printerModel, machineText)
        || compatTextContains(machineText, printerName)
        || compatTextContains(printerName, machineText)) {
        return localCompatResult(true, 360, {});
    }
    if (compatTextContains(printersText, printerModel)
        || compatTextContains(printersText, printerName)
        || compatTextContains(printerModel, printersText)
        || compatTextContains(printerName, printersText)) {
        return localCompatResult(true, 330, {});
    }

    const QStringList metadataTokens = compatTokens(metadataText);
    const QStringList printerTokens = compatTokens(printerModel + QLatin1Char(' ')
                                                  + printerName + QLatin1Char(' ')
                                                  + printerMachineType);
    const int overlapCount = compatTokenOverlapCount(metadataTokens, printerTokens);
    if (overlapCount > 0) {
        return localCompatResult(true, 280 + overlapCount, {});
    }

    return localCompatResult(false, 0, QStringLiteral("Slice file does not match selected printer model."));
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
        for (const auto& item : r.items) {
            QVariantMap projectMap = printerProjectToMap(item);
            const QString currentImage = projectMap.value(QStringLiteral("img")).toString();
            if (!localImageIsVisuallyUsable(currentImage)) {
                const QString fallbackImage = resolveProjectImageFromFilesCache(
                    m_cache,
                    projectMap.value(QStringLiteral("currentFile")).toString(),
                    projectMap.value(QStringLiteral("gcodeName")).toString());
                if (!fallbackImage.isEmpty()) {
                    projectMap.insert(QStringLiteral("img"), fallbackImage);
                }
            }
            projects.append(projectMap);
        }
        out.insert("projects", projects);
        if (m_cache != nullptr && !normalizedPrinterId.isEmpty()) {
            m_cache->upsertJobsForPrinter(normalizedPrinterId, projects);
            out.insert("projects", m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit));
        }
    } else if (m_cache != nullptr && !normalizedPrinterId.isEmpty()) {
        const QVariantList cached = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
        if (!cached.isEmpty()) {
            QVariantList enriched = cached;
            for (int i = 0; i < enriched.size(); ++i) {
                QVariantMap projectMap = enriched[i].toMap();
                const QString currentImage = projectMap.value(QStringLiteral("img")).toString();
                if (localImageIsVisuallyUsable(currentImage)) {
                    continue;
                }
                const QString fallbackImage = resolveProjectImageFromFilesCache(
                    m_cache,
                    projectMap.value(QStringLiteral("currentFile")).toString(),
                    projectMap.value(QStringLiteral("gcodeName")).toString());
                if (!fallbackImage.isEmpty()) {
                    projectMap.insert(QStringLiteral("img"), fallbackImage);
                    enriched[i] = projectMap;
                }
            }
            out.insert("ok", true);
            out.insert("message", QStringLiteral("Jobs chargés depuis le cache local."));
            out.insert("projects", enriched);
        }
    }
    finalizeUiMessage(out);
    return out;
}

QVariantMap CloudBridge::loadCachedPrinterProjects(const QString& printerId, int page, int limit) const {
    UiPerfTrace perf("cloud_bridge.load_cached_printer_projects");
    perf.setField("page", std::to_string(page));
    perf.setField("limit", std::to_string(limit));
    QVariantMap out;
    out.insert("ok", false);
    out.insert("message", QStringLiteral("Cache local indisponible."));
    out.insert("messageKey", QStringLiteral("cache.printer_projects.unavailable"));
    out.insert("fallbackMessage", QStringLiteral("Local printer jobs cache is unavailable."));
    out.insert("projects", QVariantList{});
    if constexpr (kDebugBuildEnabled) {
        out.insert("rawJson", QString{});
    }

    const QString normalizedPrinterId = printerId.trimmed();
    perf.setField("printer_id_present", normalizedPrinterId.isEmpty() ? "0" : "1");
    if (normalizedPrinterId.isEmpty()) {
        out.insert("message", QStringLiteral("printer_id requis."));
        out.insert("messageKey", QStringLiteral("cloud.printer_id.required"));
        out.insert("fallbackMessage", QStringLiteral("Printer identifier is required."));
        finalizeUiMessage(out);
        return out;
    }

    if (m_cache == nullptr || !m_cache->isAvailable()) {
        finalizeUiMessage(out);
        return out;
    }

    const QVariantList projects = m_cache->loadJobsForPrinter(normalizedPrinterId, page, limit);
    perf.setCount("items", projects.size());
    out.insert("ok", true);
    out.insert("message", projects.isEmpty()
                             ? QStringLiteral("Aucun job en cache pour cette imprimante.")
                             : QStringLiteral("Jobs chargés depuis le cache local."));
    out.insert("messageKey", projects.isEmpty()
                             ? QStringLiteral("cache.printer_projects.empty")
                             : QStringLiteral("cache.printer_projects.loaded"));
    out.insert("fallbackMessage", projects.isEmpty()
                                  ? QStringLiteral("No cached print job for this printer.")
                                  : QStringLiteral("Printer jobs loaded from local cache."));
    out.insert("params", QVariantMap{
        {QStringLiteral("printerId"), normalizedPrinterId},
        {QStringLiteral("count"), projects.size()},
        {QStringLiteral("page"), page},
        {QStringLiteral("limit"), limit}
    });
    out.insert("projects", projects);
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::loadCachedPrinterProjectsAsync(const QString& printerId, int page, int limit) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedPrinterId = printerId.trimmed();
    launchBackgroundTask([this, normalizedPrinterId, page, limit]() {
        const QVariantMap result = loadCachedPrinterProjects(normalizedPrinterId, page, limit);
        QMetaObject::invokeMethod(this, [this, normalizedPrinterId, result]() {
            emit cachedPrinterProjectsLoaded(normalizedPrinterId, result);
        }, Qt::QueuedConnection);
    });
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
        out.insert("messageKey", QStringLiteral("cloud.print_order.dry_run"));
        out.insert("fallbackMessage", QStringLiteral("Print order payload generated (dry-run)."));
        out.insert("params", QVariantMap{
            {QStringLiteral("printerId"), printerId},
            {QStringLiteral("fileId"), fileId}
        });
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
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.print_order.sent")
                             : QStringLiteral("cloud.print_order.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("Print order sent.")
                                  : QStringLiteral("Failed to send print order."));
    out.insert("params", QVariantMap{
        {QStringLiteral("printerId"), printerId},
        {QStringLiteral("fileId"), fileId}
    });
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

void CloudBridge::sendPrintOrderAsync(const QString& printerId,
                                      const QString& fileId,
                                      bool deleteAfterPrint,
                                      bool dryRun) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedPrinterId = printerId.trimmed();
    const QString normalizedFileId = fileId.trimmed();
    launchBackgroundTask([this, normalizedPrinterId, normalizedFileId, deleteAfterPrint, dryRun]() {
        const QVariantMap result = sendPrintOrder(normalizedPrinterId,
                                                  normalizedFileId,
                                                  deleteAfterPrint,
                                                  dryRun);
        QMetaObject::invokeMethod(this, [this, normalizedPrinterId, normalizedFileId, result]() {
            emit printOrderFinished(normalizedPrinterId, normalizedFileId, result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap CloudBridge::sendPrinterOrder(const QString& printerId,
                                          int orderId,
                                          const QVariantMap& data,
                                          const QString& projectId) const {
    QVariantMap out;
    const QString normalizedPrinterId = printerId.trimmed();
    if (normalizedPrinterId.isEmpty()) {
        out.insert("ok", false);
        out.insert("message", QStringLiteral("printer_id requis."));
        out.insert("messageKey", QStringLiteral("cloud.printer_id.required"));
        out.insert("fallbackMessage", QStringLiteral("Printer identifier is required."));
        finalizeUiMessage(out);
        return out;
    }
    if (orderId <= 0) {
        out.insert("ok", false);
        out.insert("message", QStringLiteral("order_id invalide."));
        out.insert("messageKey", QStringLiteral("cloud.order_id.invalid"));
        out.insert("fallbackMessage", QStringLiteral("Order identifier is invalid."));
        out.insert("params", QVariantMap{{QStringLiteral("orderId"), orderId}});
        finalizeUiMessage(out);
        return out;
    }

    const usecases::cloud::SendPrinterOrderUseCase useCase;
    const std::string dataJson = compactJsonFromVariantMap(data);
    const auto r = useCase.execute(normalizedPrinterId.toStdString(),
                                   orderId,
                                   projectId.trimmed().toStdString(),
                                   dataJson);
    out.insert("ok", r.ok);
    out.insert("message", QString::fromStdString(r.message));
    out.insert("messageKey", r.ok
                             ? QStringLiteral("cloud.printer_order.sent")
                             : QStringLiteral("cloud.printer_order.failed"));
    out.insert("fallbackMessage", r.ok
                                  ? QStringLiteral("Printer order sent.")
                                  : QStringLiteral("Failed to send printer order."));
    out.insert("params", QVariantMap{
        {QStringLiteral("printerId"), normalizedPrinterId},
        {QStringLiteral("orderId"), orderId},
        {QStringLiteral("projectId"), projectId.trimmed()}
    });
    out.insert("taskId", QString::fromStdString(r.taskId));
    out.insert("msgId", QString::fromStdString(r.msgId));
    if (r.ok && m_cache != nullptr) {
        m_cache->invalidateScope(QStringLiteral("printers"));
    }
    finalizeUiMessage(out);
    return out;
}

void CloudBridge::sendPrinterOrderAsync(const QString& printerId,
                                        int orderId,
                                        const QVariantMap& data,
                                        const QString& projectId,
                                        const QString& context) {
    if (m_shuttingDown.load()) {
        return;
    }
    const QString normalizedPrinterId = printerId.trimmed();
    const QString normalizedProjectId = projectId.trimmed();
    const QString normalizedContext = context.trimmed();
    launchBackgroundTask([this, normalizedPrinterId, orderId, data, normalizedProjectId, normalizedContext]() {
        const QVariantMap result = sendPrinterOrder(normalizedPrinterId,
                                                    orderId,
                                                    data,
                                                    normalizedProjectId);
        QMetaObject::invokeMethod(this, [this, normalizedContext, normalizedPrinterId, orderId, result]() {
            emit printerOrderFinished(normalizedContext, normalizedPrinterId, orderId, result);
        }, Qt::QueuedConnection);
    });
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

    const std::string rawCorrelationId = logging::raw::nextCorrelationId("http");
    logging::raw::logHttpRequest(rawCorrelationId,
                                 "GET",
                                 signedUrl.toStdString(),
                                 rawRequestHeaders(req),
                                 {});
    m_dlReply = m_nam->get(req);

    connect(m_dlReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_dlFile) m_dlFile->write(m_dlReply->readAll());
    });

    connect(m_dlReply, &QNetworkReply::downloadProgress,
            this, [this](qint64 recv, qint64 total) {
                emit downloadProgress(recv, total);
            });

    connect(m_dlReply, &QNetworkReply::finished, this, [this, rawCorrelationId]() {
        const bool netOk = (m_dlReply->error() == QNetworkReply::NoError);
        const QString errStr = m_dlReply->errorString();
        const QString path   = m_dlPath;
        const int httpStatus = m_dlReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const qint64 streamedBytes = m_dlFile ? m_dlFile->size() : 0;
        logging::raw::logHttpResponse(
            rawCorrelationId,
            httpStatus,
            m_dlReply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString(),
            rawResponseHeaders(*m_dlReply),
            "<binary download streamed to disk: " + std::to_string(streamedBytes) + " bytes>",
            netOk ? std::string{} : errStr.toStdString());

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
