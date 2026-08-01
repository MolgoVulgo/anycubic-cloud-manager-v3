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
        "SELECT task_id, cloud_file_id, gcode_id, gcode_name, printer_id, printer_name, print_status, progress, elapsed_sec, remaining_sec, "
        "current_layer, total_layers, current_file, reason, create_time, end_time, img "
        "FROM jobs WHERE printer_id = :printerId ORDER BY create_time DESC, updated_at DESC LIMIT :limit OFFSET :offset"));
    q.bindValue(QStringLiteral(":printerId"), normalizedPrinterId);
    q.bindValue(QStringLiteral(":limit"), limit);
    q.bindValue(QStringLiteral(":offset"), offset);
    if (q.exec()) {
      while (q.next()) {
        QVariantMap item;
        item.insert("taskId", q.value(0).toString());
        item.insert("cloudFileId", q.value(1).toString());
        item.insert("gcodeId", q.value(2).toString());
        item.insert("gcodeName", q.value(3).toString());
        item.insert("printerId", q.value(4).toString());
        item.insert("printerName", q.value(5).toString());
        item.insert("printStatus", q.value(6).toInt());
        item.insert("progress", q.value(7).toInt());
        item.insert("elapsedSec", q.value(8).toInt());
        item.insert("remainingSec", q.value(9).toInt());
        item.insert("currentLayer", q.value(10).toInt());
        item.insert("totalLayers", q.value(11).toInt());
        item.insert("currentFile", q.value(12).toString());
        item.insert("reason", q.value(13).toString());
        item.insert("createTime", q.value(14).toLongLong());
        item.insert("endTime", q.value(15).toLongLong());
        item.insert("img", q.value(16).toString());
        out.append(item);
      }
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return out;
}

QVariantMap LocalCacheStore::loadRecentJobsForPrinters(const QStringList& printerIds, int limitPerPrinter) const {
  QVariantMap out;
  if (!ensureReady()) {
    return out;
  }

  QStringList ids;
  ids.reserve(printerIds.size());
  for (const QString& printerId : printerIds) {
    const QString normalized = printerId.trimmed();
    if (!normalized.isEmpty() && !ids.contains(normalized)) {
      ids.push_back(normalized);
    }
  }
  if (ids.isEmpty()) {
    return out;
  }
  if (limitPerPrinter <= 0) {
    limitPerPrinter = 20;
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

    QStringList placeholders;
    placeholders.reserve(ids.size());
    for (int i = 0; i < ids.size(); ++i) {
      placeholders.push_back(QStringLiteral(":printer%1").arg(i));
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT task_id, gcode_name, printer_id, printer_name, print_status, progress, elapsed_sec, remaining_sec, "
        "current_layer, total_layers, current_file, reason, create_time, end_time, img "
        "FROM jobs WHERE printer_id IN (%1) ORDER BY printer_id ASC, create_time DESC, updated_at DESC")
                  .arg(placeholders.join(QStringLiteral(","))));
    for (int i = 0; i < ids.size(); ++i) {
      q.bindValue(placeholders.at(i), ids.at(i));
    }

    QHash<QString, int> counts;
    if (q.exec()) {
      while (q.next()) {
        const QString printerId = q.value(2).toString();
        const int count = counts.value(printerId, 0);
        if (count >= limitPerPrinter) {
          continue;
        }
        counts.insert(printerId, count + 1);

        QVariantMap item;
        item.insert("taskId", q.value(0).toString());
        item.insert("gcodeName", q.value(1).toString());
        item.insert("printerId", printerId);
        item.insert("printerName", q.value(3).toString());
        item.insert("printStatus", q.value(4).toInt());
        item.insert("progress", q.value(5).toInt());
        item.insert("elapsedSec", q.value(6).toInt());
        item.insert("remainingSec", q.value(7).toInt());
        item.insert("currentLayer", q.value(8).toInt());
        item.insert("totalLayers", q.value(9).toInt());
        item.insert("currentFile", q.value(10).toString());
        item.insert("reason", q.value(11).toString());
        item.insert("createTime", q.value(12).toLongLong());
        item.insert("endTime", q.value(13).toLongLong());
        item.insert("img", q.value(14).toString());

        QVariantList jobs = out.value(printerId).toList();
        jobs.append(item);
        out.insert(printerId, jobs);
      }
    }

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return out;
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
          "task_id, printer_id, cloud_file_id, gcode_id, printer_name, gcode_name, print_status, progress, elapsed_sec, remaining_sec, "
          "current_layer, total_layers, current_file, reason, create_time, end_time, img, updated_at"
          ") VALUES("
          ":taskId, :printerId, :cloudFileId, :gcodeId, :printerName, :gcodeName, :printStatus, :progress, :elapsedSec, :remainingSec, "
          ":currentLayer, :totalLayers, :currentFile, :reason, :createTime, :endTime, :img, :updatedAt"
          ")"));
      for (const QVariant& item : jobs) {
        const QVariantMap map = item.toMap();
        const QString taskId = nonNullText(map, QStringLiteral("taskId")).trimmed();
        const QString printerId = nonNullText(map, QStringLiteral("printerId")).trimmed();
        if (taskId.isEmpty() || printerId.isEmpty()) continue;

        ins.bindValue(QStringLiteral(":taskId"), taskId);
        ins.bindValue(QStringLiteral(":printerId"), printerId);
        ins.bindValue(QStringLiteral(":cloudFileId"), nonNullText(map, QStringLiteral("cloudFileId")));
        ins.bindValue(QStringLiteral(":gcodeId"), nonNullText(map, QStringLiteral("gcodeId")));
        ins.bindValue(QStringLiteral(":printerName"), nonNullText(map, QStringLiteral("printerName")));
        ins.bindValue(QStringLiteral(":gcodeName"), nonNullText(map, QStringLiteral("gcodeName")));
        ins.bindValue(QStringLiteral(":printStatus"), map.value(QStringLiteral("printStatus"), 0));
        ins.bindValue(QStringLiteral(":progress"), map.value(QStringLiteral("progress"), -1));
        ins.bindValue(QStringLiteral(":elapsedSec"), map.value(QStringLiteral("elapsedSec"), -1));
        ins.bindValue(QStringLiteral(":remainingSec"), map.value(QStringLiteral("remainingSec"), -1));
        ins.bindValue(QStringLiteral(":currentLayer"), map.value(QStringLiteral("currentLayer"), -1));
        ins.bindValue(QStringLiteral(":totalLayers"), map.value(QStringLiteral("totalLayers"), -1));
        ins.bindValue(QStringLiteral(":currentFile"), nonNullText(map, QStringLiteral("currentFile")));
        ins.bindValue(QStringLiteral(":reason"), nonNullText(map, QStringLiteral("reason")));
        ins.bindValue(QStringLiteral(":createTime"), map.value(QStringLiteral("createTime"), 0));
        ins.bindValue(QStringLiteral(":endTime"), map.value(QStringLiteral("endTime"), 0));
        ins.bindValue(QStringLiteral(":img"), nonNullText(map, QStringLiteral("img")));
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
          "task_id, printer_id, cloud_file_id, gcode_id, printer_name, gcode_name, print_status, progress, elapsed_sec, remaining_sec, "
          "current_layer, total_layers, current_file, reason, create_time, end_time, img, updated_at"
          ") VALUES("
          ":taskId, :printerId, :cloudFileId, :gcodeId, :printerName, :gcodeName, :printStatus, :progress, :elapsedSec, :remainingSec, "
          ":currentLayer, :totalLayers, :currentFile, :reason, :createTime, :endTime, :img, :updatedAt"
          ")"));
      for (const QVariant& item : jobs) {
        const QVariantMap map = item.toMap();
        const QString taskId = nonNullText(map, QStringLiteral("taskId")).trimmed();
        if (taskId.isEmpty()) continue;

        ins.bindValue(QStringLiteral(":taskId"), taskId);
        ins.bindValue(QStringLiteral(":printerId"), normalizedPrinterId);
        ins.bindValue(QStringLiteral(":cloudFileId"), nonNullText(map, QStringLiteral("cloudFileId")));
        ins.bindValue(QStringLiteral(":gcodeId"), nonNullText(map, QStringLiteral("gcodeId")));
        ins.bindValue(QStringLiteral(":printerName"), nonNullText(map, QStringLiteral("printerName")));
        ins.bindValue(QStringLiteral(":gcodeName"), nonNullText(map, QStringLiteral("gcodeName")));
        ins.bindValue(QStringLiteral(":printStatus"), map.value(QStringLiteral("printStatus"), 0));
        ins.bindValue(QStringLiteral(":progress"), map.value(QStringLiteral("progress"), -1));
        ins.bindValue(QStringLiteral(":elapsedSec"), map.value(QStringLiteral("elapsedSec"), -1));
        ins.bindValue(QStringLiteral(":remainingSec"), map.value(QStringLiteral("remainingSec"), -1));
        ins.bindValue(QStringLiteral(":currentLayer"), map.value(QStringLiteral("currentLayer"), -1));
        ins.bindValue(QStringLiteral(":totalLayers"), map.value(QStringLiteral("totalLayers"), -1));
        ins.bindValue(QStringLiteral(":currentFile"), nonNullText(map, QStringLiteral("currentFile")));
        ins.bindValue(QStringLiteral(":reason"), nonNullText(map, QStringLiteral("reason")));
        ins.bindValue(QStringLiteral(":createTime"), map.value(QStringLiteral("createTime"), 0));
        ins.bindValue(QStringLiteral(":endTime"), map.value(QStringLiteral("endTime"), 0));
        ins.bindValue(QStringLiteral(":img"), nonNullText(map, QStringLiteral("img")));
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

bool LocalCacheStore::upsertJobsForPrinter(const QString& printerId, const QVariantList& jobs) const {
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

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO jobs("
        "task_id, printer_id, cloud_file_id, gcode_id, printer_name, gcode_name, print_status, progress, elapsed_sec, remaining_sec, "
        "current_layer, total_layers, current_file, reason, create_time, end_time, img, updated_at"
        ") VALUES("
        ":taskId, :printerId, :cloudFileId, :gcodeId, :printerName, :gcodeName, :printStatus, :progress, :elapsedSec, :remainingSec, "
        ":currentLayer, :totalLayers, :currentFile, :reason, :createTime, :endTime, :img, :updatedAt"
        ") ON CONFLICT(task_id) DO UPDATE SET "
        "  printer_id = excluded.printer_id,"
        "  cloud_file_id = excluded.cloud_file_id,"
        "  gcode_id = excluded.gcode_id,"
        "  printer_name = excluded.printer_name,"
        "  gcode_name = excluded.gcode_name,"
        "  print_status = excluded.print_status,"
        "  progress = excluded.progress,"
        "  elapsed_sec = excluded.elapsed_sec,"
        "  remaining_sec = excluded.remaining_sec,"
        "  current_layer = excluded.current_layer,"
        "  total_layers = excluded.total_layers,"
        "  current_file = excluded.current_file,"
        "  reason = excluded.reason,"
        "  create_time = excluded.create_time,"
        "  end_time = excluded.end_time,"
        "  img = excluded.img,"
        "  updated_at = excluded.updated_at"));

    ok = true;
    for (const QVariant& item : jobs) {
      const QVariantMap map = item.toMap();
      const QString taskId = nonNullText(map, QStringLiteral("taskId")).trimmed();
      if (taskId.isEmpty()) continue;

      ins.bindValue(QStringLiteral(":taskId"), taskId);
      ins.bindValue(QStringLiteral(":printerId"), normalizedPrinterId);
      ins.bindValue(QStringLiteral(":cloudFileId"), nonNullText(map, QStringLiteral("cloudFileId")));
      ins.bindValue(QStringLiteral(":gcodeId"), nonNullText(map, QStringLiteral("gcodeId")));
      ins.bindValue(QStringLiteral(":printerName"), nonNullText(map, QStringLiteral("printerName")));
      ins.bindValue(QStringLiteral(":gcodeName"), nonNullText(map, QStringLiteral("gcodeName")));
      ins.bindValue(QStringLiteral(":printStatus"), map.value(QStringLiteral("printStatus"), 0));
      ins.bindValue(QStringLiteral(":progress"), map.value(QStringLiteral("progress"), -1));
      ins.bindValue(QStringLiteral(":elapsedSec"), map.value(QStringLiteral("elapsedSec"), -1));
      ins.bindValue(QStringLiteral(":remainingSec"), map.value(QStringLiteral("remainingSec"), -1));
      ins.bindValue(QStringLiteral(":currentLayer"), map.value(QStringLiteral("currentLayer"), -1));
      ins.bindValue(QStringLiteral(":totalLayers"), map.value(QStringLiteral("totalLayers"), -1));
      ins.bindValue(QStringLiteral(":currentFile"), nonNullText(map, QStringLiteral("currentFile")));
      ins.bindValue(QStringLiteral(":reason"), nonNullText(map, QStringLiteral("reason")));
      ins.bindValue(QStringLiteral(":createTime"), map.value(QStringLiteral("createTime"), 0));
      ins.bindValue(QStringLiteral(":endTime"), map.value(QStringLiteral("endTime"), 0));
      ins.bindValue(QStringLiteral(":img"), nonNullText(map, QStringLiteral("img")));
      ins.bindValue(QStringLiteral(":updatedAt"), now);
      if (!ins.exec()) {
        ok = false;
        break;
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

} // namespace accloud
