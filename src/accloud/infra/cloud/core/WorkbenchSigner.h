#pragma once

#ifdef ACCLOUD_WITH_QT

#include <QNetworkRequest>
#include <QString>

namespace accloud::cloud::core {

struct SignStatus {
    bool ok{false};
    QString error;
};

class WorkbenchSigner {
public:
    SignStatus apply(QNetworkRequest& req, const QString& xxToken = {}) const;
};

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
