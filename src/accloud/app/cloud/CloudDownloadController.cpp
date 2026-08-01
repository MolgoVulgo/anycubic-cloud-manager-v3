#include "CloudDownloadController.h"

#include "infra/logging/JsonlLogger.h"
#include "infra/logging/RawTrafficLogger.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <string>

namespace accloud {
namespace {

logging::raw::HeaderList rawRequestHeaders(const QNetworkRequest& request) {
    logging::raw::HeaderList headers;
    const auto names = request.rawHeaderList();
    headers.reserve(static_cast<std::size_t>(names.size()));
    for (const QByteArray& name : names) {
        headers.emplace_back(name.toStdString(), request.rawHeader(name).toStdString());
    }
    return headers;
}

logging::raw::HeaderList rawResponseHeaders(const QNetworkReply& reply) {
    logging::raw::HeaderList headers;
    const auto pairs = reply.rawHeaderPairs();
    headers.reserve(static_cast<std::size_t>(pairs.size()));
    for (const auto& pair : pairs) {
        headers.emplace_back(pair.first.toStdString(), pair.second.toStdString());
    }
    return headers;
}

} // namespace

CloudDownloadController::CloudDownloadController(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this)) {}

CloudDownloadController::~CloudDownloadController() {
    shutdown();
}

void CloudDownloadController::start(const QString& signedUrl, const QString& savePath) {
    if (m_reply) {
        logging::warn("app", "cloud_download", "download_already_running",
                      "Un téléchargement est déjà en cours");
        return;
    }

    m_path = savePath;
    m_file = new QFile(savePath, this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logging::error("app", "cloud_download", "download_open_failed",
                       "Impossible d'ouvrir le fichier de destination",
                       {{"path", savePath.toStdString()}});
        emit finished(false, "Impossible d'ouvrir : " + savePath, {});
        delete m_file;
        m_file = nullptr;
        return;
    }

    logging::info("app", "cloud_download", "download_start",
                  "Démarrage téléchargement",
                  {{"dest", savePath.toStdString()}});

    // Signed URL download intentionally carries no Workbench Authorization/XX-* headers.
    QNetworkRequest request{QUrl(signedUrl)};
    request.setTransferTimeout(0);

    const std::string correlationId = logging::raw::nextCorrelationId("http");
    logging::raw::logHttpRequest(correlationId,
                                 "GET",
                                 signedUrl.toStdString(),
                                 rawRequestHeaders(request),
                                 {});
    m_reply = m_nam->get(request);

    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_file && m_reply) {
            m_file->write(m_reply->readAll());
        }
    });

    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &CloudDownloadController::progress);

    connect(m_reply, &QNetworkReply::finished, this, [this, correlationId]() {
        if (!m_reply) {
            return;
        }
        const bool netOk = (m_reply->error() == QNetworkReply::NoError);
        const QString error = m_reply->errorString();
        const QString path = m_path;
        const int httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const qint64 streamedBytes = m_file ? m_file->size() : 0;
        logging::raw::logHttpResponse(
            correlationId,
            httpStatus,
            m_reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().toStdString(),
            rawResponseHeaders(*m_reply),
            "<binary download streamed to disk: " + std::to_string(streamedBytes) + " bytes>",
            netOk ? std::string{} : error.toStdString());

        if (m_file) {
            m_file->flush();
            m_file->close();
        }

        if (netOk) {
            logging::info("app", "cloud_download", "download_finished_ok",
                          "Téléchargement terminé", {{"dest", path.toStdString()}});
            emit finished(true, "Téléchargement terminé", path);
        } else {
            logging::warn("app", "cloud_download", "download_failed",
                          "Échec téléchargement", {{"error", error.toStdString()}});
            if (m_file) {
                m_file->remove();
            }
            emit finished(false, "Erreur: " + error, {});
        }
        cleanup();
    });
}

void CloudDownloadController::cancel() {
    if (m_reply) {
        logging::info("app", "cloud_download", "download_cancelled",
                      "Annulation téléchargement");
        m_reply->abort();
    }
}

void CloudDownloadController::shutdown() {
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
    }
    cleanup();
}

void CloudDownloadController::cleanup() {
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        delete m_file;
        m_file = nullptr;
    }
    m_path.clear();
}

} // namespace accloud
