#include "CloudFilesWorkflowBridge.h"

#include <QStringList>

#include <utility>
#include <vector>

namespace accloud {

CloudFilesWorkflowBridge::CloudFilesWorkflowBridge(QObject* parent)
    : QObject(parent) {}

bool CloudFilesWorkflowBridge::startBatchDelete(const QVariantList& files) {
    if (m_batchDelete.running() || !m_singleDeleteFileId.isEmpty()) {
        return false;
    }

    std::vector<usecases::cloud::CloudFileDeleteItem> items;
    items.reserve(static_cast<std::size_t>(files.size()));
    for (const QVariant& value : files) {
        const QVariantMap file = value.toMap();
        const QString fileId = file.value(QStringLiteral("fileId")).toString().trimmed();
        if (fileId.isEmpty()) {
            continue;
        }
        items.push_back({fileId.toStdString(),
                         file.value(QStringLiteral("fileName")).toString().toStdString()});
    }

    if (!m_batchDelete.start(std::move(items))) {
        return false;
    }

    emit batchDeleteStarted(static_cast<int>(m_batchDelete.total()));
    requestCurrentBatchFile();
    return true;
}

void CloudFilesWorkflowBridge::cancelBatchDelete() {
    m_batchDelete.cancel();
}

bool CloudFilesWorkflowBridge::deleteSingleFile(const QString& fileId) {
    const QString normalizedFileId = fileId.trimmed();
    if (normalizedFileId.isEmpty() || m_batchDelete.running() || !m_singleDeleteFileId.isEmpty()) {
        return false;
    }
    m_singleDeleteFileId = normalizedFileId;
    emit deleteFileRequested(normalizedFileId);
    return true;
}

void CloudFilesWorkflowBridge::handleDeleteFileFinished(const QString& fileId,
                                                        const QVariantMap& result) {
    const QString normalizedFileId = fileId.trimmed();
    const auto current = m_batchDelete.current();
    if (m_batchDelete.running() && current.has_value()
        && normalizedFileId.toStdString() == current->fileId) {
        const bool success = result.value(QStringLiteral("ok")).toBool();
        const QString message = result.value(QStringLiteral("message")).toString();
        if (!m_batchDelete.handleResult(current->fileId, success, message.toStdString())) {
            return;
        }

        if (success) {
            emit batchDeleteFileSucceeded(normalizedFileId);
        }
        emit batchDeleteProgress(static_cast<int>(m_batchDelete.completed()),
                                 static_cast<int>(m_batchDelete.total()),
                                 static_cast<int>(m_batchDelete.succeeded()),
                                 normalizedFileId,
                                 success);

        if (m_batchDelete.running()) {
            requestCurrentBatchFile();
        } else {
            emit batchDeleteFinished(batchSummaryToVariant());
        }
        return;
    }

    if (!m_singleDeleteFileId.isEmpty() && normalizedFileId == m_singleDeleteFileId) {
        m_singleDeleteFileId.clear();
        emit singleDeleteFinished(normalizedFileId, result);
    }
}

void CloudFilesWorkflowBridge::requestCurrentBatchFile() {
    const auto current = m_batchDelete.current();
    if (!current.has_value()) {
        return;
    }
    emit deleteFileRequested(QString::fromStdString(current->fileId));
}

QVariantMap CloudFilesWorkflowBridge::batchSummaryToVariant() const {
    const auto summary = m_batchDelete.summary();
    QVariantList failures;
    failures.reserve(static_cast<qsizetype>(summary.failures.size()));
    for (const auto& failure : summary.failures) {
        QVariantMap row;
        row.insert(QStringLiteral("fileId"), QString::fromStdString(failure.fileId));
        row.insert(QStringLiteral("message"), QString::fromStdString(failure.message));
        failures.push_back(row);
    }

    QVariantMap out;
    out.insert(QStringLiteral("requested"), static_cast<int>(summary.requested));
    out.insert(QStringLiteral("completed"), static_cast<int>(summary.completed));
    out.insert(QStringLiteral("succeeded"), static_cast<int>(summary.succeeded));
    out.insert(QStringLiteral("failed"), static_cast<int>(summary.failures.size()));
    out.insert(QStringLiteral("failures"), failures);
    out.insert(QStringLiteral("cancelled"), summary.cancelled);
    return out;
}

} // namespace accloud
