#include "app/LocalCacheStore.h"
#include "app/cache/LocalCacheSql.h"

#include "infra/logging/JsonlLogger.h"

#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <string>

namespace accloud {
using namespace cache;

QVariantMap LocalCacheStore::loadQuota() const {
  if (!ensureReady()) {
    return {};
  }

  QVariantMap out;
  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      closeAndRemoveDatabase(db, connectionName);
      return out;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT payload FROM quota WHERE id = 1"));
    if (q.exec() && q.next()) {
      out = parsePayload(q.value(0).toString());
    }
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return out;
}

bool LocalCacheStore::saveQuota(const QVariantMap& quota) const {
  if (!ensureReady()) {
    return false;
  }

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  bool ok = false;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO quota(id, payload, updated_at) VALUES(1, :payload, :updatedAt) "
                             "ON CONFLICT(id) DO UPDATE SET payload = excluded.payload, updated_at = excluded.updated_at"));
    q.bindValue(QStringLiteral(":payload"), encodePayload(quota));
    q.bindValue(QStringLiteral(":updatedAt"), nowEpochSec());
    ok = q.exec();

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return ok;
}

QVariantList LocalCacheStore::loadPendingDirectPrints() const {
  QVariantList out;
  if (!ensureReady()) return out;
  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) { closeAndRemoveDatabase(db, connectionName); return out; }
    QSqlQuery q(db);
    if (q.exec(QStringLiteral(
        "SELECT printer_id, cloud_file_id, cloud_gcode_id, cloud_file_name, cloud_file_size, "
        "print_task_id, print_msg_id, printer_local_filename, printer_local_path, "
        "delete_after_success, delete_local_on_failure, observed_active, state, created_at, updated_at "
        "FROM pending_direct_prints ORDER BY updated_at DESC"))) {
      while (q.next()) {
        QVariantMap m;
        m.insert(QStringLiteral("printerId"), q.value(0).toString());
        m.insert(QStringLiteral("cloudFileId"), q.value(1).toString());
        m.insert(QStringLiteral("cloudGcodeId"), q.value(2).toString());
        m.insert(QStringLiteral("cloudFileName"), q.value(3).toString());
        m.insert(QStringLiteral("cloudFileSize"), q.value(4).toLongLong());
        m.insert(QStringLiteral("printTaskId"), q.value(5).toString());
        m.insert(QStringLiteral("printMsgId"), q.value(6).toString());
        m.insert(QStringLiteral("printerLocalFilename"), q.value(7).toString());
        m.insert(QStringLiteral("printerLocalPath"), q.value(8).toString());
        m.insert(QStringLiteral("deleteAfterSuccess"), q.value(9).toInt() == 1);
        m.insert(QStringLiteral("deleteLocalOnFailure"), q.value(10).toInt() == 1);
        m.insert(QStringLiteral("observedActive"), q.value(11).toInt() == 1);
        m.insert(QStringLiteral("state"), q.value(12).toString());
        m.insert(QStringLiteral("createdAt"), q.value(13).toLongLong());
        m.insert(QStringLiteral("updatedAt"), q.value(14).toLongLong());
        out.append(m);
      }
    }
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return out;
}

bool LocalCacheStore::savePendingDirectPrint(const QVariantMap& operation) const {
  if (!ensureReady()) return false;
  const QString printerId = nonNullText(operation, QStringLiteral("printerId")).trimmed();
  if (printerId.isEmpty()) return false;
  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  bool ok = false;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) { closeAndRemoveDatabase(db, connectionName); return false; }
    const qint64 now = nowEpochSec();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
      "INSERT INTO pending_direct_prints(printer_id, cloud_file_id, cloud_gcode_id, cloud_file_name, cloud_file_size, "
      "print_task_id, print_msg_id, printer_local_filename, printer_local_path, delete_after_success, "
      "delete_local_on_failure, observed_active, state, created_at, updated_at) VALUES("
      ":printerId,:cloudFileId,:cloudGcodeId,:cloudFileName,:cloudFileSize,:printTaskId,:printMsgId,"
      ":localFilename,:localPath,:deleteAfter,:deleteOnFailure,:observedActive,:state,:createdAt,:updatedAt) "
      "ON CONFLICT(printer_id) DO UPDATE SET cloud_file_id=excluded.cloud_file_id, cloud_gcode_id=excluded.cloud_gcode_id, "
      "cloud_file_name=excluded.cloud_file_name, cloud_file_size=excluded.cloud_file_size, print_task_id=excluded.print_task_id, "
      "print_msg_id=excluded.print_msg_id, printer_local_filename=excluded.printer_local_filename, "
      "printer_local_path=excluded.printer_local_path, delete_after_success=excluded.delete_after_success, "
      "delete_local_on_failure=excluded.delete_local_on_failure, observed_active=excluded.observed_active, "
      "state=excluded.state, updated_at=excluded.updated_at"));
    q.bindValue(QStringLiteral(":printerId"), printerId);
    q.bindValue(QStringLiteral(":cloudFileId"), nonNullText(operation, QStringLiteral("cloudFileId")));
    q.bindValue(QStringLiteral(":cloudGcodeId"), nonNullText(operation, QStringLiteral("cloudGcodeId")));
    q.bindValue(QStringLiteral(":cloudFileName"), nonNullText(operation, QStringLiteral("cloudFileName")));
    q.bindValue(QStringLiteral(":cloudFileSize"), operation.value(QStringLiteral("cloudFileSize"), 0));
    q.bindValue(QStringLiteral(":printTaskId"), nonNullText(operation, QStringLiteral("printTaskId")));
    q.bindValue(QStringLiteral(":printMsgId"), nonNullText(operation, QStringLiteral("printMsgId")));
    q.bindValue(QStringLiteral(":localFilename"), nonNullText(operation, QStringLiteral("printerLocalFilename")));
    q.bindValue(QStringLiteral(":localPath"), nonNullText(operation, QStringLiteral("printerLocalPath"), QStringLiteral("/")));
    q.bindValue(QStringLiteral(":deleteAfter"), operation.value(QStringLiteral("deleteAfterSuccess")).toBool() ? 1 : 0);
    q.bindValue(QStringLiteral(":deleteOnFailure"), operation.value(QStringLiteral("deleteLocalOnFailure")).toBool() ? 1 : 0);
    q.bindValue(QStringLiteral(":observedActive"), operation.value(QStringLiteral("observedActive")).toBool() ? 1 : 0);
    q.bindValue(QStringLiteral(":state"), nonNullText(operation, QStringLiteral("state"), QStringLiteral("UPLOADED")));
    const qint64 createdAt = operation.value(QStringLiteral("createdAt"), now).toLongLong();
    q.bindValue(QStringLiteral(":createdAt"), createdAt > 0 ? createdAt : now);
    q.bindValue(QStringLiteral(":updatedAt"), now);
    ok = q.exec();
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return ok;
}

bool LocalCacheStore::removePendingDirectPrint(const QString& printerId) const {
  if (!ensureReady()) return false;
  const QString normalized = printerId.trimmed();
  if (normalized.isEmpty()) return false;
  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  bool ok = false;
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) { closeAndRemoveDatabase(db, connectionName); return false; }
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM pending_direct_prints WHERE printer_id=:printerId"));
    q.bindValue(QStringLiteral(":printerId"), normalized);
    ok = q.exec();
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return ok;
}

void LocalCacheStore::updateSyncState(const QString& scope, bool ok, const QString& errorMessage) const {
  if (!ensureReady()) {
    return;
  }

  const QString normalized = scope.trimmed().toLower();
  if (normalized.isEmpty()) {
    return;
  }

  const qint64 now = nowEpochSec();

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      closeAndRemoveDatabase(db, connectionName);
      return;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO sync_state(scope, last_success_at, last_attempt_at, last_status_ok, last_error) "
        "VALUES(:scope, :lastSuccessAt, :lastAttemptAt, :statusOk, :lastError) "
        "ON CONFLICT(scope) DO UPDATE SET "
        "  last_success_at = CASE "
        "    WHEN excluded.last_status_ok = 1 THEN excluded.last_success_at "
        "    ELSE sync_state.last_success_at "
        "  END, "
        "  last_attempt_at = excluded.last_attempt_at, "
        "  last_status_ok = excluded.last_status_ok, "
        "  last_error = excluded.last_error"));
    q.bindValue(QStringLiteral(":scope"), normalized);
    q.bindValue(QStringLiteral(":lastSuccessAt"), ok ? now : 0);
    q.bindValue(QStringLiteral(":lastAttemptAt"), now);
    q.bindValue(QStringLiteral(":statusOk"), ok ? 1 : 0);
    q.bindValue(QStringLiteral(":lastError"), ok ? QString{} : errorMessage.left(600));
    q.exec();

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
}

std::optional<LocalCacheStore::SyncState> LocalCacheStore::syncState(const QString& scope) const {
  if (!ensureReady()) {
    return std::nullopt;
  }

  const QString normalized = scope.trimmed().toLower();
  if (normalized.isEmpty()) {
    return std::nullopt;
  }

  SyncState state;
  bool found = false;

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      closeAndRemoveDatabase(db, connectionName);
      return std::nullopt;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT last_success_at, last_attempt_at, last_status_ok, last_error "
                             "FROM sync_state WHERE scope = :scope"));
    q.bindValue(QStringLiteral(":scope"), normalized);
    if (q.exec() && q.next()) {
      state.hasSuccess = q.value(0).toLongLong() > 0;
      state.lastSuccessAt = q.value(0).toLongLong();
      state.lastAttemptAt = q.value(1).toLongLong();
      state.lastStatusOk = q.value(2).toInt() == 1;
      state.lastError = q.value(3).toString();
      found = true;
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  if (!found) {
    return std::nullopt;
  }
  return state;
}

void LocalCacheStore::invalidateScope(const QString& scope) const {
  if (!ensureReady()) {
    return;
  }

  const QString normalized = scope.trimmed().toLower();
  if (normalized.isEmpty()) {
    return;
  }

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      closeAndRemoveDatabase(db, connectionName);
      return;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO sync_state(scope, last_success_at, last_attempt_at, last_status_ok, last_error) "
                             "VALUES(:scope, 0, 0, 0, '') "
                             "ON CONFLICT(scope) DO UPDATE SET last_success_at = 0"));
    q.bindValue(QStringLiteral(":scope"), normalized);
    q.exec();

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
}

void LocalCacheStore::cleanupRetention() const {
  if (!ensureReady()) {
    return;
  }

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      closeAndRemoveDatabase(db, connectionName);
      return;
    }
    enforceMaxRows(db, QStringLiteral("cloud_files"), QStringLiteral("file_id"), 1500);
    enforceMaxRows(db, QStringLiteral("cloud_printers"), QStringLiteral("printer_id"), 300);
    enforceMaxRows(db, QStringLiteral("jobs"), QStringLiteral("task_id"), 3000);
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
}

} // namespace accloud
