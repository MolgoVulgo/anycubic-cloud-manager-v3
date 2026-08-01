#include "ThumbnailService.h"

#include "app/LocalCacheStore.h"
#include "app/ThumbnailCachePolicy.h"
#include "infra/cloud/thumbnail/ThumbnailValidation.h"
#include "infra/config/AppPaths.h"
#include "infra/logging/JsonlLogger.h"
#include "infra/logging/RawTrafficLogger.h"
#include "infra/logging/Redactor.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSslError>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <filesystem>
#include <string>

namespace accloud::thumbnail_service {

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

QString fileExtension(const QString& fileName) {
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot < 0 || dot + 1 >= fileName.size()) {
        return {};
    }
    return fileName.mid(dot + 1).toLower();
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

struct ThumbnailFailureMemo {
    qint64 retryAfter{0};
    bool permanent{false};
    bool tooSmall{false};
    QString category;
};

QMutex g_thumbnailFailureMutex;
QHash<QByteArray, ThumbnailFailureMemo> g_thumbnailFailures;
constexpr int kThumbnailTimeoutMs = 12000;
constexpr qint64 kThumbnailTransientRetryDelaySec = 15;
constexpr qint64 kThumbnailNonPermanentRetryDelaySec = 300;

QByteArray thumbnailFailureKey(const QString& source) {
    return thumbnail_cache::stableCacheKey(source);
}

bool thumbnailRetryIsDeferred(const QByteArray& failureKey,
                              ThumbnailFailureMemo* deferredFailure = nullptr) {
    if (failureKey.isEmpty()) {
        return false;
    }

    QMutexLocker lock(&g_thumbnailFailureMutex);
    const auto existing = g_thumbnailFailures.constFind(failureKey);
    if (existing == g_thumbnailFailures.cend()) {
        return false;
    }
    if (!existing->permanent
        && existing->retryAfter <= QDateTime::currentSecsSinceEpoch()) {
        g_thumbnailFailures.remove(failureKey);
        return false;
    }
    if (deferredFailure != nullptr) {
        *deferredFailure = *existing;
    }
    return true;
}

void rememberThumbnailFailure(const QByteArray& failureKey,
                              bool retryable,
                              bool tooSmall,
                              const QString& category) {
    if (failureKey.isEmpty()) {
        return;
    }

    ThumbnailFailureMemo memo;
    memo.permanent = thumbnail_cache::isPermanentContentFailure(category);
    if (memo.permanent) {
        memo.retryAfter = 0;
    } else {
        const qint64 delay = retryable
            ? kThumbnailTransientRetryDelaySec
            : kThumbnailNonPermanentRetryDelaySec;
        memo.retryAfter = QDateTime::currentSecsSinceEpoch() + delay;
    }
    memo.tooSmall = tooSmall;
    memo.category = category;

    QMutexLocker lock(&g_thumbnailFailureMutex);
    g_thumbnailFailures.insert(failureKey, memo);
}

void clearThumbnailFailure(const QByteArray& failureKey) {
    if (failureKey.isEmpty()) {
        return;
    }
    QMutexLocker lock(&g_thumbnailFailureMutex);
    g_thumbnailFailures.remove(failureKey);
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
    bool forceDownload,
    const usecases::cloud::CancellationCallback& shouldCancel) {
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
    const QByteArray failureKey = thumbnailFailureKey(normalized);
    if (forceDownload) {
        removeCachedThumbnailFiles(cacheBasePath);
        clearThumbnailFailure(failureKey);
    } else {
        const QString cachedPath = findReadableCachedImage(cacheBasePath);
        if (!cachedPath.isEmpty()) {
            clearThumbnailFailure(failureKey);
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
    ThumbnailFailureMemo deferredFailure;
    if (!forceDownload && thumbnailRetryIsDeferred(failureKey, &deferredFailure)) {
        out.tooSmall = deferredFailure.tooSmall;
        out.failureCategory = deferredFailure.category.isEmpty()
            ? QStringLiteral("retry_deferred")
            : deferredFailure.category;
        return out;
    }

    const ThumbnailFetchResult fetch = fetchThumbnailToCache(normalized, cacheBasePath, shouldCancel);
    if (fetch.cancelled) {
        out.cancelled = true;
        out.failureCategory = QStringLiteral("cancelled");
        return out;
    }
    if (fetch.localUrl.isEmpty()) {
        rememberThumbnailFailure(failureKey,
                                 fetch.retryable,
                                 fetch.tooSmall,
                                 fetch.failureCategory);
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
    clearThumbnailFailure(failureKey);
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

void resolveThumbnailInMap(QVariantMap& map, bool downloadMissing, bool forceDownload) {
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


} // namespace accloud::thumbnail_service
