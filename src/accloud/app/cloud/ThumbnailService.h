#pragma once

#include "app/usecases/cloud/UpdateCloudPwszPreviewsUseCase.h"

#include <QString>
#include <QVariantMap>

namespace accloud {
class LocalCacheStore;

namespace thumbnail_service {

struct ThumbnailResolveResult {
    QString localUrl;
    bool tooSmall{false};
    bool cancelled{false};
    bool localValidationReused{false};
    QString failureCategory;
};

ThumbnailResolveResult resolveThumbnailLocalUrl(
    const QString& source,
    bool downloadMissing,
    bool forceDownload = false,
    const usecases::cloud::CancellationCallback& shouldCancel = {});

void resolveThumbnailInMap(QVariantMap& map,
                           bool downloadMissing,
                           bool forceDownload = false);

bool localImageIsVisuallyUsable(const QString& sourceUrl);

QString resolveProjectImageFromFilesCache(const LocalCacheStore* cache,
                                          const QString& currentFile,
                                          const QString& gcodeName);

} // namespace thumbnail_service
} // namespace accloud
