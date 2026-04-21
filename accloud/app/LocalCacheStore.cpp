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

void closeAndRemoveDatabase(QSqlDatabase& db, const QString& connectionName) {
  if (db.isValid()) {
    db.close();
    db = QSqlDatabase();
  }
  QSqlDatabase::removeDatabase(connectionName);
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

bool tableHasRows(QSqlDatabase& db, const QString& table) {
  QSqlQuery q(db);
  if (!q.exec(QStringLiteral("SELECT 1 FROM ") + table + QStringLiteral(" LIMIT 1"))) {
    return false;
  }
  return q.next();
}

bool tableHasColumn(QSqlDatabase& db, const QString& table, const QString& column) {
  QSqlQuery q(db);
  if (!q.exec(QStringLiteral("PRAGMA table_info(") + table + QStringLiteral(")"))) {
    return false;
  }
  while (q.next()) {
    if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0) {
      return true;
    }
  }
  return false;
}

bool ensureColumnExists(QSqlDatabase& db,
                        const QString& table,
                        const QString& column,
                        const QString& definition) {
  if (tableHasColumn(db, table, column)) {
    return true;
  }

  QSqlQuery alter(db);
  const QString sql = QStringLiteral("ALTER TABLE ") + table
                      + QStringLiteral(" ADD COLUMN ") + definition;
  if (!alter.exec(sql)) {
    logging::error("app", "local_cache", "schema_alter_failed",
                   "Unable to alter cache schema for new column",
                   {{"table", table.toStdString()},
                    {"column", column.toStdString()},
                    {"error", alter.lastError().text().toStdString()}});
    return false;
  }
  return true;
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

bool migrateLegacyFiles(QSqlDatabase& db) {
  if (tableHasRows(db, QStringLiteral("cloud_files"))) {
    return true;
  }

  QSqlQuery read(db);
  if (!read.exec(QStringLiteral("SELECT payload, updated_at FROM files ORDER BY updated_at DESC"))) {
    return true;
  }

  QSqlQuery ins(db);
  ins.prepare(QStringLiteral(
      "INSERT OR REPLACE INTO cloud_files("
      "file_id, file_name, status, size_bytes, size_text, machine, material, upload_time, print_time, "
      "layer_thickness, layers, is_pwmb, resin_usage, dimensions, thumbnail_url, gcode_id, updated_at"
      ") VALUES("
      ":id, :name, :status, :sizeBytes, :sizeText, :machine, :material, :uploadTime, :printTime, "
      ":layerThickness, :layers, :isPwmb, :resinUsage, :dimensions, :thumbnailUrl, :gcodeId, :updatedAt"
      ")"));

  while (read.next()) {
    const QVariantMap map = decodePayloadText(read.value(0).toString());
    const QString id = map.value(QStringLiteral("fileId")).toString().trimmed();
    if (id.isEmpty()) continue;

    const qint64 updatedAt = read.value(1).toLongLong();
    ins.bindValue(QStringLiteral(":id"), id);
    ins.bindValue(QStringLiteral(":name"), map.value(QStringLiteral("fileName")).toString());
    ins.bindValue(QStringLiteral(":status"), map.value(QStringLiteral("status")).toString());
    ins.bindValue(QStringLiteral(":sizeBytes"), map.value(QStringLiteral("sizeBytes"), 0));
    ins.bindValue(QStringLiteral(":sizeText"), map.value(QStringLiteral("sizeText")).toString());
    ins.bindValue(QStringLiteral(":machine"), map.value(QStringLiteral("machine")).toString());
    ins.bindValue(QStringLiteral(":material"), map.value(QStringLiteral("material")).toString());
    ins.bindValue(QStringLiteral(":uploadTime"), map.value(QStringLiteral("uploadTime")).toString());
    ins.bindValue(QStringLiteral(":printTime"), map.value(QStringLiteral("printTime")).toString());
    ins.bindValue(QStringLiteral(":layerThickness"), map.value(QStringLiteral("layerThickness")).toString());
    ins.bindValue(QStringLiteral(":layers"), map.value(QStringLiteral("layers"), 0));
    ins.bindValue(QStringLiteral(":isPwmb"), map.value(QStringLiteral("isPwmb"), false).toBool() ? 1 : 0);
    ins.bindValue(QStringLiteral(":resinUsage"), map.value(QStringLiteral("resinUsage")).toString());
    ins.bindValue(QStringLiteral(":dimensions"), map.value(QStringLiteral("dimensions")).toString());
    ins.bindValue(QStringLiteral(":thumbnailUrl"), map.value(QStringLiteral("thumbnailUrl")).toString());
    ins.bindValue(QStringLiteral(":gcodeId"), map.value(QStringLiteral("gcodeId")).toString());
    ins.bindValue(QStringLiteral(":updatedAt"), updatedAt > 0 ? updatedAt : readNow());
    if (!ins.exec()) {
      return false;
    }
  }

  return true;
}

bool migrateLegacyPrinters(QSqlDatabase& db) {
  if (tableHasRows(db, QStringLiteral("cloud_printers"))) {
    return true;
  }

  QSqlQuery read(db);
  if (!read.exec(QStringLiteral("SELECT payload, updated_at FROM printers ORDER BY updated_at DESC"))) {
    return true;
  }

  QSqlQuery ins(db);
  ins.prepare(QStringLiteral(
      "INSERT OR REPLACE INTO cloud_printers("
      "printer_id, printer_key, machine_type, name, model, type, last_seen, state, reason, available, current_file, updated_at"
      ") VALUES("
      ":id, :printerKey, :machineType, :name, :model, :type, :lastSeen, :state, :reason, :available, :currentFile, :updatedAt"
      ")"));

  while (read.next()) {
    const QVariantMap map = decodePayloadText(read.value(0).toString());
    const QString id = map.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) continue;

    const qint64 updatedAt = read.value(1).toLongLong();
    const QString printerKey = map.value(QStringLiteral("printerKey")).toString().trimmed().isEmpty()
        ? map.value(QStringLiteral("key")).toString().trimmed()
        : map.value(QStringLiteral("printerKey")).toString().trimmed();
    ins.bindValue(QStringLiteral(":id"), id);
    ins.bindValue(QStringLiteral(":printerKey"), printerKey);
    ins.bindValue(QStringLiteral(":machineType"), map.value(QStringLiteral("machineType")).toString());
    ins.bindValue(QStringLiteral(":name"), map.value(QStringLiteral("name")).toString());
    ins.bindValue(QStringLiteral(":model"), map.value(QStringLiteral("model")).toString());
    ins.bindValue(QStringLiteral(":type"), map.value(QStringLiteral("type")).toString());
    ins.bindValue(QStringLiteral(":lastSeen"), map.value(QStringLiteral("lastSeen")).toString());
    ins.bindValue(QStringLiteral(":state"), map.value(QStringLiteral("state")).toString());
    ins.bindValue(QStringLiteral(":reason"), map.value(QStringLiteral("reason")).toString());
    ins.bindValue(QStringLiteral(":available"), map.value(QStringLiteral("available"), -1));
    ins.bindValue(QStringLiteral(":currentFile"), map.value(QStringLiteral("currentFile")).toString());
    ins.bindValue(QStringLiteral(":updatedAt"), updatedAt > 0 ? updatedAt : readNow());
    if (!ins.exec()) {
      return false;
    }
  }

  return true;
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
      // Legacy payload tables kept for backward-compatible reads.
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
      QStringLiteral("CREATE TABLE IF NOT EXISTS cloud_files ("
                     "  file_id TEXT PRIMARY KEY,"
                     "  file_name TEXT NOT NULL DEFAULT '',"
                     "  status TEXT NOT NULL DEFAULT 'UNKNOWN',"
                     "  size_bytes INTEGER NOT NULL DEFAULT 0,"
                     "  size_text TEXT NOT NULL DEFAULT '',"
                     "  machine TEXT NOT NULL DEFAULT '',"
                     "  material TEXT NOT NULL DEFAULT '',"
                     "  upload_time TEXT NOT NULL DEFAULT '',"
                     "  print_time TEXT NOT NULL DEFAULT '',"
                     "  layer_thickness TEXT NOT NULL DEFAULT '',"
                     "  layers INTEGER NOT NULL DEFAULT 0,"
                     "  is_pwmb INTEGER NOT NULL DEFAULT 0,"
                     "  resin_usage TEXT NOT NULL DEFAULT '',"
                     "  dimensions TEXT NOT NULL DEFAULT '',"
                     "  thumbnail_url TEXT NOT NULL DEFAULT '',"
                     "  gcode_id TEXT NOT NULL DEFAULT '',"
                     "  updated_at INTEGER NOT NULL"
                     ")"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS cloud_printers ("
                     "  printer_id TEXT PRIMARY KEY,"
                     "  printer_key TEXT NOT NULL DEFAULT '',"
                     "  machine_type TEXT NOT NULL DEFAULT '',"
                     "  name TEXT NOT NULL DEFAULT '',"
                     "  model TEXT NOT NULL DEFAULT '',"
                     "  type TEXT NOT NULL DEFAULT '',"
                     "  last_seen TEXT NOT NULL DEFAULT '',"
                     "  state TEXT NOT NULL DEFAULT 'UNKNOWN',"
                     "  reason TEXT NOT NULL DEFAULT '',"
                     "  available INTEGER NOT NULL DEFAULT -1,"
                     "  current_file TEXT NOT NULL DEFAULT '',"
                     "  updated_at INTEGER NOT NULL"
                     ")"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS jobs ("
                     "  task_id TEXT PRIMARY KEY,"
                     "  printer_id TEXT NOT NULL DEFAULT '',"
                     "  printer_name TEXT NOT NULL DEFAULT '',"
                     "  gcode_name TEXT NOT NULL DEFAULT '',"
                     "  print_status INTEGER NOT NULL DEFAULT 0,"
                     "  reason TEXT NOT NULL DEFAULT '',"
                     "  create_time INTEGER NOT NULL DEFAULT 0,"
                     "  end_time INTEGER NOT NULL DEFAULT 0,"
                     "  img TEXT NOT NULL DEFAULT '',"
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
                     ")"),
      QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cloud_files_updated_at ON cloud_files(updated_at DESC)"),
      QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cloud_printers_updated_at ON cloud_printers(updated_at DESC)"),
      QStringLiteral("CREATE INDEX IF NOT EXISTS idx_jobs_printer_id ON jobs(printer_id)"),
      QStringLiteral("CREATE INDEX IF NOT EXISTS idx_jobs_create_time ON jobs(create_time DESC)")};

  for (const QString& sql : statements) {
    if (!q.exec(sql)) {
      logging::error("app", "local_cache", "schema_exec_failed",
                     "Unable to execute cache schema statement",
                     {{"sql", sql.toStdString()}, {"error", q.lastError().text().toStdString()}});
      return false;
    }
  }

  if (!ensureColumnExists(db, QStringLiteral("cloud_printers"),
                          QStringLiteral("printer_key"),
                          QStringLiteral("printer_key TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("machine_type"),
                             QStringLiteral("machine_type TEXT NOT NULL DEFAULT ''"))) {
    return false;
  }

  if (!db.transaction()) {
    return false;
  }

  const bool migrated = migrateLegacyFiles(db) && migrateLegacyPrinters(db);
  if (!migrated) {
    db.rollback();
    logging::warn("app", "local_cache", "legacy_migration_failed",
                  "Legacy payload migration failed, keeping typed tables as-is");
  } else {
    db.commit();
  }

  QSqlQuery setVersion(db);
  setVersion.prepare(QStringLiteral("INSERT INTO meta(key, value) VALUES('schema_version','2') "
                                    "ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
  if (!setVersion.exec()) {
    logging::warn("app", "local_cache", "schema_version_write_failed",
                  "Unable to write cache schema version",
                  {{"error", setVersion.lastError().text().toStdString()}});
  }
  return true;
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
      closeAndRemoveDatabase(db, connectionName);
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
      closeAndRemoveDatabase(db, connectionName);
      return out;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT file_id, file_name, status, size_bytes, size_text, machine, material, upload_time, print_time, "
        "layer_thickness, layers, is_pwmb, resin_usage, dimensions, thumbnail_url, gcode_id "
        "FROM cloud_files ORDER BY updated_at DESC LIMIT :limit OFFSET :offset"));
    q.bindValue(QStringLiteral(":limit"), limit);
    q.bindValue(QStringLiteral(":offset"), offset);
    if (q.exec()) {
      while (q.next()) {
        QVariantMap item;
        item.insert("fileId", q.value(0).toString());
        item.insert("fileName", q.value(1).toString());
        item.insert("status", q.value(2).toString());
        item.insert("sizeBytes", q.value(3).toULongLong());
        item.insert("sizeText", q.value(4).toString());
        item.insert("machine", q.value(5).toString());
        item.insert("material", q.value(6).toString());
        item.insert("uploadTime", q.value(7).toString());
        item.insert("printTime", q.value(8).toString());
        item.insert("layerThickness", q.value(9).toString());
        item.insert("layers", q.value(10).toInt());
        item.insert("isPwmb", q.value(11).toInt() == 1);
        item.insert("resinUsage", q.value(12).toString());
        item.insert("dimensions", q.value(13).toString());
        item.insert("thumbnailUrl", q.value(14).toString());
        item.insert("gcodeId", q.value(15).toString());
        out.append(item);
      }
    }

    // Legacy fallback for pre-v2 typed schema.
    if (out.isEmpty()) {
      QSqlQuery legacy(db);
      legacy.prepare(QStringLiteral("SELECT payload FROM files ORDER BY updated_at DESC LIMIT :limit OFFSET :offset"));
      legacy.bindValue(QStringLiteral(":limit"), limit);
      legacy.bindValue(QStringLiteral(":offset"), offset);
      if (legacy.exec()) {
        while (legacy.next()) {
          out.append(parsePayload(legacy.value(0).toString()));
        }
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
      closeAndRemoveDatabase(db, connectionName);
      return out;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT printer_id, printer_key, machine_type, name, model, type, last_seen, state, reason, available, current_file "
        "FROM cloud_printers ORDER BY updated_at DESC"));
    if (q.exec()) {
      while (q.next()) {
        QVariantMap item;
        item.insert("id", q.value(0).toString());
        item.insert("printerKey", q.value(1).toString());
        item.insert("machineType", q.value(2).toString());
        item.insert("name", q.value(3).toString());
        item.insert("model", q.value(4).toString());
        item.insert("type", q.value(5).toString());
        item.insert("lastSeen", q.value(6).toString());
        item.insert("state", q.value(7).toString());
        item.insert("reason", q.value(8).toString());
        item.insert("available", q.value(9).toInt());
        item.insert("currentFile", q.value(10).toString());
        out.append(item);
      }
    }

    // Legacy fallback for pre-v2 typed schema.
    if (out.isEmpty()) {
      QSqlQuery legacy(db);
      legacy.prepare(QStringLiteral("SELECT payload FROM printers ORDER BY updated_at DESC"));
      if (legacy.exec()) {
        while (legacy.next()) {
          out.append(parsePayload(legacy.value(0).toString()));
        }
      }
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return out;
}

QVariantList LocalCacheStore::loadJobsForPrinter(const QString& printerId, int page, int limit) const {
  QVariantList out;
  if (!ensureReady()) {
    return out;
  }

  const QString normalizedPrinterId = printerId.trimmed();
  if (normalizedPrinterId.isEmpty()) {
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
      closeAndRemoveDatabase(db, connectionName);
      return out;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT task_id, gcode_name, printer_id, printer_name, print_status, reason, create_time, end_time, img "
        "FROM jobs WHERE printer_id = :printerId ORDER BY create_time DESC, updated_at DESC LIMIT :limit OFFSET :offset"));
    q.bindValue(QStringLiteral(":printerId"), normalizedPrinterId);
    q.bindValue(QStringLiteral(":limit"), limit);
    q.bindValue(QStringLiteral(":offset"), offset);
    if (q.exec()) {
      while (q.next()) {
        QVariantMap item;
        item.insert("taskId", q.value(0).toString());
        item.insert("gcodeName", q.value(1).toString());
        item.insert("printerId", q.value(2).toString());
        item.insert("printerName", q.value(3).toString());
        item.insert("printStatus", q.value(4).toInt());
        item.insert("reason", q.value(5).toString());
        item.insert("createTime", q.value(6).toLongLong());
        item.insert("endTime", q.value(7).toLongLong());
        item.insert("img", q.value(8).toString());
        out.append(item);
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
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    const qint64 now = nowEpochSec();
    if (!db.transaction()) {
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    QSqlQuery clear(db);
    ok = clear.exec(QStringLiteral("DELETE FROM cloud_files"));

    if (ok) {
      QSqlQuery ins(db);
      ins.prepare(QStringLiteral(
          "INSERT INTO cloud_files("
          "file_id, file_name, status, size_bytes, size_text, machine, material, upload_time, print_time, "
          "layer_thickness, layers, is_pwmb, resin_usage, dimensions, thumbnail_url, gcode_id, updated_at"
          ") VALUES("
          ":id, :name, :status, :sizeBytes, :sizeText, :machine, :material, :uploadTime, :printTime, "
          ":layerThickness, :layers, :isPwmb, :resinUsage, :dimensions, :thumbnailUrl, :gcodeId, :updatedAt"
          ")"));
      for (const QVariant& item : files) {
        const QVariantMap map = item.toMap();
        const QString id = map.value(QStringLiteral("fileId")).toString().trimmed();
        if (id.isEmpty()) continue;

        ins.bindValue(QStringLiteral(":id"), id);
        ins.bindValue(QStringLiteral(":name"), map.value(QStringLiteral("fileName")).toString());
        ins.bindValue(QStringLiteral(":status"), map.value(QStringLiteral("status"), QStringLiteral("UNKNOWN")));
        ins.bindValue(QStringLiteral(":sizeBytes"), map.value(QStringLiteral("sizeBytes"), 0));
        ins.bindValue(QStringLiteral(":sizeText"), map.value(QStringLiteral("sizeText")).toString());
        ins.bindValue(QStringLiteral(":machine"), map.value(QStringLiteral("machine")).toString());
        ins.bindValue(QStringLiteral(":material"), map.value(QStringLiteral("material")).toString());
        ins.bindValue(QStringLiteral(":uploadTime"), map.value(QStringLiteral("uploadTime")).toString());
        ins.bindValue(QStringLiteral(":printTime"), map.value(QStringLiteral("printTime")).toString());
        ins.bindValue(QStringLiteral(":layerThickness"), map.value(QStringLiteral("layerThickness")).toString());
        ins.bindValue(QStringLiteral(":layers"), map.value(QStringLiteral("layers"), 0));
        ins.bindValue(QStringLiteral(":isPwmb"), map.value(QStringLiteral("isPwmb"), false).toBool() ? 1 : 0);
        ins.bindValue(QStringLiteral(":resinUsage"), map.value(QStringLiteral("resinUsage")).toString());
        ins.bindValue(QStringLiteral(":dimensions"), map.value(QStringLiteral("dimensions")).toString());
        ins.bindValue(QStringLiteral(":thumbnailUrl"), map.value(QStringLiteral("thumbnailUrl")).toString());
        ins.bindValue(QStringLiteral(":gcodeId"), map.value(QStringLiteral("gcodeId")).toString());
        ins.bindValue(QStringLiteral(":updatedAt"), now);
        if (!ins.exec()) {
          ok = false;
          break;
        }
      }
    }

    if (ok) {
      enforceMaxRows(db, QStringLiteral("cloud_files"), QStringLiteral("file_id"), 1500);
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
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    const qint64 now = nowEpochSec();
    if (!db.transaction()) {
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    QSqlQuery clear(db);
    ok = clear.exec(QStringLiteral("DELETE FROM cloud_printers"));

    if (ok) {
      QSqlQuery ins(db);
      ins.prepare(QStringLiteral(
          "INSERT INTO cloud_printers("
          "printer_id, printer_key, machine_type, name, model, type, last_seen, state, reason, available, current_file, updated_at"
          ") VALUES("
          ":id, :printerKey, :machineType, :name, :model, :type, :lastSeen, :state, :reason, :available, :currentFile, :updatedAt"
          ")"));
      for (const QVariant& item : printers) {
        const QVariantMap map = item.toMap();
        const QString id = map.value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty()) continue;

        const QString printerKey = map.value(QStringLiteral("printerKey")).toString().trimmed().isEmpty()
            ? map.value(QStringLiteral("key")).toString().trimmed()
            : map.value(QStringLiteral("printerKey")).toString().trimmed();
        ins.bindValue(QStringLiteral(":id"), id);
        ins.bindValue(QStringLiteral(":printerKey"), printerKey);
        ins.bindValue(QStringLiteral(":machineType"), map.value(QStringLiteral("machineType")).toString());
        ins.bindValue(QStringLiteral(":name"), map.value(QStringLiteral("name")).toString());
        ins.bindValue(QStringLiteral(":model"), map.value(QStringLiteral("model")).toString());
        ins.bindValue(QStringLiteral(":type"), map.value(QStringLiteral("type")).toString());
        ins.bindValue(QStringLiteral(":lastSeen"), map.value(QStringLiteral("lastSeen")).toString());
        ins.bindValue(QStringLiteral(":state"), map.value(QStringLiteral("state"), QStringLiteral("UNKNOWN")));
        ins.bindValue(QStringLiteral(":reason"), map.value(QStringLiteral("reason")).toString());
        ins.bindValue(QStringLiteral(":available"), map.value(QStringLiteral("available"), -1));
        ins.bindValue(QStringLiteral(":currentFile"), map.value(QStringLiteral("currentFile")).toString());
        ins.bindValue(QStringLiteral(":updatedAt"), now);
        if (!ins.exec()) {
          ok = false;
          break;
        }
      }
    }

    if (ok) {
      enforceMaxRows(db, QStringLiteral("cloud_printers"), QStringLiteral("printer_id"), 300);
      ok = db.commit();
    } else {
      db.rollback();
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return ok;
}

bool LocalCacheStore::replaceJobs(const QVariantList& jobs) const {
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

    const qint64 now = nowEpochSec();
    if (!db.transaction()) {
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    QSqlQuery clear(db);
    ok = clear.exec(QStringLiteral("DELETE FROM jobs"));

    if (ok) {
      QSqlQuery ins(db);
      ins.prepare(QStringLiteral(
          "INSERT INTO jobs("
          "task_id, printer_id, printer_name, gcode_name, print_status, reason, create_time, end_time, img, updated_at"
          ") VALUES("
          ":taskId, :printerId, :printerName, :gcodeName, :printStatus, :reason, :createTime, :endTime, :img, :updatedAt"
          ")"));
      for (const QVariant& item : jobs) {
        const QVariantMap map = item.toMap();
        const QString taskId = map.value(QStringLiteral("taskId")).toString().trimmed();
        const QString printerId = map.value(QStringLiteral("printerId")).toString().trimmed();
        if (taskId.isEmpty() || printerId.isEmpty()) continue;

        ins.bindValue(QStringLiteral(":taskId"), taskId);
        ins.bindValue(QStringLiteral(":printerId"), printerId);
        ins.bindValue(QStringLiteral(":printerName"), map.value(QStringLiteral("printerName")).toString());
        ins.bindValue(QStringLiteral(":gcodeName"), map.value(QStringLiteral("gcodeName")).toString());
        ins.bindValue(QStringLiteral(":printStatus"), map.value(QStringLiteral("printStatus"), 0));
        ins.bindValue(QStringLiteral(":reason"), map.value(QStringLiteral("reason")).toString());
        ins.bindValue(QStringLiteral(":createTime"), map.value(QStringLiteral("createTime"), 0));
        ins.bindValue(QStringLiteral(":endTime"), map.value(QStringLiteral("endTime"), 0));
        ins.bindValue(QStringLiteral(":img"), map.value(QStringLiteral("img")).toString());
        ins.bindValue(QStringLiteral(":updatedAt"), now);
        if (!ins.exec()) {
          ok = false;
          break;
        }
      }
    }

    if (ok) {
      enforceMaxRows(db, QStringLiteral("jobs"), QStringLiteral("task_id"), 3000);
      ok = db.commit();
    } else {
      db.rollback();
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return ok;
}

bool LocalCacheStore::replaceJobsForPrinter(const QString& printerId, const QVariantList& jobs) const {
  if (!ensureReady()) {
    return false;
  }

  const QString normalizedPrinterId = printerId.trimmed();
  if (normalizedPrinterId.isEmpty()) {
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

    const qint64 now = nowEpochSec();
    if (!db.transaction()) {
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    QSqlQuery clear(db);
    clear.prepare(QStringLiteral("DELETE FROM jobs WHERE printer_id = :printerId"));
    clear.bindValue(QStringLiteral(":printerId"), normalizedPrinterId);
    ok = clear.exec();

    if (ok) {
      QSqlQuery ins(db);
      ins.prepare(QStringLiteral(
          "INSERT INTO jobs("
          "task_id, printer_id, printer_name, gcode_name, print_status, reason, create_time, end_time, img, updated_at"
          ") VALUES("
          ":taskId, :printerId, :printerName, :gcodeName, :printStatus, :reason, :createTime, :endTime, :img, :updatedAt"
          ")"));
      for (const QVariant& item : jobs) {
        const QVariantMap map = item.toMap();
        const QString taskId = map.value(QStringLiteral("taskId")).toString().trimmed();
        if (taskId.isEmpty()) continue;

        ins.bindValue(QStringLiteral(":taskId"), taskId);
        ins.bindValue(QStringLiteral(":printerId"), normalizedPrinterId);
        ins.bindValue(QStringLiteral(":printerName"), map.value(QStringLiteral("printerName")).toString());
        ins.bindValue(QStringLiteral(":gcodeName"), map.value(QStringLiteral("gcodeName")).toString());
        ins.bindValue(QStringLiteral(":printStatus"), map.value(QStringLiteral("printStatus"), 0));
        ins.bindValue(QStringLiteral(":reason"), map.value(QStringLiteral("reason")).toString());
        ins.bindValue(QStringLiteral(":createTime"), map.value(QStringLiteral("createTime"), 0));
        ins.bindValue(QStringLiteral(":endTime"), map.value(QStringLiteral("endTime"), 0));
        ins.bindValue(QStringLiteral(":img"), map.value(QStringLiteral("img")).toString());
        ins.bindValue(QStringLiteral(":updatedAt"), now);
        if (!ins.exec()) {
          ok = false;
          break;
        }
      }
    }

    if (ok) {
      enforceMaxRows(db, QStringLiteral("jobs"), QStringLiteral("task_id"), 3000);
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

void LocalCacheStore::removeFile(const QString& fileId) const {
  if (!ensureReady()) {
    return;
  }

  const QString normalized = fileId.trimmed();
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
    q.prepare(QStringLiteral("DELETE FROM cloud_files WHERE file_id = :id"));
    q.bindValue(QStringLiteral(":id"), normalized);
    q.exec();

    QSqlQuery qLegacy(db);
    qLegacy.prepare(QStringLiteral("DELETE FROM files WHERE file_id = :id"));
    qLegacy.bindValue(QStringLiteral(":id"), normalized);
    qLegacy.exec();

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
