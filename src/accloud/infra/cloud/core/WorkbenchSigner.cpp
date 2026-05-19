#include "WorkbenchSigner.h"

#ifdef ACCLOUD_WITH_QT

#include "infra/cloud/SignHeaders.h"

namespace accloud::cloud::core {

SignStatus WorkbenchSigner::apply(QNetworkRequest& req, const QString& xxToken) const {
    const accloud::cloud::WorkbenchHeaderStatus status =
        accloud::cloud::applyWorkbenchHeaders(req, xxToken);
    return SignStatus{status.ok, status.error};
}

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
