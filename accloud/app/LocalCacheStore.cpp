#include "LocalCacheStore.h"

#include "infra/logging/JsonlLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>
#include <QUuid>

#include <cstdlib>

namespace accloud {
namespace {

QMutex g_dbMutex;

QString resolveDbPath() {
  if (const char* env = std::getenv("ACCLOUD_DB_PATH"); env != nullptr && *env != '\0') {
    return QString::fromUtf8(env);
  }
  return QStringLiteral("accloud_cache.db");
}

qint64 readNow() {
  return QDateTime::currentSecsSinceEpoch();
}

QString newConnectionName() {
  return QStringLiteral("accloud_cache_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool runSchema(QSqlDatabase& db) {
  QSqlQuery q(db);
  const QStringList statements = {
      QStringLiteral("PRAGMA journal_mode=WAL"),
      QStringLiteral("PRAGMA synchronous=NORMAL"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS meta ("
                     "  key TEXT PRIMARY KEY,"
                     "  value TEXT NOT NULL"
                     ")"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS files ("
                     "  file_id TEXT PRIMARY KEY,"
                     "  payload TEXT NOT NULL,"
                     "  updated_at INTEGER NOT NULL"
                     ")"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS printers ("
                     "  printer_id TEXT PRIMARY KEY,"
                     "  payload TEXT NOT NULL,"
                     "  updated_at INTEGER NOT NULL"
                     ")"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS quota ("
                     "  id INTEGER PRIMARY KEY CHECK(id = 1),"
                     "  payload TEXT NOT NULL,"
                     "  updated_at INTEGER NOT NULL"
                     ")"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS sync_state ("
                     "  scope TEXT PRIMARY KEY,"
                     "  last_success_at INTEGER NOT NULL DEFAULT 0,"
                     "  last_attempt_at INTEGER NOT NULL DEFAULT 0,"
                     "  last_status_ok INTEGER NOT NULL DEFAULT 0,"
                     "  last_error TEXT NOT NULL DEFAULT ''"
                     ")")};

  for (const QString& sql : statements) {
    if (!q.exec(sql)) {
      logging::error("app", "local_cache", "schema_exec_failed",
                     "Unable to execute cache schema statement",
                     {{"sql", sql.toStdString()}, {"error", q.lastError().text().toStdString()}});
      return false;
    }
  }

  QSqlQuery setVersion(db);
  setVersion.prepare(QStringLiteral("INSERT INTO meta(key, value) VALUES('schema_version','1') "
                                    "ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
  if (!setVersion.exec()) {
    logging::warn("app", "local_cache", "schema_version_write_failed",
                  "Unable to write cache schema version",
                  {{"error", setVersion.lastError().text().toStdString()}});
  }
  return true;
}

QVariantMap decodePayloadText(const QString& payload) {
  const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
  if (!doc.isObject()) {
    return {};
  }
  return doc.object().toVariantMap();
}

QString encodePayloadMap(const QVariantMap& map) {
  const QJsonObject obj = QJsonObject::fromVariantMap(map);
  const QJsonDocument doc(obj);
  return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

void enforceMaxRows(QSqlDatabase& db, const QString& table, const QString& idColumn, int maxRows) {
  if (maxRows <= 0) return;

  QSqlQuery countQuery(db);
  if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM ") + table)) {
    return;
  }
  if (!countQuery.next()) {
    return;
  }

  const int count = countQuery.value(0).toInt();
  const int extra = count - maxRows;
  if (extra <= 0) {
    return;
  }

  QSqlQuery prune(db);
  const QString sql = QStringLiteral("DELETE FROM ") + table
                      + QStringLiteral(" WHERE ") + idColumn
                      + QStringLiteral(" IN (SELECT ") + idColumn
                      + QStringLiteral(" FROM ") + table
                      + QStringLiteral(" ORDER BY updated_at ASC LIMIT :extra)");
  prune.prepare(sql);
  prune.bindValue(QStringLiteral(":extra"), extra);
  prune.exec();
}

} // namespace

LocalCacheStore::LocalCacheStore()
    : m_dbPath(resolveDbPath()) {
  const QFileInfo info(m_dbPath);
  if (!info.absoluteDir().exists() && !info.absolutePath().isEmpty()) {
    QDir().mkpath(info.absolutePath());
  }
}

bool LocalCacheStore::isAvailable() const {
  return ensureReady();
}

QString LocalCacheStore::databasePath() const {
  return m_dbPath;
}

bool LocalCacheStore::ensureReady() const {
  QMutexLocker lock(&g_dbMutex);
  if (m_ready) {
    return true;
  }

  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      logging::error("app", "local_cache", "db_open_failed", "Unable to open local cache database",
                     {{"path", m_dbPath.toStdString()}, {"error", db.lastError().text().toStdString()}});
      QSqlDatabase::removeDatabase(connectionName);
      return false;
    }

    m_ready = runSchema(db);
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return m_ready;
}

bool LocalCacheStore::migrate() const {
  return ensureReady();
}

QVariantMap LocalCacheStore::parsePayload(const QString& payload) const {
  return decodePayloadText(payload);
}

QString LocalCacheStore::encodePayload(const QVariantMap& map) const {
  return encodePayloadMap(map);
}

qint64 LocalCacheStore::nowEpochSec() const {
  return readNow();
}

QVariantList LocalCacheStore::loadFiles(int page, int limit) const {
  QVariantList out;
  if (!ensureReady()) {
    return out;
  }

  if (page < 1) page = 1;
  if (limit <= 0) limit = 20;
  const int offset = (page - 1) * limit;

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      QSqlDatabase::removeDatabase(connectionName);
      return out;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT payload FROM files ORDER BY updated_at DESC LIMIT :limit OFFSET :offset"));
    q.bindValue(QStringLiteral(":limit"), limit);
    q.bindValue(QStringLiteral(":offset"), offset);
    if (q.exec()) {
      while (q.next()) {
        out.append(parsePayload(q.value(0).toString()));
      }
    }
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return out;
}

QVariantList LocalCacheStore::loadPrinters() const {
  QVariantList out;
  if (!ensureReady()) {
    return out;
  }

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      QSqlDatabase::removeDatabase(connectionName);
      return out;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT payload FROM printers ORDER BY updated_at DESC"));
    if (q.exec()) {
      while (q.next()) {
        out.append(parsePayload(q.value(0).toString()));
      }
    }
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return out;
}

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
      QSqlDatabase::removeDatabase(connectionName);
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

bool LocalCacheStore::replaceFiles(const QVariantList& files) const {
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
      QSqlDatabase::removeDatabase(connectionName);
      return false;
    }

    const qint64 now = nowEpochSec();
    if (!db.transaction()) {
      db.close();
      QSqlDatabase::removeDatabase(connectionName);
      return false;
    }

    QSqlQuery clear(db);
    ok = clear.exec(QStringLiteral("DELETE FROM files"));

    if (ok) {
      QSqlQuery ins(db);
      ins.prepare(QStringLiteral("INSERT INTO files(file_id, payload, updated_at) VALUES(:id, :payload, :updatedAt)"));
      for (const QVariant& item : files) {
        const QVariantMap map = item.toMap();
        const QString id = map.value(QStringLiteral("fileId")).toString();
        if (id.trimmed().isEmpty()) continue;
        ins.bindValue(QStringLiteral(":id"), id);
        ins.bindValue(QStringLiteral(":payload"), encodePayload(map));
        ins.bindValue(QStringLiteral(":updatedAt"), now);
        if (!ins.exec()) {
          ok = false;
          break;
        }
      }
    }

    if (ok) {
      enforceMaxRows(db, QStringLiteral("files"), QStringLiteral("file_id"), 1500);
      ok = db.commit();
    } else {
      db.rollback();
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return ok;
}

bool LocalCacheStore::replacePrinters(const QVariantList& printers) const {
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
      QSqlDatabase::removeDatabase(connectionName);
      return false;
    }

    const qint64 now = nowEpochSec();
    if (!db.transaction()) {
      db.close();
      QSqlDatabase::removeDatabase(connectionName);
      return false;
    }

    QSqlQuery clear(db);
    ok = clear.exec(QStringLiteral("DELETE FROM printers"));

    if (ok) {
      QSqlQuery ins(db);
      ins.prepare(QStringLiteral("INSERT INTO printers(printer_id, payload, updated_at) VALUES(:id, :payload, :updatedAt)"));
      for (const QVariant& item : printers) {
        const QVariantMap map = item.toMap();
        const QString id = map.value(QStringLiteral("id")).toString();
        if (id.trimmed().isEmpty()) continue;
        ins.bindValue(QStringLiteral(":id"), id);
        ins.bindValue(QStringLiteral(":payload"), encodePayload(map));
        ins.bindValue(QStringLiteral(":updatedAt"), now);
        if (!ins.exec()) {
          ok = false;
          break;
        }
      }
    }

    if (ok) {
      enforceMaxRows(db, QStringLiteral("printers"), QStringLiteral("printer_id"), 300);
      ok = db.commit();
    } else {
      db.rollback();
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return ok;
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
      QSqlDatabase::removeDatabase(connectionName);
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

void LocalCacheStore::removeFile(const QString& fileId) const {
  if (!ensureReady()) {
    return;
  }

  if (fileId.trimmed().isEmpty()) {
    return;
  }

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      QSqlDatabase::removeDatabase(connectionName);
      return;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM files WHERE file_id = :id"));
    q.bindValue(QStringLiteral(":id"), fileId.trimmed());
    q.exec();
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
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
      QSqlDatabase::removeDatabase(connectionName);
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

  QMutexLocker lock(&g_dbMutex);
  const QString connectionName = newConnectionName();
  {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
      QSqlDatabase::removeDatabase(connectionName);
      return std::nullopt;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT last_success_at, last_attempt_at, last_status_ok, last_error "
                             "FROM sync_state WHERE scope = :scope"));
    q.bindValue(QStringLiteral(":scope"), normalized);
    if (!q.exec() || !q.next()) {
      db.close();
      QSqlDatabase::removeDatabase(connectionName);
      return std::nullopt;
    }

    state.hasSuccess = q.value(0).toLongLong() > 0;
    state.lastSuccessAt = q.value(0).toLongLong();
    state.lastAttemptAt = q.value(1).toLongLong();
    state.lastStatusOk = q.value(2).toInt() == 1;
    state.lastError = q.value(3).toString();

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
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
      QSqlDatabase::removeDatabase(connectionName);
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
      QSqlDatabase::removeDatabase(connectionName);
      return;
    }
    enforceMaxRows(db, QStringLiteral("files"), QStringLiteral("file_id"), 1500);
    enforceMaxRows(db, QStringLiteral("printers"), QStringLiteral("printer_id"), 300);
    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
}

} // namespace accloud
