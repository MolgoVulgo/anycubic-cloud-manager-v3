#include "PrintWorkflowBridge.h"

#include "app/printing/PrinterFileCompatibility.h"

#include <QString>

#include <utility>
#include <vector>

namespace accloud {
namespace {

using usecases::printing::DirectPrintLifecycleEffectKind;

int toUiCompletion(usecases::printing::DirectPrintCompletionKind completionKind) {
  switch (completionKind) {
  case usecases::printing::DirectPrintCompletionKind::Success:
    return PrintWorkflowBridge::SuccessCompletion;
  case usecases::printing::DirectPrintCompletionKind::Failure:
    return PrintWorkflowBridge::FailureCompletion;
  case usecases::printing::DirectPrintCompletionKind::None:
    return PrintWorkflowBridge::NoCompletion;
  }
  return PrintWorkflowBridge::NoCompletion;
}

QString cloudFileIdFrom(const QVariantMap& fileData) {
  QString value = fileData.value(QStringLiteral("fileId")).toString().trimmed();
  if (value.isEmpty()) {
    value = fileData.value(QStringLiteral("id")).toString().trimmed();
  }
  if (value.isEmpty()) {
    value = fileData.value(QStringLiteral("gcodeId")).toString().trimmed();
  }
  return value;
}

std::vector<usecases::printing::DirectPrintProjectSnapshot> directProjectSnapshots(
    const QVariantList& projects) {
  std::vector<usecases::printing::DirectPrintProjectSnapshot> snapshots;
  snapshots.reserve(static_cast<std::size_t>(projects.size()));
  for (const QVariant& value : projects) {
    const QVariantMap project = value.toMap();
    usecases::printing::DirectPrintProjectSnapshot snapshot;
    snapshot.printerId = project.value(QStringLiteral("printerId")).toString().trimmed().toStdString();
    snapshot.taskId = project.value(QStringLiteral("taskId")).toString().trimmed().toStdString();
    snapshot.cloudFileId = project.value(QStringLiteral("cloudFileId")).toString().trimmed().toStdString();
    snapshot.currentFile = project.value(QStringLiteral("currentFile")).toString().trimmed().toStdString();
    snapshot.gcodeName = project.value(QStringLiteral("gcodeName")).toString().trimmed().toStdString();
    snapshot.printStatus = project.value(QStringLiteral("printStatus")).toInt();
    snapshots.push_back(std::move(snapshot));
  }
  return snapshots;
}

} // namespace

PrintWorkflowBridge::PrintWorkflowBridge(QObject* parent)
    : QObject(parent)
    , m_pendingStore(m_cacheStore) {}

QVariantList PrintWorkflowBridge::pendingDirectPrints() const {
  QVariantList rows;
  const auto operations = m_pendingStore.loadAll();
  rows.reserve(static_cast<qsizetype>(operations.size()));
  for (const auto& operation : operations) {
    rows.push_back(PendingDirectPrintStore::toVariantMap(operation));
  }
  return rows;
}

bool PrintWorkflowBridge::trackDirectPrint(const QVariantMap& operation) {
  return m_pendingStore.save(PendingDirectPrintStore::fromVariantMap(operation));
}

bool PrintWorkflowBridge::completeDirectPrint(const QString& printerId) {
  const QString key = printerId.trimmed();
  m_directLocalDeleteInFlight.remove(key);
  m_pendingLocalDeleteDispatchByPrinterId.remove(key);
  for (auto it = m_pendingLocalDeleteByMsgId.begin(); it != m_pendingLocalDeleteByMsgId.end();) {
    if (it.value().operation.value(QStringLiteral("printerId")).toString().trimmed() == key) {
      it = m_pendingLocalDeleteByMsgId.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_directCloudDeleteByFileId.begin(); it != m_directCloudDeleteByFileId.end();) {
    if (it.value().value(QStringLiteral("printerId")).toString().trimmed() == key) {
      it = m_directCloudDeleteByFileId.erase(it);
    } else {
      ++it;
    }
  }
  const bool removed = m_pendingStore.remove(key.toStdString());
  if (removed) {
    emit directPrintTrackingReleased(key);
  }
  return removed;
}

void PrintWorkflowBridge::reconcileDirectPrints(const QVariantList& projects) {
  const auto snapshots = directProjectSnapshots(projects);
  const auto operations = m_pendingStore.loadAll();
  for (const auto& operation : operations) {
    const auto match = usecases::printing::findMatchingDirectPrintProject(operation, snapshots);
    const int terminalStatus = match.has_value() ? snapshots[*match].printStatus : 0;
    const QString printerId = QString::fromStdString(operation.printerId).trimmed();
    const QString cloudFileId = QString::fromStdString(operation.cloudFileId).trimmed();
    usecases::printing::DirectPrintLifecycleContext context;
    context.localDeleteInFlight = m_directLocalDeleteInFlight.contains(printerId);
    context.cloudDeleteInFlight = m_directCloudDeleteByFileId.contains(cloudFileId);
    processDirectTransition(
        usecases::printing::reconcileDirectPrintOperation(operation, snapshots, context),
        terminalStatus);
  }
}

usecases::printing::DirectPrintCompletionKind PrintWorkflowBridge::toDomainCompletion(int completionKind) {
  if (completionKind == SuccessCompletion) {
    return usecases::printing::DirectPrintCompletionKind::Success;
  }
  if (completionKind == FailureCompletion) {
    return usecases::printing::DirectPrintCompletionKind::Failure;
  }
  return usecases::printing::DirectPrintCompletionKind::None;
}

QVariantMap PrintWorkflowBridge::beginDirectLocalDelete(const QVariantMap& operation,
                                                        int completionKind) {
  const QString printerId = operation.value(QStringLiteral("printerId")).toString().trimmed();
  QVariantMap result = applyTransition(usecases::printing::beginDirectPrintLocalDelete(
      PendingDirectPrintStore::fromVariantMap(operation),
      toDomainCompletion(completionKind),
      m_directLocalDeleteInFlight.contains(printerId)));
  if (result.value(QStringLiteral("requestLocalDelete")).toBool() && !printerId.isEmpty()) {
    m_directLocalDeleteInFlight.insert(printerId);
  }
  return result;
}

QVariantMap PrintWorkflowBridge::handleDirectLocalDeleteDispatch(const QVariantMap& operation,
                                                                 int completionKind,
                                                                 bool accepted,
                                                                 const QString& confirmationMsgId) {
  const QString printerId = operation.value(QStringLiteral("printerId")).toString().trimmed();
  const QString normalizedMsgId = confirmationMsgId.trimmed();
  const bool confirmationPending = accepted && !normalizedMsgId.isEmpty();
  QVariantMap result = applyTransition(usecases::printing::handleDirectPrintLocalDeleteDispatch(
      PendingDirectPrintStore::fromVariantMap(operation),
      toDomainCompletion(completionKind),
      {accepted, confirmationPending}));

  if (confirmationPending) {
    PendingLocalDeleteConfirmation pending;
    pending.operation = result.value(QStringLiteral("operation")).toMap();
    pending.completionKind = completionKind;
    m_pendingLocalDeleteByMsgId.insert(normalizedMsgId, pending);
  } else if (!printerId.isEmpty()) {
    m_directLocalDeleteInFlight.remove(printerId);
  }
  return result;
}

QVariantMap PrintWorkflowBridge::beginDirectCloudDelete(const QVariantMap& operation,
                                                        bool cloudDeleteInFlight) {
  return applyTransition(usecases::printing::beginDirectPrintCloudDelete(
      PendingDirectPrintStore::fromVariantMap(operation), cloudDeleteInFlight));
}

QVariantMap PrintWorkflowBridge::handleDirectCloudDeleteResult(const QVariantMap& operation,
                                                               bool success) {
  return applyTransition(usecases::printing::handleDirectPrintCloudDeleteResult(
      PendingDirectPrintStore::fromVariantMap(operation), success));
}

bool PrintWorkflowBridge::trackRemotePrintCleanup(const QString& printerId,
                                                  const QVariantMap& fileData) {
  const QString key = printerId.trimmed();
  if (key.isEmpty() || fileData.value(QStringLiteral("directMode")).toBool()) {
    return false;
  }
  RemoteCleanupOperation operation;
  operation.printerId = key;
  operation.cloudFileId = cloudFileIdFrom(fileData);
  operation.fileName = fileData.value(QStringLiteral("fileName")).toString().trimmed();
  operation.deleteAfterPrint = fileData.value(QStringLiteral("deleteAfterPrint")).toBool();
  m_remoteCleanupByPrinterId.insert(key, operation);
  return true;
}

void PrintWorkflowBridge::beginRemotePostPrintCleanup(const QString& printerId) {
  const QString key = printerId.trimmed();
  auto it = m_remoteCleanupByPrinterId.find(key);
  if (key.isEmpty() || it == m_remoteCleanupByPrinterId.end() || m_remoteCleanupInFlight.contains(key)) {
    return;
  }
  const RemoteCleanupOperation operation = it.value();
  if (!operation.deleteAfterPrint) {
    m_remoteCleanupByPrinterId.erase(it);
    emit remotePrintTrackingReleased(key);
    return;
  }
  if (operation.fileName.isEmpty()) {
    m_remoteCleanupByPrinterId.erase(it);
    emit remotePrintTrackingReleased(key);
    emit remoteCleanupNotice(RemoteLocalDeleteFailed, key);
    return;
  }

  m_remoteCleanupInFlight.insert(key);
  QVariantMap request;
  request.insert(QStringLiteral("printerId"), key);
  request.insert(QStringLiteral("cloudFileId"), operation.cloudFileId);
  request.insert(QStringLiteral("fileName"), operation.fileName);
  emit remoteLocalDeleteRequested(request);
}

QString PrintWorkflowBridge::beginRemotePrintPreparation(const QString& mode,
                                                         const QString& fileId,
                                                         const QString& fileName,
                                                         const QVariantMap& fileData,
                                                         const QVariantList& printers,
                                                         const QString& preferredPrinterId) {
  ++m_remotePreparationSequence;
  m_remotePreparation = {};
  m_remotePreparation.requestId = QStringLiteral("remote-print-compat-%1").arg(m_remotePreparationSequence);
  m_remotePreparation.mode = mode.trimmed().toLower();
  m_remotePreparation.fileId = fileId.trimmed();
  m_remotePreparation.fileName = fileName.trimmed();
  m_remotePreparation.fileData = fileData;
  m_remotePreparation.printers = printers;
  m_remotePreparation.preferredPrinterId = preferredPrinterId.trimmed();

  const QString ext = printing::cloudSliceFileExtension(m_remotePreparation.fileName);
  if (m_remotePreparation.mode != QStringLiteral("direct") && !m_remotePreparation.fileId.isEmpty()) {
    m_remotePreparation.compatibilityChecked = true;
    emit remoteCompatibilityByFileIdRequested(m_remotePreparation.requestId, m_remotePreparation.fileId);
  } else if (printing::isKnownCloudSliceExtension(ext)) {
    m_remotePreparation.compatibilityChecked = true;
    emit remoteCompatibilityByExtRequested(m_remotePreparation.requestId, ext);
  } else {
    finalizeRemotePreparation();
  }
  return m_remotePreparation.requestId;
}

void PrintWorkflowBridge::cancelRemotePrintPreparation() {
  m_remotePreparation = {};
}

QVariantMap PrintWorkflowBridge::evaluateRemotePrintGuard(const QString& mode,
                                                          const QVariantMap& printer,
                                                          const QVariantMap& fileData) const {
  return m_prepareRemotePrintUseCase.evaluateGuard(mode, printer, fileData);
}

void PrintWorkflowBridge::handlePrinterFileAction(const QString& printerId,
                                                  const QString& action,
                                                  const QString& state,
                                                  int code,
                                                  const QString& msgId,
                                                  const QString& message) {
  Q_UNUSED(printerId);
  Q_UNUSED(message);
  if (action.compare(QStringLiteral("deleteLocal"), Qt::CaseInsensitive) != 0) {
    return;
  }

  const QString key = msgId.trimmed();
  auto pendingIt = m_pendingLocalDeleteByMsgId.find(key);
  if (key.isEmpty() || pendingIt == m_pendingLocalDeleteByMsgId.end()) {
    return;
  }

  const PendingLocalDeleteConfirmation pending = pendingIt.value();
  m_pendingLocalDeleteByMsgId.erase(pendingIt);
  const QString operationPrinterId =
      pending.operation.value(QStringLiteral("printerId")).toString().trimmed();
  if (!operationPrinterId.isEmpty()) {
    m_directLocalDeleteInFlight.remove(operationPrinterId);
  }

  const bool success = state.compare(QStringLiteral("success"), Qt::CaseInsensitive) == 0
      && code == 200;
  QVariantMap result = applyTransition(usecases::printing::handleDirectPrintLocalDeleteConfirmation(
      PendingDirectPrintStore::fromVariantMap(pending.operation),
      toDomainCompletion(pending.completionKind),
      success,
      false));

  if (!success) {
    emit directCleanupNotice(LocalDeleteConfirmationFailed);
    return;
  }
  const QVariantMap operation = result.value(QStringLiteral("operation")).toMap();
  if (result.value(QStringLiteral("requestCloudDelete")).toBool()) {
    dispatchDirectCloudDelete(operation);
    return;
  }
  if (result.value(QStringLiteral("removed")).toBool()) {
    emit directPrintTrackingReleased(operation.value(QStringLiteral("printerId")).toString().trimmed());
    if (pending.completionKind == FailureCompletion) {
      emit directCleanupNotice(FailureLocalDeleteCompleted);
    }
  }
}

void PrintWorkflowBridge::handlePrinterOrderFinished(const QVariantMap& context,
                                                     const QString& printerId,
                                                     int orderId,
                                                     const QVariantMap& result) {
  Q_UNUSED(orderId);
  const QString kind = context.value(QStringLiteral("kind")).toString();
  const QString key = printerId.trimmed();
  if (kind == QStringLiteral("directCleanup")) {
    auto pendingIt = m_pendingLocalDeleteDispatchByPrinterId.find(key);
    if (pendingIt == m_pendingLocalDeleteDispatchByPrinterId.end()) {
      m_directLocalDeleteInFlight.remove(key);
      return;
    }
    const PendingLocalDeleteConfirmation pending = pendingIt.value();
    m_pendingLocalDeleteDispatchByPrinterId.erase(pendingIt);
    const QString msgId = result.value(QStringLiteral("msgId")).toString().trimmed();
    const bool accepted = result.value(QStringLiteral("ok")).toBool();
    QVariantMap dispatch = handleDirectLocalDeleteDispatch(
        pending.operation, pending.completionKind, accepted, msgId);
    const QVariantMap operation = dispatch.value(QStringLiteral("operation")).toMap();
    if (!accepted) {
      emit directCleanupNotice(LocalDeleteDispatchFailed);
      return;
    }
    if (!msgId.isEmpty()) {
      return;
    }
    if (dispatch.value(QStringLiteral("requestCloudDelete")).toBool()) {
      dispatchDirectCloudDelete(operation);
      return;
    }
    if (dispatch.value(QStringLiteral("removed")).toBool()) {
      emit directPrintTrackingReleased(key);
      if (pending.completionKind == FailureCompletion) {
        emit directCleanupNotice(FailureLocalDeleteCompleted);
      }
    }
    return;
  }
  if (kind != QStringLiteral("remotePostPrintCleanup")) {
    return;
  }
  auto it = m_remoteCleanupByPrinterId.find(key);
  if (it == m_remoteCleanupByPrinterId.end()) {
    m_remoteCleanupInFlight.remove(key);
    return;
  }
  const RemoteCleanupOperation operation = it.value();
  if (!result.value(QStringLiteral("ok")).toBool()) {
    m_remoteCleanupInFlight.remove(key);
    m_remoteCleanupByPrinterId.erase(it);
    emit remotePrintTrackingReleased(key);
    emit remoteCleanupNotice(RemoteLocalDeleteFailed, key);
    return;
  }
  if (operation.cloudFileId.isEmpty()) {
    m_remoteCleanupInFlight.remove(key);
    m_remoteCleanupByPrinterId.erase(it);
    emit remotePrintTrackingReleased(key);
    emit remoteCleanupNotice(RemoteCloudFileIdMissing, key);
    return;
  }
  m_remoteCloudDeleteByFileId.insert(operation.cloudFileId, key);
  emit remoteCloudDeleteRequested(operation.cloudFileId);
}

void PrintWorkflowBridge::handleCloudDeleteFinished(const QString& fileId, const QVariantMap& result) {
  const QString normalizedFileId = fileId.trimmed();
  auto directIt = m_directCloudDeleteByFileId.find(normalizedFileId);
  if (directIt != m_directCloudDeleteByFileId.end()) {
    const QVariantMap operation = directIt.value();
    m_directCloudDeleteByFileId.erase(directIt);
    const bool success = result.value(QStringLiteral("ok")).toBool();
    QVariantMap transition = handleDirectCloudDeleteResult(operation, success);
    if (transition.value(QStringLiteral("removed")).toBool()) {
      emit directPrintTrackingReleased(operation.value(QStringLiteral("printerId")).toString().trimmed());
      emit directCleanupNotice(DirectCleanupCompleted);
    } else if (!success) {
      emit directCleanupNotice(DirectCloudDeleteFailed);
    }
    return;
  }

  auto cloudIt = m_remoteCloudDeleteByFileId.find(normalizedFileId);
  if (cloudIt == m_remoteCloudDeleteByFileId.end()) {
    return;
  }
  const QString printerId = cloudIt.value();
  m_remoteCloudDeleteByFileId.erase(cloudIt);
  m_remoteCleanupInFlight.remove(printerId);
  m_remoteCleanupByPrinterId.remove(printerId);
  emit remotePrintTrackingReleased(printerId);
  emit remoteCleanupNotice(result.value(QStringLiteral("ok")).toBool()
                               ? RemoteCleanupCompleted
                               : RemoteCloudDeleteFailed,
                           printerId);
}

void PrintWorkflowBridge::handleCompatiblePrintersByFileIdReady(const QString& requestId,
                                                                const QString& fileId,
                                                                const QVariantMap& result) {
  if (requestId.trimmed().isEmpty() || requestId != m_remotePreparation.requestId
      || fileId.trimmed() != m_remotePreparation.fileId) {
    return;
  }
  if (result.value(QStringLiteral("ok")).toBool()) {
    finalizeRemotePreparation(result);
    return;
  }
  m_remotePreparation.compatibilityFailed = true;
  const QString ext = printing::cloudSliceFileExtension(m_remotePreparation.fileName);
  if (printing::isKnownCloudSliceExtension(ext)) {
    emit remoteCompatibilityByExtRequested(m_remotePreparation.requestId, ext);
    return;
  }
  finalizeRemotePreparation();
}

void PrintWorkflowBridge::handleCompatiblePrintersByExtReady(const QString& requestId,
                                                             const QString& fileExt,
                                                             const QVariantMap& result) {
  if (requestId.trimmed().isEmpty() || requestId != m_remotePreparation.requestId) {
    return;
  }
  const QString expectedExt = printing::cloudSliceFileExtension(m_remotePreparation.fileName);
  if (fileExt.trimmed().toLower() != expectedExt) {
    return;
  }
  if (!result.value(QStringLiteral("ok")).toBool()) {
    m_remotePreparation.compatibilityFailed = true;
    finalizeRemotePreparation();
    return;
  }
  finalizeRemotePreparation(result);
}

void PrintWorkflowBridge::finalizeRemotePreparation(const QVariantMap& serverCompatibility) {
  if (m_remotePreparation.requestId.isEmpty()) {
    return;
  }
  QVariantMap result = m_prepareRemotePrintUseCase.execute(
      m_remotePreparation.mode,
      m_remotePreparation.fileId,
      m_remotePreparation.fileName,
      m_remotePreparation.fileData,
      m_remotePreparation.printers,
      m_remotePreparation.preferredPrinterId,
      serverCompatibility,
      m_remotePreparation.compatibilityChecked,
      m_remotePreparation.compatibilityFailed);
  result.insert(QStringLiteral("requestId"), m_remotePreparation.requestId);
  m_remotePreparation = {};
  emit remotePrintPreparationReady(result);
}

void PrintWorkflowBridge::processDirectTransition(
    usecases::printing::DirectPrintLifecycleTransition transition,
    int terminalPrintStatus) {
  QVariantMap result = applyTransition(std::move(transition));
  const QVariantMap operation = result.value(QStringLiteral("operation")).toMap();
  const QString printerId = operation.value(QStringLiteral("printerId")).toString().trimmed();

  if (result.value(QStringLiteral("removed")).toBool()) {
    emit directPrintTrackingReleased(printerId);
    if (terminalPrintStatus == 3 || terminalPrintStatus == 4) {
      emit directCleanupNotice(FailureFilesKept);
    }
  }
  if (result.value(QStringLiteral("requestLocalDelete")).toBool()) {
    dispatchDirectLocalDelete(operation, result.value(QStringLiteral("completionKind")).toInt());
  }
  if (result.value(QStringLiteral("requestCloudDelete")).toBool()) {
    dispatchDirectCloudDelete(operation);
  }
}

void PrintWorkflowBridge::dispatchDirectLocalDelete(const QVariantMap& operation,
                                                     int completionKind) {
  const QString printerId = operation.value(QStringLiteral("printerId")).toString().trimmed();
  const QString fileName = operation.value(QStringLiteral("printerLocalFilename")).toString().trimmed();
  QString filePath = operation.value(QStringLiteral("printerLocalPath")).toString().trimmed();
  if (filePath.isEmpty()) {
    filePath = QStringLiteral("/");
  }
  if (printerId.isEmpty() || fileName.isEmpty() || completionKind == NoCompletion) {
    const QVariantMap failed = handleDirectLocalDeleteDispatch(
        operation, completionKind, false, QString{});
    Q_UNUSED(failed);
    emit directCleanupNotice(LocalDeleteDispatchFailed);
    return;
  }
  if (m_pendingLocalDeleteDispatchByPrinterId.contains(printerId)) {
    return;
  }

  m_directLocalDeleteInFlight.insert(printerId);
  m_pendingLocalDeleteDispatchByPrinterId.insert(
      printerId, PendingLocalDeleteConfirmation{operation, completionKind});
  QVariantMap request;
  request.insert(QStringLiteral("printerId"), printerId);
  request.insert(QStringLiteral("fileName"), fileName);
  request.insert(QStringLiteral("path"), filePath);
  request.insert(QStringLiteral("completionKind"), completionKind);
  emit directLocalDeleteRequested(request);
}

void PrintWorkflowBridge::dispatchDirectCloudDelete(const QVariantMap& operation) {
  const QString fileId = operation.value(QStringLiteral("cloudFileId")).toString().trimmed();
  if (fileId.isEmpty() || m_directCloudDeleteByFileId.contains(fileId)) {
    return;
  }
  m_directCloudDeleteByFileId.insert(fileId, operation);
  emit directCloudDeleteRequested(operation);
}

QVariantMap PrintWorkflowBridge::applyTransition(
    usecases::printing::DirectPrintLifecycleTransition transition) {
  QVariantMap result;
  bool persisted = false;
  bool removed = false;
  bool requestLocalDelete = false;
  bool requestCloudDelete = false;
  int completionKind = NoCompletion;

  for (const auto& effect : transition.effects) {
    switch (effect.kind) {
    case DirectPrintLifecycleEffectKind::PersistOperation:
      persisted = m_pendingStore.save(transition.operation) || persisted;
      break;
    case DirectPrintLifecycleEffectKind::RemoveOperation:
      removed = m_pendingStore.remove(transition.operation.printerId) || removed;
      break;
    case DirectPrintLifecycleEffectKind::RequestLocalDelete:
      requestLocalDelete = true;
      completionKind = toUiCompletion(effect.completionKind);
      break;
    case DirectPrintLifecycleEffectKind::RequestCloudDelete:
      requestCloudDelete = true;
      break;
    }
  }

  result.insert(QStringLiteral("operation"), PendingDirectPrintStore::toVariantMap(transition.operation));
  result.insert(QStringLiteral("persisted"), persisted);
  result.insert(QStringLiteral("removed"), removed);
  result.insert(QStringLiteral("requestLocalDelete"), requestLocalDelete);
  result.insert(QStringLiteral("requestCloudDelete"), requestCloudDelete);
  result.insert(QStringLiteral("completionKind"), completionKind);
  result.insert(QStringLiteral("matchedProject"), transition.matchedProject);
  result.insert(QStringLiteral("cleanupError"),
                printing::isDirectPrintCleanupError(transition.operation.state));
  return result;
}

} // namespace accloud
