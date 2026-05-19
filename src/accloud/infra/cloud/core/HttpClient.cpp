#include "HttpClient.h"

#ifdef ACCLOUD_WITH_QT

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace accloud::cloud::core {

HttpResponse HttpClient::execute(const BuiltRequest& built) const {
    QNetworkAccessManager nam;
    QEventLoop loop;
    QNetworkReply* reply = nullptr;

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
    response.body = reply->readAll().toStdString();
    response.ok = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();
    return response;
}

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
