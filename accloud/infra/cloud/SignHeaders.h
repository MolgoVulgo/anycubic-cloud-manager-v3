#pragma once

#ifdef ACCLOUD_WITH_QT

#include <QNetworkRequest>
#include <QString>

namespace accloud::cloud {

struct WorkbenchHeaderStatus {
    bool ok{false};
    QString error;
};

// Applies XX-* Workbench signature headers to req.
// Must be called for all paths containing /p/p/workbench/api/
// xxToken: optional value from session.tokens["token"] (feeds XX-Token)
WorkbenchHeaderStatus applyWorkbenchHeaders(QNetworkRequest& req, const QString& xxToken = {});

} // namespace accloud::cloud

#endif // ACCLOUD_WITH_QT
