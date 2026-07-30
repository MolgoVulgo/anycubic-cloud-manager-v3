#include "ThumbnailCachePolicy.h"

#include <QCryptographicHash>
#include <QUrl>

namespace accloud::thumbnail_cache {
namespace {

bool isLocalSource(const QString& source) {
    const QUrl url(source.trimmed());
    return url.isLocalFile();
}

void appendUnique(QStringList& output, const QString& source) {
    const QString value = source.trimmed();
    if (!value.isEmpty() && !output.contains(value)) {
        output.append(value);
    }
}

} // namespace

QString canonicalSourceIdentity(const QString& source) {
    const QString value = source.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    QUrl url(value);
    if (!url.isValid() || url.scheme().isEmpty()) {
        return value;
    }

    const QString scheme = url.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        return value;
    }

    url.setScheme(scheme);
    url.setHost(url.host().toLower());
    url.setUserInfo(QString{});
    url.setQuery(QString{});
    url.setFragment(QString{});
    return url.toString(QUrl::FullyEncoded);
}

QByteArray stableCacheKey(const QString& source) {
    return QCryptographicHash::hash(canonicalSourceIdentity(source).toUtf8(),
                                    QCryptographicHash::Sha1)
        .toHex();
}

QStringList orderedCandidates(const QStringList& candidates,
                              const QString& persistedSource,
                              const QString& displayedSource,
                              bool forceDownload) {
    QStringList ordered;
    ordered.reserve(candidates.size() + 2);

    if (forceDownload) {
        for (const QString& candidate : candidates) {
            appendUnique(ordered, candidate);
        }
        appendUnique(ordered, persistedSource);
        appendUnique(ordered, displayedSource);
        return ordered;
    }

    if (isLocalSource(displayedSource)) {
        appendUnique(ordered, displayedSource);
    }
    if (isLocalSource(persistedSource)) {
        appendUnique(ordered, persistedSource);
    }
    for (const QString& candidate : candidates) {
        if (isLocalSource(candidate)) {
            appendUnique(ordered, candidate);
        }
    }

    if (!isLocalSource(persistedSource)) {
        appendUnique(ordered, persistedSource);
    }
    for (const QString& candidate : candidates) {
        if (!isLocalSource(candidate)) {
            appendUnique(ordered, candidate);
        }
    }
    if (!isLocalSource(displayedSource)) {
        appendUnique(ordered, displayedSource);
    }
    return ordered;
}

bool shouldReuseValidatedLocalCandidate(const QString& source,
                                        bool validationKnownValid,
                                        bool forceDownload) {
    return validationKnownValid && !forceDownload && isLocalSource(source);
}

} // namespace accloud::thumbnail_cache
