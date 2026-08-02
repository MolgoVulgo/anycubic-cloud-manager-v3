#pragma once

#include "app/usecases/cloud/UpdateCloudPwszPreviewsUseCase.h"

#include <QString>
#include <QVariantMap>

namespace accloud::thumbnail_service {

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

} // namespace accloud::thumbnail_service
