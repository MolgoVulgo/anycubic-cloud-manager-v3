#pragma once

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <future>
#include <functional>
#include <mutex>
#include <vector>

namespace accloud {

class LocalCacheStore;

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
    Q_INVOKABLE QVariantMap loadCachedQuota() const;
    Q_INVOKABLE void refreshFilesAsync(int page = 1, int limit = 20, bool force = false);

    // Retourne { ok, message }
    Q_INVOKABLE QVariantMap deleteFile(const QString& fileId) const;

    // Retourne { ok, message, url }  — ne pas afficher l'url dans l'UI
    Q_INVOKABLE QVariantMap getDownloadUrl(const QString& fileId) const;
    // Retourne { ok, message, fileId, gcodeId, uploadStatus }
    Q_INVOKABLE QVariantMap uploadLocalFile(const QString& localPath) const;
    // Upload asynchrone pour UI avec progression.
    Q_INVOKABLE void startUploadLocalFile(const QString& localPath);

    // Retourne { ok, message, printers:[...] }
    // Champs debug (uniquement si ACCLOUD_DEBUG=ON): endpoint, rawJson
    Q_INVOKABLE QVariantMap fetchPrinters() const;
    Q_INVOKABLE QVariantMap loadCachedPrinters() const;
    Q_INVOKABLE void refreshPrintersAsync(bool force = false);
    Q_INVOKABLE QVariantMap loadCachedFiles(int page = 1, int limit = 20) const;

    // Retourne { ok, message, printers:[{id,available,reason}] }
    Q_INVOKABLE QVariantMap fetchCompatiblePrintersByExt(const QString& fileExt) const;
    // Retourne { ok, message, printers:[{id,available,reason}] }
    Q_INVOKABLE QVariantMap fetchCompatiblePrintersByFileId(const QString& fileId) const;
    // Retourne { ok, score, reason } pour une validation locale file/printer sans appel réseau.
    Q_INVOKABLE QVariantMap evaluateLocalPrinterFileCompatibility(const QVariantMap& printer,
                                                                  const QVariantMap& file) const;
    // Retourne { ok, message, details:{...} }
    // Champ debug (uniquement si ACCLOUD_DEBUG=ON): rawJson
    Q_INVOKABLE QVariantMap fetchPrinterDetails(const QString& printerId) const;
    // Retourne { ok, message, reasons:[{reason,desc,helpUrl,type,push,popup}] }
    Q_INVOKABLE QVariantMap fetchReasonCatalog() const;
    Q_INVOKABLE void refreshReasonCatalogAsync(bool force = false);
    // Retourne { ok, message, projects:[{taskId,gcodeName,printerId,printerName,printStatus,progress,elapsedSec,remainingSec,currentLayer,totalLayers,currentFile,reason,createTime,endTime,img}] }
    // Champ debug (uniquement si ACCLOUD_DEBUG=ON): rawJson
    Q_INVOKABLE QVariantMap fetchPrinterProjects(const QString& printerId,
                                                 int page = 1,
                                                 int limit = 10) const;
    Q_INVOKABLE void refreshPrinterInsightsAsync(const QString& printerId,
                                                 int page = 1,
                                                 int limit = 20,
                                                 bool force = false);
    // Retourne { ok, message, projects:[...] } depuis la DB locale uniquement.
    Q_INVOKABLE QVariantMap loadCachedPrinterProjects(const QString& printerId,
                                                      int page = 1,
                                                      int limit = 20) const;

    // Retourne { ok, message, taskId, msgId, correlationTicket, correlationStatus }
    Q_INVOKABLE QVariantMap sendPrintOrder(const QString& printerId,
                                           const QString& fileId,
                                           bool deleteAfterPrint = false,
                                           bool dryRun = false) const;
    // Retourne { ok, message, taskId, msgId }
    Q_INVOKABLE QVariantMap sendPrinterOrder(const QString& printerId,
                                             int orderId,
                                             const QVariantMap& data = QVariantMap(),
                                             const QString& projectId = QString()) const;

    // ── Téléchargement asynchrone ─────────────────────────────────────────
    // signedUrl : URL obtenue via getDownloadUrl()
    // savePath  : chemin local de destination
    Q_INVOKABLE void startDownload(const QString& signedUrl, const QString& savePath);
    Q_INVOKABLE void cancelDownload();

Q_SIGNALS:
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(bool ok, const QString& message, const QString& savedPath);
    void uploadProgressChanged(double progress, const QString& phase);
    void uploadFinished(bool ok,
                        const QString& message,
                        const QString& fileId,
                        const QString& gcodeId,
                        int uploadStatus,
                        bool unlockOk);
    void filesUpdatedFromCache(const QVariantList& files, const QString& message);
    void filesUpdatedFromCloud(const QVariantList& files, const QString& message);
    void printersUpdatedFromCache(const QVariantList& printers, const QString& message);
    void printersUpdatedFromCloud(const QVariantList& printers, const QString& message);
    void reasonCatalogUpdatedFromCloud(const QVariantList& reasons, const QString& message);
    void printerInsightsUpdatedFromCloud(const QString& printerId,
                                         const QVariantMap& details,
                                         const QVariantList& projects,
                                         const QString& detailsRawJson,
                                         const QString& projectsRawJson,
                                         const QString& message);
    void quotaUpdatedFromCache(const QVariantMap& quota, const QString& message);
    void quotaUpdatedFromCloud(const QVariantMap& quota, const QString& message);
    void syncFailed(const QString& scope, const QString& message);

private:
    bool shouldRefresh(const QString& scope, int ttlSec, bool force) const;
    QVariantList fetchFilesWithRetry(int page, int limit, QString& message, bool& ok, bool downloadThumbnails) const;
    QVariantList fetchPrintersWithRetry(QString& message, bool& ok, QString& rawJson) const;
    QVariantMap fetchQuotaWithRetry(QString& message, bool& ok) const;
    void cleanupDownload();
    void launchBackgroundTask(std::function<void()> task);
    void reapFinishedBackgroundTasksLocked();
    void waitBackgroundTasks();

    QNetworkAccessManager* m_nam{nullptr};
    QNetworkReply*         m_dlReply{nullptr};
    QFile*                 m_dlFile{nullptr};
    QString                m_dlPath;
    LocalCacheStore*       m_cache{nullptr};
    mutable std::atomic_bool m_refreshFilesRunning{false};
    mutable std::atomic_bool m_refreshPrintersRunning{false};
    mutable std::atomic_bool m_refreshReasonCatalogRunning{false};
    std::atomic_bool m_shuttingDown{false};
    std::mutex m_backgroundTasksMutex;
    std::vector<std::future<void>> m_backgroundTasks;
};

} // namespace accloud
