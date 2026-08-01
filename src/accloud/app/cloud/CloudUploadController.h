#pragma once

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

class CloudUploadController final : public QObject {
    Q_OBJECT
public:
    explicit CloudUploadController(LocalCacheStore* cache, QObject* parent = nullptr);
    ~CloudUploadController() override;

    QVariantMap inspectPwszPreview(const QString& localPath) const;
    QVariantMap uploadLocalFile(const QString& localPath,
                                bool completePwszPreview2) const;
    void startUploadLocalFile(const QString& localPath,
                              bool completePwszPreview2,
                              const QString& requestContext);
    void startPwszCloudPreviewUpdate(const QVariantList& files);
    void cancelPwszCloudPreviewUpdate();
    void setRefreshFilesCallback(std::function<void()> callback);
    void shutdown();

Q_SIGNALS:
    void uploadProgressChanged(double progress, const QString& phase);
    void uploadFinished(bool ok,
                        const QString& message,
                        const QString& fileId,
                        const QString& gcodeId,
                        int uploadStatus,
                        bool unlockOk,
                        bool localFileSynchronized);
    void uploadContextProgressChanged(const QString& requestContext,
                                      double progress,
                                      const QString& phase);
    void uploadContextFinished(const QString& requestContext,
                               const QVariantMap& result);
    void pwszCloudPreviewUpdateProgress(int current,
                                        int total,
                                        const QString& fileName,
                                        const QString& phase);
    void pwszCloudPreviewUpdateFinished(const QVariantMap& summary);

private:
    void invalidateFilesAndQuota();
    void launchBackgroundTask(std::function<void()> task);
    void reapFinishedBackgroundTasksLocked();
    void waitBackgroundTasks();

    LocalCacheStore* m_cache{nullptr};
    std::function<void()> m_refreshFilesCallback;
    std::atomic_bool m_pwszCloudUpdateRunning{false};
    std::atomic_bool m_pwszCloudUpdateCancelRequested{false};
    std::atomic_bool m_shuttingDown{false};
    std::mutex m_backgroundTasksMutex;
    std::vector<std::future<void>> m_backgroundTasks;
};

} // namespace accloud
