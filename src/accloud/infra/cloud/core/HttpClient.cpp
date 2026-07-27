#include "HttpClient.h"

#ifdef ACCLOUD_WITH_QT

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

#include "infra/logging/RawTrafficLogger.h"

namespace accloud::cloud::core {
namespace {

logging::raw::HeaderList requestHeaders(const QNetworkRequest& request) {
    logging::raw::HeaderList headers;
    const auto names = request.rawHeaderList();
    headers.reserve(static_cast<std::size_t>(names.size()));
    for (const QByteArray& name : names) {
        headers.emplace_back(name.toStdString(), request.rawHeader(name).toStdString());
    }
    return headers;
}

logging::raw::HeaderList responseHeaders(const QNetworkReply& reply) {
    logging::raw::HeaderList headers;
    const auto pairs = reply.rawHeaderPairs();
    headers.reserve(static_cast<std::size_t>(pairs.size()));
    for (const auto& pair : pairs) {
        headers.emplace_back(pair.first.toStdString(), pair.second.toStdString());
    }
    return headers;
}

const char* methodName(HttpMethod method) {
    switch (method) {
        case HttpMethod::Get:
            return "GET";
        case HttpMethod::Post:
            return "POST";
    }
    return "UNKNOWN";
}

} // namespace

HttpResponse HttpClient::execute(const BuiltRequest& built) const {
    QNetworkAccessManager nam;
    QEventLoop loop;
    QNetworkReply* reply = nullptr;
    const std::string rawCorrelationId = logging::raw::nextCorrelationId("http");
    logging::raw::logHttpRequest(
        rawCorrelationId,
        methodName(built.method),
        built.request.url().toString(QUrl::FullyEncoded).toStdString(),
        requestHeaders(built.request),
        built.body.toStdString());

    switch (built.method) {
        case HttpMethod::Get:
            reply = nam.get(built.request);
            break;
        case HttpMethod::Post:
            reply = nam.post(built.request, built.body);
            break;
    }

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    HttpResponse response;
    response.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.error = reply->errorString().toStdString();
    const QByteArray responseBytes = reply->readAll();
    response.body = responseBytes.toStdString();
    response.ok = (reply->error() == QNetworkReply::NoError);
    logging::raw::logHttpResponse(
        rawCorrelationId,
        response.httpStatus,
        reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString(),
        responseHeaders(*reply),
        response.body,
        response.ok ? std::string{} : response.error);
    reply->deleteLater();
    return response;
}

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
