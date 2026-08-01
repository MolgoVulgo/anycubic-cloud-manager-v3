#pragma once

#include "app/LocalCacheStore.h"
#include "app/printing/PendingDirectPrintStore.h"
#include "app/usecases/printing/DirectPrintLifecycleUseCase.h"
#include "app/usecases/printing/PrepareRemotePrintUseCase.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

namespace accloud {

class PrintWorkflowBridge final : public QObject {
  Q_OBJECT

 public:
  enum CompletionKind {
    NoCompletion = 0,
    SuccessCompletion = 1,
    FailureCompletion = 2,
  };
  Q_ENUM(CompletionKind)

  enum CleanupNoticeKind {
    NoCleanupNotice = 0,
    LocalDeleteConfirmationFailed = 1,
    FailureLocalDeleteCompleted = 2,
    LocalDeleteDispatchFailed = 3,
    FailureFilesKept = 4,
    DirectCleanupCompleted = 5,
    DirectCloudDeleteFailed = 6,
  };
  Q_ENUM(CleanupNoticeKind)

  enum RemoteCleanupNoticeKind {
    NoRemoteCleanupNotice = 0,
    RemoteLocalDeleteFailed = 1,
    RemoteCloudFileIdMissing = 2,
    RemoteCloudDeleteFailed = 3,
    RemoteCleanupCompleted = 4,
  };
  Q_ENUM(RemoteCleanupNoticeKind)

  explicit PrintWorkflowBridge(QObject* parent = nullptr);

  QVariantList pendingDirectPrints() const;
  Q_INVOKABLE bool trackDirectPrint(const QVariantMap& operation);
  bool completeDirectPrint(const QString& printerId);
  Q_INVOKABLE void reconcileDirectPrints(const QVariantList& projects);

  QVariantMap beginDirectLocalDelete(const QVariantMap& operation,
                                     int completionKind);
  QVariantMap handleDirectLocalDeleteDispatch(const QVariantMap& operation,
                                              int completionKind,
                                              bool accepted,
                                              const QString& confirmationMsgId);
  QVariantMap beginDirectCloudDelete(const QVariantMap& operation,
                                     bool cloudDeleteInFlight = false);
  QVariantMap handleDirectCloudDeleteResult(const QVariantMap& operation,
                                            bool success);

  Q_INVOKABLE bool trackRemotePrintCleanup(const QString& printerId,
                                           const QVariantMap& fileData);
  Q_INVOKABLE void beginRemotePostPrintCleanup(const QString& printerId);

  Q_INVOKABLE QString beginRemotePrintPreparation(const QString& mode,
                                                  const QString& fileId,
                                                  const QString& fileName,
                                                  const QVariantMap& fileData,
                                                  const QVariantList& printers,
                                                  const QString& preferredPrinterId);
  Q_INVOKABLE void cancelRemotePrintPreparation();
  Q_INVOKABLE QVariantMap evaluateRemotePrintGuard(const QString& mode,
                                                   const QVariantMap& printer,
                                                   const QVariantMap& fileData) const;

 public slots:
  void handlePrinterFileAction(const QString& printerId,
                               const QString& action,
                               const QString& state,
                               int code,
                               const QString& msgId,
                               const QString& message);
  void handlePrinterOrderFinished(const QVariantMap& context,
                                  const QString& printerId,
                                  int orderId,
                                  const QVariantMap& result);
  void handleCloudDeleteFinished(const QString& fileId, const QVariantMap& result);
  void handleCompatiblePrintersByFileIdReady(const QString& requestId,
                                             const QString& fileId,
                                             const QVariantMap& result);
  void handleCompatiblePrintersByExtReady(const QString& requestId,
                                          const QString& fileExt,
                                          const QVariantMap& result);

 signals:
  void directLocalDeleteRequested(const QVariantMap& request);
  void directCloudDeleteRequested(const QVariantMap& operation);
  void directPrintTrackingReleased(const QString& printerId);
  void directCleanupNotice(int noticeKind);

  void remoteLocalDeleteRequested(const QVariantMap& request);
  void remoteCloudDeleteRequested(const QString& fileId);
  void remotePrintTrackingReleased(const QString& printerId);
  void remoteCleanupNotice(int noticeKind, const QString& printerId);

  void remoteCompatibilityByFileIdRequested(const QString& requestId, const QString& fileId);
  void remoteCompatibilityByExtRequested(const QString& requestId, const QString& fileExt);
  void remotePrintPreparationReady(const QVariantMap& result);

 private:
  struct PendingLocalDeleteConfirmation {
    QVariantMap operation;
    int completionKind{NoCompletion};
  };

  struct RemoteCleanupOperation {
    QString printerId;
    QString cloudFileId;
    QString fileName;
    bool deleteAfterPrint{false};
  };

  struct PendingRemotePreparation {
    QString requestId;
    QString mode;
    QString fileId;
    QString fileName;
    QVariantMap fileData;
    QVariantList printers;
    QString preferredPrinterId;
    bool compatibilityChecked{false};
    bool compatibilityFailed{false};
  };

  [[nodiscard]] static usecases::printing::DirectPrintCompletionKind toDomainCompletion(int completionKind);
  [[nodiscard]] QVariantMap applyTransition(usecases::printing::DirectPrintLifecycleTransition transition);
  void processDirectTransition(usecases::printing::DirectPrintLifecycleTransition transition,
                               int terminalPrintStatus = 0);
  void dispatchDirectLocalDelete(const QVariantMap& operation, int completionKind);
  void dispatchDirectCloudDelete(const QVariantMap& operation);
  void finalizeRemotePreparation(const QVariantMap& serverCompatibility = {});

  LocalCacheStore m_cacheStore;
  PendingDirectPrintStore m_pendingStore;
  QSet<QString> m_directLocalDeleteInFlight;
  QHash<QString, PendingLocalDeleteConfirmation> m_pendingLocalDeleteDispatchByPrinterId;
  QHash<QString, PendingLocalDeleteConfirmation> m_pendingLocalDeleteByMsgId;
  QHash<QString, QVariantMap> m_directCloudDeleteByFileId;

  QHash<QString, RemoteCleanupOperation> m_remoteCleanupByPrinterId;
  QHash<QString, QString> m_remoteCloudDeleteByFileId;
  QSet<QString> m_remoteCleanupInFlight;

  quint64 m_remotePreparationSequence{0};
  PendingRemotePreparation m_remotePreparation;
  usecases::printing::PrepareRemotePrintUseCase m_prepareRemotePrintUseCase;
};

} // namespace accloud
