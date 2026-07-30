#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace accloud::thumbnail_cache {

QString canonicalSourceIdentity(const QString& source);
QByteArray stableCacheKey(const QString& source);
QStringList orderedCandidates(const QStringList& candidates,
                              const QString& persistedSource,
                              const QString& displayedSource,
                              bool forceDownload);
bool shouldReuseValidatedLocalCandidate(const QString& source,
                                        bool validationKnownValid,
                                        bool forceDownload);

} // namespace accloud::thumbnail_cache
