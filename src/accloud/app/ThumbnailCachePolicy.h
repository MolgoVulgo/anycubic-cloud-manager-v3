#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace accloud::thumbnail_cache {

enum class ThumbnailRefreshPolicy {
    CacheOnly,
    NewFilesOnly,
    MissingThumbnails,
    ForceAll,
};

struct ThumbnailRefreshDecision {
    bool downloadMissing{false};
    bool forceDownload{false};
};

ThumbnailRefreshDecision refreshDecision(ThumbnailRefreshPolicy policy,
                                         bool hasInventoryBaseline,
                                         bool fileAlreadyKnown);

QString canonicalSourceIdentity(const QString& source);
QByteArray stableCacheKey(const QString& source);
QStringList orderedCandidates(const QStringList& candidates,
                              const QString& persistedSource,
                              const QString& displayedSource,
                              bool forceDownload);
bool shouldReuseValidatedLocalCandidate(const QString& source,
                                        bool validationKnownValid,
                                        bool forceDownload);
bool isPermanentContentFailure(const QString& category);

} // namespace accloud::thumbnail_cache
