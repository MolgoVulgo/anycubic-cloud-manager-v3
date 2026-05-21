#pragma once

#ifdef ACCLOUD_WITH_QT

#include "EndpointRegistry.h"
#include "WorkbenchSigner.h"

#include <QByteArray>
#include <QNetworkRequest>
#include <QString>

#include <optional>
#include <string>

namespace accloud::cloud::core {

struct BuiltRequest {
    QNetworkRequest request;
    QByteArray body;
    HttpMethod method{HttpMethod::Get};
};

class WorkbenchRequestBuilder {
public:
    explicit WorkbenchRequestBuilder(WorkbenchSigner signer = {});

    std::optional<BuiltRequest> build(const EndpointDefinition& endpoint,
                                      const std::string& accessToken,
                                      const std::string& xxToken,
                                      const QString& queryString = {},
                                      const QByteArray& body = {}) const;

private:
    WorkbenchSigner signer_;
};

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
