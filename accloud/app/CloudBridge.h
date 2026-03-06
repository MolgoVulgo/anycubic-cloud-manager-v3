#pragma once

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QVariantMap>

namespace accloud {

class CloudBridge : public QObject {
    Q_OBJECT

public:
    explicit CloudBridge(QObject* parent = nullptr);
    ~CloudBridge() override;

    // ── Opérations synchrones ──────────────────────────────────────────────
    // Retourne { ok, message, files: [QVariantMap], total }
    Q_INVOKABLE QVariantMap fetchFiles(int page = 1, int limit = 20) const;

    // Retourne { ok, message, totalDisplay, usedDisplay, totalBytes, usedBytes }
    Q_INVOKABLE QVariantMap fetchQuota() const;

    // Retourne { ok, message }
    Q_INVOKABLE QVariantMap deleteFile(const QString& fileId) const;

    // Retourne { ok, message, url }  — ne pas afficher l'url dans l'UI
    Q_INVOKABLE QVariantMap getDownloadUrl(const QString& fileId) const;

    // Retourne { ok, message, endpoint, rawJson, printers:[{id,name,model,type,lastSeen,state,reason,available,progress,elapsedSec,remainingSec,currentFile}] }
    Q_INVOKABLE QVariantMap fetchPrinters() const;

    // Retourne { ok, message, printers:[{id,available,reason}] }
    Q_INVOKABLE QVariantMap fetchCompatiblePrintersByExt(const QString& fileExt) const;
    // Retourne { ok, message, printers:[{id,available,reason}] }
    Q_INVOKABLE QVariantMap fetchCompatiblePrintersByFileId(const QString& fileId) const;
    // Retourne { ok, message, details:{...}, rawJson }
    Q_INVOKABLE QVariantMap fetchPrinterDetails(const QString& printerId) const;
    // Retourne { ok, message, reasons:[{reason,desc,helpUrl,type,push,popup}] }
    Q_INVOKABLE QVariantMap fetchReasonCatalog() const;
    // Retourne { ok, message, projects:[{taskId,gcodeName,printerId,printerName,printStatus,progress,reason,createTime,endTime,img}] }
    Q_INVOKABLE QVariantMap fetchPrinterProjects(const QString& printerId,
                                                 int page = 1,
                                                 int limit = 10) const;

    // Retourne { ok, message, taskId }
    Q_INVOKABLE QVariantMap sendPrintOrder(const QString& printerId,
                                           const QString& fileId,
                                           bool deleteAfterPrint = false,
                                           bool dryRun = false) const;

    // ── Téléchargement asynchrone ─────────────────────────────────────────
    // signedUrl : URL obtenue via getDownloadUrl()
    // savePath  : chemin local de destination
    Q_INVOKABLE void startDownload(const QString& signedUrl, const QString& savePath);
    Q_INVOKABLE void cancelDownload();

Q_SIGNALS:
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(bool ok, const QString& message, const QString& savedPath);

private:
    bool loadTokens(std::string& accessToken, std::string& xxToken) const;
    void cleanupDownload();

    QNetworkAccessManager* m_nam{nullptr};
    QNetworkReply*         m_dlReply{nullptr};
    QFile*                 m_dlFile{nullptr};
    QString                m_dlPath;
};

} // namespace accloud
