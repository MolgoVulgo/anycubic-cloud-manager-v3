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
        "SELECT file_id, file_name, status, status_code, size_bytes, size_text, machine, material, upload_time, print_time, "
        "layer_thickness, layers, is_pwmb, resin_usage, dimensions, thumbnail_url, thumbnail_source_url, gcode_id "
        "FROM cloud_files ORDER BY updated_at DESC LIMIT :limit OFFSET :offset"));
    q.bindValue(QStringLiteral(":limit"), limit);
    q.bindValue(QStringLiteral(":offset"), offset);
    if (q.exec()) {
      while (q.next()) {
        QVariantMap item;
        item.insert("fileId", q.value(0).toString());
        item.insert("fileName", q.value(1).toString());
        item.insert("status", q.value(2).toString());
        item.insert("statusCode", q.value(3).toInt());
        item.insert("sizeBytes", q.value(4).toULongLong());
        item.insert("sizeText", q.value(5).toString());
        item.insert("machine", q.value(6).toString());
        item.insert("material", q.value(7).toString());
        item.insert("uploadTime", q.value(8).toString());
        item.insert("printTime", q.value(9).toString());
        item.insert("layerThickness", q.value(10).toString());
        item.insert("layers", q.value(11).toInt());
        item.insert("isPwmb", q.value(12).toInt() == 1);
        item.insert("resinUsage", q.value(13).toString());
        item.insert("dimensions", q.value(14).toString());
        item.insert("thumbnailUrl", q.value(15).toString());
        item.insert("thumbnailSourceUrl", q.value(16).toString());
        item.insert("gcodeId", q.value(17).toString());
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
      logging::error("app", "local_cache", "file_cache_open_failed",
                     "Unable to open local cache for file replacement",
                     sqlErrorFields(db.lastError()));
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    const qint64 now = nowEpochSec();
    if (!db.transaction()) {
      logging::error("app", "local_cache", "file_cache_transaction_failed",
                     "Unable to start file cache replacement transaction",
                     sqlErrorFields(db.lastError()));
      closeAndRemoveDatabase(db, connectionName);
      return false;
    }

    {
      QSqlQuery clear(db);
      if (!clear.exec(QStringLiteral("DELETE FROM cloud_files"))) {
        logging::error("app", "local_cache", "file_cache_clear_failed",
                       "Unable to clear cached cloud files",
                       sqlErrorFields(clear.lastError()));
        ok = false;
      } else {
        clear.finish();
        ok = true;
      }

      if (ok) {
        QSqlQuery ins(db);
        const QString insertSql = QStringLiteral(
            "INSERT INTO cloud_files("
            "file_id, file_name, status, status_code, size_bytes, size_text, machine, material, upload_time, print_time, "
            "layer_thickness, layers, is_pwmb, resin_usage, dimensions, thumbnail_url, thumbnail_source_url, gcode_id, updated_at"
            ") VALUES("
            ":id, :name, :status, :statusCode, :sizeBytes, :sizeText, :machine, :material, :uploadTime, :printTime, "
            ":layerThickness, :layers, :isPwmb, :resinUsage, :dimensions, :thumbnailUrl, :thumbnailSourceUrl, :gcodeId, :updatedAt"
            ")");
        if (!ins.prepare(insertSql)) {
          logging::error("app", "local_cache", "file_cache_prepare_failed",
                         "Unable to prepare cloud file cache insertion",
                         sqlErrorFields(ins.lastError()));
          ok = false;
        }

        if (ok) {
          for (const QVariant& item : files) {
            const QVariantMap map = item.toMap();
            const QString id = nonNullText(map, QStringLiteral("fileId")).trimmed();
            if (id.isEmpty()) continue;

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
            ins.bindValue(QStringLiteral(":updatedAt"), now);
            if (!ins.exec()) {
              logging::error("app", "local_cache", "file_cache_insert_failed",
                             "Unable to insert cloud file into local cache",
                             sqlErrorFields(ins.lastError(), {{"fileId", id.toStdString()}}));
              ok = false;
              break;
            }
          }
        }
        ins.finish();
      }
    }

    if (ok) {
      enforceMaxRows(db, QStringLiteral("cloud_files"), QStringLiteral("file_id"), 1500);
      if (!db.commit()) {
        logging::error("app", "local_cache", "file_cache_commit_failed",
                       "Unable to commit cloud file cache replacement",
                       sqlErrorFields(db.lastError()));
        ok = false;
        db.rollback();
      }
    } else if (!db.rollback()) {
      logging::warn("app", "local_cache", "file_cache_rollback_failed",
                    "Unable to rollback cloud file cache replacement",
                    sqlErrorFields(db.lastError()));
    }

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

} // namespace accloud
