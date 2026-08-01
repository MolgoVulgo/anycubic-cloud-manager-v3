#include "app/cache/LocalCacheSql.h"
#include "app/UserPaths.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUuid>

#include <cstdlib>
#include <string>

namespace accloud::cache {
QMutex g_dbMutex;
constexpr int kSchemaVersion = 5;

QString resolveDbPath() {
  if (const char* env = std::getenv("ACCLOUD_DB_PATH"); env != nullptr && *env != '\0') {
    return QString::fromUtf8(env);
  }
  return accloud::app::userCacheDbPath();
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

QVariantMap printerDetailsFromColumns(const QSqlQuery& q, int firstColumn) {
  QVariantMap details;
  const QString firmwareVersion = q.value(firstColumn).toString();
  const QString printCount = q.value(firstColumn + 1).toString();
  const QString printTotalTime = q.value(firstColumn + 2).toString();
  const QString materialUsed = q.value(firstColumn + 3).toString();
  const QString releaseFilmStatus = q.value(firstColumn + 4).toString();
  const QString releaseFilmLayers = q.value(firstColumn + 5).toString();
  const int releaseFilmTimes = q.value(firstColumn + 6).toInt();
  const int releaseFilmStatusCode = q.value(firstColumn + 7).toInt();
  if (!firmwareVersion.trimmed().isEmpty()) details.insert(QStringLiteral("firmwareVersion"), firmwareVersion);
  if (!printCount.trimmed().isEmpty()) details.insert(QStringLiteral("printCount"), printCount);
  if (!printTotalTime.trimmed().isEmpty()) details.insert(QStringLiteral("printTotalTime"), printTotalTime);
  if (!materialUsed.trimmed().isEmpty()) details.insert(QStringLiteral("materialUsed"), materialUsed);
  if (!releaseFilmStatus.trimmed().isEmpty()) details.insert(QStringLiteral("releaseFilmStatus"), releaseFilmStatus);
  if (!releaseFilmLayers.trimmed().isEmpty()) details.insert(QStringLiteral("releaseFilmLayers"), releaseFilmLayers);
  if (releaseFilmTimes >= 0) details.insert(QStringLiteral("releaseFilmTimes"), releaseFilmTimes);
  if (releaseFilmStatusCode >= 0) details.insert(QStringLiteral("releaseFilmStatusCode"), releaseFilmStatusCode);
  return details;
}

bool hasMeaningfulDetailValue(const QVariant& value) {
  if (!value.isValid() || value.isNull()) {
    return false;
  }
  bool ok = false;
  const int intValue = value.toInt(&ok);
  if (ok) {
    return intValue >= 0;
  }
  const QString text = value.toString().trimmed();
  return !text.isEmpty() && text != QStringLiteral("-");
}

QVariantMap mergePrinterDetails(const QVariantMap& existing, const QVariantMap& incoming) {
  QVariantMap merged = existing;
  for (auto it = incoming.constBegin(); it != incoming.constEnd(); ++it) {
    if (hasMeaningfulDetailValue(it.value())) {
      merged.insert(it.key(), it.value());
    }
  }
  return merged;
}

QString nonNullText(const QVariantMap& map,
                    const QString& key,
                    const QString& fallback) {
  const QVariant value = map.value(key);
  if (!value.isValid() || value.isNull()) {
    return fallback;
  }
  const QString text = value.toString();
  return text.isNull() ? fallback : text;
}

QString detailString(const QVariantMap& details, const QString& key) {
  return nonNullText(details, key).trimmed();
}

int detailInt(const QVariantMap& details, const QString& key) {
  bool ok = false;
  const int value = details.value(key, -1).toInt(&ok);
  return ok ? value : -1;
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

logging::FieldMap sqlErrorFields(const QSqlError& error,
                                 logging::FieldMap fields) {
  fields.insert_or_assign("error", error.text().toStdString());
  const QString nativeCode = error.nativeErrorCode().trimmed();
  if (!nativeCode.isEmpty()) {
    fields.insert_or_assign("nativeCode", nativeCode.toStdString());
  }
  return fields;
}

int readSchemaVersion(QSqlDatabase& db) {
  QSqlQuery query(db);
  query.prepare(QStringLiteral("SELECT value FROM meta WHERE key='schema_version'"));
  if (!query.exec() || !query.next()) {
    return 0;
  }
  bool ok = false;
  const int version = query.value(0).toString().toInt(&ok);
  return ok ? version : 0;
}

bool migrateCloudFilesSchemaV4(QSqlDatabase& db) {
  const bool hasStatusCode = tableHasColumn(db, QStringLiteral("cloud_files"),
                                            QStringLiteral("status_code"));
  const bool hasThumbnailSource = tableHasColumn(db, QStringLiteral("cloud_files"),
                                                  QStringLiteral("thumbnail_source_url"));
  if (hasStatusCode && hasThumbnailSource) {
    return true;
  }

  if (!db.transaction()) {
    logging::error("app", "local_cache", "schema_v4_transaction_failed",
                   "Unable to start cache schema v4 migration",
                   sqlErrorFields(db.lastError()));
    return false;
  }

  const bool altered = ensureColumnExists(
                           db, QStringLiteral("cloud_files"),
                           QStringLiteral("status_code"),
                           QStringLiteral("status_code INTEGER NOT NULL DEFAULT 0"))
      && ensureColumnExists(
          db, QStringLiteral("cloud_files"),
          QStringLiteral("thumbnail_source_url"),
          QStringLiteral("thumbnail_source_url TEXT NOT NULL DEFAULT ''"));
  const bool verified = altered
      && tableHasColumn(db, QStringLiteral("cloud_files"), QStringLiteral("status_code"))
      && tableHasColumn(db, QStringLiteral("cloud_files"),
                        QStringLiteral("thumbnail_source_url"));

  if (!verified) {
    db.rollback();
    logging::error("app", "local_cache", "schema_v4_verification_failed",
                   "Cache schema v4 columns are missing after migration");
    return false;
  }

  if (!db.commit()) {
    logging::error("app", "local_cache", "schema_v4_commit_failed",
                   "Unable to commit cache schema v4 migration",
                   sqlErrorFields(db.lastError()));
    db.rollback();
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
      "file_id, file_name, status, status_code, size_bytes, size_text, machine, material, upload_time, print_time, "
      "layer_thickness, layers, is_pwmb, resin_usage, dimensions, thumbnail_url, thumbnail_source_url, gcode_id, updated_at"
      ") VALUES("
      ":id, :name, :status, :statusCode, :sizeBytes, :sizeText, :machine, :material, :uploadTime, :printTime, "
      ":layerThickness, :layers, :isPwmb, :resinUsage, :dimensions, :thumbnailUrl, :thumbnailSourceUrl, :gcodeId, :updatedAt"
      ")"));

  while (read.next()) {
    const QVariantMap map = decodePayloadText(read.value(0).toString());
    const QString id = nonNullText(map, QStringLiteral("fileId")).trimmed();
    if (id.isEmpty()) continue;

    const qint64 updatedAt = read.value(1).toLongLong();
    ins.bindValue(QStringLiteral(":id"), id);
    ins.bindValue(QStringLiteral(":name"), nonNullText(map, QStringLiteral("fileName")));
    ins.bindValue(QStringLiteral(":status"), nonNullText(map, QStringLiteral("status"), QStringLiteral("UNKNOWN")));
    ins.bindValue(QStringLiteral(":statusCode"), map.value(QStringLiteral("statusCode"), 0));
    ins.bindValue(QStringLiteral(":sizeBytes"), map.value(QStringLiteral("sizeBytes"), 0));
    ins.bindValue(QStringLiteral(":sizeText"), nonNullText(map, QStringLiteral("sizeText")));
    ins.bindValue(QStringLiteral(":machine"), nonNullText(map, QStringLiteral("machine")));
    ins.bindValue(QStringLiteral(":material"), nonNullText(map, QStringLiteral("material")));
    ins.bindValue(QStringLiteral(":uploadTime"), nonNullText(map, QStringLiteral("uploadTime")));
    ins.bindValue(QStringLiteral(":printTime"), nonNullText(map, QStringLiteral("printTime")));
    ins.bindValue(QStringLiteral(":layerThickness"), nonNullText(map, QStringLiteral("layerThickness")));
    ins.bindValue(QStringLiteral(":layers"), map.value(QStringLiteral("layers"), 0));
    ins.bindValue(QStringLiteral(":isPwmb"), map.value(QStringLiteral("isPwmb"), false).toBool() ? 1 : 0);
    ins.bindValue(QStringLiteral(":resinUsage"), nonNullText(map, QStringLiteral("resinUsage")));
    ins.bindValue(QStringLiteral(":dimensions"), nonNullText(map, QStringLiteral("dimensions")));
    ins.bindValue(QStringLiteral(":thumbnailUrl"), nonNullText(map, QStringLiteral("thumbnailUrl")));
    ins.bindValue(QStringLiteral(":thumbnailSourceUrl"), nonNullText(map, QStringLiteral("thumbnailSourceUrl")));
    ins.bindValue(QStringLiteral(":gcodeId"), nonNullText(map, QStringLiteral("gcodeId")));
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
    const QString id = nonNullText(map, QStringLiteral("id")).trimmed();
    if (id.isEmpty()) continue;

    const qint64 updatedAt = read.value(1).toLongLong();
    const QString printerKey = nonNullText(map, QStringLiteral("printerKey")).trimmed().isEmpty()
        ? nonNullText(map, QStringLiteral("key")).trimmed()
        : nonNullText(map, QStringLiteral("printerKey")).trimmed();
    ins.bindValue(QStringLiteral(":id"), id);
    ins.bindValue(QStringLiteral(":printerKey"), printerKey);
    ins.bindValue(QStringLiteral(":machineType"), nonNullText(map, QStringLiteral("machineType")));
    ins.bindValue(QStringLiteral(":name"), nonNullText(map, QStringLiteral("name")));
    ins.bindValue(QStringLiteral(":model"), nonNullText(map, QStringLiteral("model")));
    ins.bindValue(QStringLiteral(":type"), nonNullText(map, QStringLiteral("type")));
    ins.bindValue(QStringLiteral(":lastSeen"), nonNullText(map, QStringLiteral("lastSeen")));
    ins.bindValue(QStringLiteral(":state"), nonNullText(map, QStringLiteral("state"), QStringLiteral("UNKNOWN")));
    ins.bindValue(QStringLiteral(":reason"), nonNullText(map, QStringLiteral("reason")));
    ins.bindValue(QStringLiteral(":available"), map.value(QStringLiteral("available"), -1));
    ins.bindValue(QStringLiteral(":currentFile"), nonNullText(map, QStringLiteral("currentFile")));
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
                     "  status_code INTEGER NOT NULL DEFAULT 0,"
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
                     "  thumbnail_source_url TEXT NOT NULL DEFAULT '',"
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
                     "  firmware_version TEXT NOT NULL DEFAULT '',"
                     "  print_count TEXT NOT NULL DEFAULT '',"
                     "  print_total_time TEXT NOT NULL DEFAULT '',"
                     "  material_used TEXT NOT NULL DEFAULT '',"
                     "  release_film_status TEXT NOT NULL DEFAULT '',"
                     "  release_film_layers TEXT NOT NULL DEFAULT '',"
                     "  release_film_times INTEGER NOT NULL DEFAULT -1,"
                     "  release_film_status_code INTEGER NOT NULL DEFAULT -1,"
                     "  updated_at INTEGER NOT NULL"
                     ")"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS jobs ("
                     "  task_id TEXT PRIMARY KEY,"
                     "  printer_id TEXT NOT NULL DEFAULT '',"
                     "  cloud_file_id TEXT NOT NULL DEFAULT '',"
                     "  gcode_id TEXT NOT NULL DEFAULT '',"
                     "  printer_name TEXT NOT NULL DEFAULT '',"
                     "  gcode_name TEXT NOT NULL DEFAULT '',"
                     "  print_status INTEGER NOT NULL DEFAULT 0,"
                     "  progress INTEGER NOT NULL DEFAULT -1,"
                     "  elapsed_sec INTEGER NOT NULL DEFAULT -1,"
                     "  remaining_sec INTEGER NOT NULL DEFAULT -1,"
                     "  current_layer INTEGER NOT NULL DEFAULT -1,"
                     "  total_layers INTEGER NOT NULL DEFAULT -1,"
                     "  current_file TEXT NOT NULL DEFAULT '',"
                     "  reason TEXT NOT NULL DEFAULT '',"
                     "  create_time INTEGER NOT NULL DEFAULT 0,"
                     "  end_time INTEGER NOT NULL DEFAULT 0,"
                     "  img TEXT NOT NULL DEFAULT '',"
                     "  updated_at INTEGER NOT NULL"
                     ")"),
      QStringLiteral("CREATE TABLE IF NOT EXISTS pending_direct_prints ("
                     "  printer_id TEXT PRIMARY KEY,"
                     "  cloud_file_id TEXT NOT NULL DEFAULT '',"
                     "  cloud_gcode_id TEXT NOT NULL DEFAULT '',"
                     "  cloud_file_name TEXT NOT NULL DEFAULT '',"
                     "  cloud_file_size INTEGER NOT NULL DEFAULT 0,"
                     "  print_task_id TEXT NOT NULL DEFAULT '',"
                     "  print_msg_id TEXT NOT NULL DEFAULT '',"
                     "  printer_local_filename TEXT NOT NULL DEFAULT '',"
                     "  printer_local_path TEXT NOT NULL DEFAULT '/',"
                     "  delete_after_success INTEGER NOT NULL DEFAULT 0,"
                     "  delete_local_on_failure INTEGER NOT NULL DEFAULT 0,"
                     "  observed_active INTEGER NOT NULL DEFAULT 0,"
                     "  state TEXT NOT NULL DEFAULT 'UPLOADED',"
                     "  created_at INTEGER NOT NULL DEFAULT 0,"
                     "  updated_at INTEGER NOT NULL DEFAULT 0"
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

  const int previousSchemaVersion = readSchemaVersion(db);
  if (!migrateCloudFilesSchemaV4(db)
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("printer_key"),
                             QStringLiteral("printer_key TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("machine_type"),
                             QStringLiteral("machine_type TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("firmware_version"),
                             QStringLiteral("firmware_version TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("print_count"),
                             QStringLiteral("print_count TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("print_total_time"),
                             QStringLiteral("print_total_time TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("material_used"),
                             QStringLiteral("material_used TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("release_film_status"),
                             QStringLiteral("release_film_status TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("release_film_layers"),
                             QStringLiteral("release_film_layers TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("release_film_times"),
                             QStringLiteral("release_film_times INTEGER NOT NULL DEFAULT -1"))
      || !ensureColumnExists(db, QStringLiteral("cloud_printers"),
                             QStringLiteral("release_film_status_code"),
                             QStringLiteral("release_film_status_code INTEGER NOT NULL DEFAULT -1"))
      || !ensureColumnExists(db, QStringLiteral("jobs"),
                             QStringLiteral("cloud_file_id"),
                             QStringLiteral("cloud_file_id TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("jobs"),
                             QStringLiteral("gcode_id"),
                             QStringLiteral("gcode_id TEXT NOT NULL DEFAULT ''"))
      || !ensureColumnExists(db, QStringLiteral("jobs"),
                             QStringLiteral("progress"),
                             QStringLiteral("progress INTEGER NOT NULL DEFAULT -1"))
      || !ensureColumnExists(db, QStringLiteral("jobs"),
                             QStringLiteral("elapsed_sec"),
                             QStringLiteral("elapsed_sec INTEGER NOT NULL DEFAULT -1"))
      || !ensureColumnExists(db, QStringLiteral("jobs"),
                             QStringLiteral("remaining_sec"),
                             QStringLiteral("remaining_sec INTEGER NOT NULL DEFAULT -1"))
      || !ensureColumnExists(db, QStringLiteral("jobs"),
                             QStringLiteral("current_layer"),
                             QStringLiteral("current_layer INTEGER NOT NULL DEFAULT -1"))
      || !ensureColumnExists(db, QStringLiteral("jobs"),
                             QStringLiteral("total_layers"),
                             QStringLiteral("total_layers INTEGER NOT NULL DEFAULT -1"))
      || !ensureColumnExists(db, QStringLiteral("jobs"),
                             QStringLiteral("current_file"),
                             QStringLiteral("current_file TEXT NOT NULL DEFAULT ''"))) {
    return false;
  }

  if (!db.transaction()) {
    logging::error("app", "local_cache", "legacy_migration_transaction_failed",
                   "Unable to start legacy cache migration",
                   sqlErrorFields(db.lastError()));
    return false;
  }

  const bool migrated = migrateLegacyFiles(db) && migrateLegacyPrinters(db);
  if (!migrated) {
    db.rollback();
    logging::warn("app", "local_cache", "legacy_migration_failed",
                  "Legacy payload migration failed, keeping typed tables as-is");
  } else if (!db.commit()) {
    logging::error("app", "local_cache", "legacy_migration_commit_failed",
                   "Unable to commit legacy cache migration",
                   sqlErrorFields(db.lastError()));
    db.rollback();
    return false;
  }

  QSqlQuery setVersion(db);
  if (!setVersion.prepare(QStringLiteral(
          "INSERT INTO meta(key, value) VALUES('schema_version', :version) "
          "ON CONFLICT(key) DO UPDATE SET value=excluded.value"))) {
    logging::error("app", "local_cache", "schema_version_prepare_failed",
                   "Unable to prepare cache schema version update",
                   sqlErrorFields(setVersion.lastError()));
    return false;
  }
  setVersion.bindValue(QStringLiteral(":version"), kSchemaVersion);
  if (!setVersion.exec()) {
    logging::error("app", "local_cache", "schema_version_write_failed",
                   "Unable to write cache schema version",
                   sqlErrorFields(setVersion.lastError(),
                                  {{"targetVersion", std::to_string(kSchemaVersion)},
                                   {"previousVersion", std::to_string(previousSchemaVersion)}}));
    return false;
  }
  return true;
}

} // namespace accloud::cache
