#pragma once

#include <QObject>
#include <QString>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace accloud {

class CloudDownloadController final : public QObject {
    Q_OBJECT
public:
    explicit CloudDownloadController(QObject* parent = nullptr);
    ~CloudDownloadController() override;

    void start(const QString& signedUrl, const QString& savePath);
    void cancel();
    void shutdown();

Q_SIGNALS:
    void progress(qint64 received, qint64 total);
    void finished(bool ok, const QString& message, const QString& savedPath);

private:
    void cleanup();

    QNetworkAccessManager* m_nam{nullptr};
    QNetworkReply* m_reply{nullptr};
    QFile* m_file{nullptr};
    QString m_path;
};

} // namespace accloud
