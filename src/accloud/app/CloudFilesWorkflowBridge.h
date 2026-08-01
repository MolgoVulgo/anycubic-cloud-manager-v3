#pragma once

#include "app/usecases/cloud/DeleteCloudFilesUseCase.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace accloud {

class CloudFilesWorkflowBridge final : public QObject {
    Q_OBJECT

public:
    explicit CloudFilesWorkflowBridge(QObject* parent = nullptr);

    Q_INVOKABLE bool startBatchDelete(const QVariantList& files);
    Q_INVOKABLE void cancelBatchDelete();
    Q_INVOKABLE bool deleteSingleFile(const QString& fileId);

public slots:
    void handleDeleteFileFinished(const QString& fileId, const QVariantMap& result);

signals:
    void deleteFileRequested(const QString& fileId);
    void batchDeleteStarted(int total);
    void batchDeleteProgress(int completed,
                             int total,
                             int succeeded,
                             const QString& fileId,
                             bool success);
    void batchDeleteFileSucceeded(const QString& fileId);
    void batchDeleteFinished(const QVariantMap& summary);
    void singleDeleteFinished(const QString& fileId, const QVariantMap& result);

private:
    void requestCurrentBatchFile();
    [[nodiscard]] QVariantMap batchSummaryToVariant() const;

    usecases::cloud::DeleteCloudFilesUseCase m_batchDelete;
    QString m_singleDeleteFileId;
};

} // namespace accloud
