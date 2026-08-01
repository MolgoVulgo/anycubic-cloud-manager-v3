#include "PendingDirectPrintStore.h"

#include "app/LocalCacheStore.h"

#include <QString>

namespace accloud {
namespace {

std::string textValue(const QVariantMap& map, const char* key) {
  return map.value(QString::fromLatin1(key)).toString().trimmed().toStdString();
}

} // namespace

PendingDirectPrintStore::PendingDirectPrintStore(LocalCacheStore& cacheStore)
    : m_cacheStore(cacheStore) {}

std::vector<printing::DirectPrintOperation> PendingDirectPrintStore::loadAll() const {
  const QVariantList rows = m_cacheStore.loadPendingDirectPrints();
  std::vector<printing::DirectPrintOperation> operations;
  operations.reserve(static_cast<std::size_t>(rows.size()));
  for (const QVariant& row : rows) {
    const QVariantMap map = row.toMap();
    auto operation = fromVariantMap(map);
    if (!operation.printerId.empty()) {
      operations.push_back(std::move(operation));
    }
  }
  return operations;
}

bool PendingDirectPrintStore::save(const printing::DirectPrintOperation& operation) const {
  if (operation.printerId.empty()) {
    return false;
  }
  return m_cacheStore.savePendingDirectPrint(toVariantMap(operation));
}

bool PendingDirectPrintStore::remove(const std::string& printerId) const {
  if (printerId.empty()) {
    return false;
  }
  return m_cacheStore.removePendingDirectPrint(QString::fromStdString(printerId));
}

printing::DirectPrintOperation PendingDirectPrintStore::fromVariantMap(const QVariantMap& map) {
  printing::DirectPrintOperation operation;
  operation.printerId = textValue(map, "printerId");
  operation.cloudFileId = textValue(map, "cloudFileId");
  operation.cloudGcodeId = textValue(map, "cloudGcodeId");
  operation.cloudFileName = textValue(map, "cloudFileName");
  operation.cloudFileSize = map.value(QStringLiteral("cloudFileSize"), 0).toULongLong();
  operation.printTaskId = textValue(map, "printTaskId");
  operation.printMsgId = textValue(map, "printMsgId");
  operation.printerLocalFilename = textValue(map, "printerLocalFilename");
  operation.printerLocalPath = textValue(map, "printerLocalPath");
  if (operation.printerLocalPath.empty()) {
    operation.printerLocalPath = "/";
  }
  operation.deleteAfterSuccess = map.value(QStringLiteral("deleteAfterSuccess"), false).toBool();
  operation.deleteLocalOnFailure = map.value(QStringLiteral("deleteLocalOnFailure"), false).toBool();
  operation.observedActive = map.value(QStringLiteral("observedActive"), false).toBool();
  operation.state = printing::directPrintStateFromPersisted(textValue(map, "state"));
  operation.createdAt = map.value(QStringLiteral("createdAt"), 0).toLongLong();
  operation.updatedAt = map.value(QStringLiteral("updatedAt"), 0).toLongLong();
  return operation;
}

QVariantMap PendingDirectPrintStore::toVariantMap(const printing::DirectPrintOperation& operation) {
  QVariantMap map;
  map.insert(QStringLiteral("printerId"), QString::fromStdString(operation.printerId));
  map.insert(QStringLiteral("cloudFileId"), QString::fromStdString(operation.cloudFileId));
  map.insert(QStringLiteral("cloudGcodeId"), QString::fromStdString(operation.cloudGcodeId));
  map.insert(QStringLiteral("cloudFileName"), QString::fromStdString(operation.cloudFileName));
  map.insert(QStringLiteral("cloudFileSize"), QVariant::fromValue<qulonglong>(operation.cloudFileSize));
  map.insert(QStringLiteral("printTaskId"), QString::fromStdString(operation.printTaskId));
  map.insert(QStringLiteral("printMsgId"), QString::fromStdString(operation.printMsgId));
  map.insert(QStringLiteral("printerLocalFilename"), QString::fromStdString(operation.printerLocalFilename));
  map.insert(QStringLiteral("printerLocalPath"), QString::fromStdString(operation.printerLocalPath));
  map.insert(QStringLiteral("deleteAfterSuccess"), operation.deleteAfterSuccess);
  map.insert(QStringLiteral("deleteLocalOnFailure"), operation.deleteLocalOnFailure);
  map.insert(QStringLiteral("observedActive"), operation.observedActive);
  map.insert(QStringLiteral("state"), QString::fromLatin1(printing::directPrintStateKey(operation.state).data(),
                                                           static_cast<int>(printing::directPrintStateKey(operation.state).size())));
  map.insert(QStringLiteral("createdAt"),
             QVariant::fromValue<qlonglong>(static_cast<qlonglong>(operation.createdAt)));
  map.insert(QStringLiteral("updatedAt"),
             QVariant::fromValue<qlonglong>(static_cast<qlonglong>(operation.updatedAt)));
  return map;
}

} // namespace accloud
