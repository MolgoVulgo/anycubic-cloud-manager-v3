#include "WorkbenchRequestBuilder.h"

#ifdef ACCLOUD_WITH_QT

#include "infra/logging/JsonlLogger.h"

#include <QUrl>
#include <utility>

namespace accloud::cloud::core {
namespace {

QString buildPathWithQuery(const char* path, const QString& queryString) {
    QString fullPath = QString::fromUtf8(path ? path : "");
    if (!queryString.isEmpty()) {
        if (queryString.startsWith('?')) {
            fullPath += queryString;
        } else {
            fullPath += '?';
            fullPath += queryString;
        }
    }
    return fullPath;
}

} // namespace

WorkbenchRequestBuilder::WorkbenchRequestBuilder(WorkbenchSigner signer)
    : signer_(std::move(signer)) {}

std::optional<BuiltRequest> WorkbenchRequestBuilder::build(const EndpointDefinition& endpoint,
                                                           const std::string& accessToken,
                                                           const std::string& xxToken,
                                                           const QString& queryString,
                                                           const QByteArray& body) const {
    if (!endpoint.path || !*endpoint.path) {
        return std::nullopt;
    }
    const QString fullPath = buildPathWithQuery(endpoint.path, queryString);
    const QUrl url(QString::fromLatin1("https://cloud-universe.anycubic.com") + fullPath);
    QNetworkRequest req(url);

    if (endpoint.contentType && *endpoint.contentType) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, endpoint.contentType);
    }
    if (endpoint.requiresBearer) {
        req.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(accessToken));
    }
    req.setTransferTimeout(endpoint.timeoutMs);

    if (endpoint.requiresWorkbenchSignature) {
        const SignStatus status = signer_.apply(req, QString::fromStdString(xxToken));
        if (!status.ok) {
            logging::warn("app", "cloud_core", "workbench_headers_missing",
                          "Workbench signature headers are not configured; request sent without XX-*",
                          {{"reason", status.error.toStdString()}});
        }
    }

    return BuiltRequest{std::move(req), body, endpoint.method};
}

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
