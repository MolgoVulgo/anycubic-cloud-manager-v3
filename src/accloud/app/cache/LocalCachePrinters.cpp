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
        "SELECT printer_id, printer_key, machine_type, name, model, type, last_seen, state, reason, available, current_file, "
        "firmware_version, print_count, print_total_time, material_used, release_film_status, release_film_layers, "
        "release_film_times, release_film_status_code "
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
        item.insert("details", printerDetailsFromColumns(q, 11));
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

    QHash<QString, QVariantMap> existingDetails;
    QSqlQuery existing(db);
    if (existing.exec(QStringLiteral(
            "SELECT printer_id, firmware_version, print_count, print_total_time, material_used, "
            "release_film_status, release_film_layers, release_film_times, release_film_status_code "
            "FROM cloud_printers"))) {
      while (existing.next()) {
        existingDetails.insert(existing.value(0).toString(), printerDetailsFromColumns(existing, 1));
      }
    }

    QSqlQuery clear(db);
    ok = clear.exec(QStringLiteral("DELETE FROM cloud_printers"));

    if (ok) {
      QSqlQuery ins(db);
      ins.prepare(QStringLiteral(
          "INSERT INTO cloud_printers("
          "printer_id, printer_key, machine_type, name, model, type, last_seen, state, reason, available, current_file, "
          "firmware_version, print_count, print_total_time, material_used, release_film_status, release_film_layers, "
          "release_film_times, release_film_status_code, updated_at"
          ") VALUES("
          ":id, :printerKey, :machineType, :name, :model, :type, :lastSeen, :state, :reason, :available, :currentFile, "
          ":firmwareVersion, :printCount, :printTotalTime, :materialUsed, :releaseFilmStatus, :releaseFilmLayers, "
          ":releaseFilmTimes, :releaseFilmStatusCode, :updatedAt"
          ")"));
      for (const QVariant& item : printers) {
        const QVariantMap map = item.toMap();
        const QString id = nonNullText(map, QStringLiteral("id")).trimmed();
        if (id.isEmpty()) continue;
        const QVariantMap incomingDetails = map.value(QStringLiteral("details")).toMap();
        QVariantMap details = existingDetails.contains(id)
            ? mergePrinterDetails(existingDetails.value(id), incomingDetails)
            : incomingDetails;

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
        ins.bindValue(QStringLiteral(":firmwareVersion"), detailString(details, QStringLiteral("firmwareVersion")));
        ins.bindValue(QStringLiteral(":printCount"), detailString(details, QStringLiteral("printCount")));
        ins.bindValue(QStringLiteral(":printTotalTime"), detailString(details, QStringLiteral("printTotalTime")));
        ins.bindValue(QStringLiteral(":materialUsed"), detailString(details, QStringLiteral("materialUsed")));
        ins.bindValue(QStringLiteral(":releaseFilmStatus"), detailString(details, QStringLiteral("releaseFilmStatus")));
        ins.bindValue(QStringLiteral(":releaseFilmLayers"), detailString(details, QStringLiteral("releaseFilmLayers")));
        ins.bindValue(QStringLiteral(":releaseFilmTimes"), detailInt(details, QStringLiteral("releaseFilmTimes")));
        ins.bindValue(QStringLiteral(":releaseFilmStatusCode"), detailInt(details, QStringLiteral("releaseFilmStatusCode")));
        ins.bindValue(QStringLiteral(":updatedAt"), now);
        if (!ins.exec()) {
          logging::error("app", "local_cache", "printer_cache_insert_failed",
                         "Unable to insert printer into local cache",
                         {{"printerId", id.toStdString()},
                          {"error", ins.lastError().text().toStdString()}});
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

bool LocalCacheStore::savePrinterDetails(const QString& printerId, const QVariantMap& details) const {
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

    QVariantMap mergedDetails = details;
    QSqlQuery existing(db);
    existing.prepare(QStringLiteral(
        "SELECT firmware_version, print_count, print_total_time, material_used, "
        "release_film_status, release_film_layers, release_film_times, release_film_status_code "
        "FROM cloud_printers WHERE printer_id=:printerId"));
    existing.bindValue(QStringLiteral(":printerId"), normalizedPrinterId);
    if (existing.exec() && existing.next()) {
      mergedDetails = mergePrinterDetails(printerDetailsFromColumns(existing, 0), details);
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO cloud_printers("
        "printer_id, firmware_version, print_count, print_total_time, material_used, release_film_status, "
        "release_film_layers, release_film_times, release_film_status_code, updated_at"
        ") VALUES("
        ":printerId, :firmwareVersion, :printCount, :printTotalTime, :materialUsed, :releaseFilmStatus, "
        ":releaseFilmLayers, :releaseFilmTimes, :releaseFilmStatusCode, :updatedAt"
        ") ON CONFLICT(printer_id) DO UPDATE SET "
        "firmware_version=excluded.firmware_version,"
        "print_count=excluded.print_count,"
        "print_total_time=excluded.print_total_time,"
        "material_used=excluded.material_used,"
        "release_film_status=excluded.release_film_status,"
        "release_film_layers=excluded.release_film_layers,"
        "release_film_times=excluded.release_film_times,"
        "release_film_status_code=excluded.release_film_status_code,"
        "updated_at=excluded.updated_at"));
    q.bindValue(QStringLiteral(":printerId"), normalizedPrinterId);
    q.bindValue(QStringLiteral(":firmwareVersion"), detailString(mergedDetails, QStringLiteral("firmwareVersion")));
    q.bindValue(QStringLiteral(":printCount"), detailString(mergedDetails, QStringLiteral("printCount")));
    q.bindValue(QStringLiteral(":printTotalTime"), detailString(mergedDetails, QStringLiteral("printTotalTime")));
    q.bindValue(QStringLiteral(":materialUsed"), detailString(mergedDetails, QStringLiteral("materialUsed")));
    q.bindValue(QStringLiteral(":releaseFilmStatus"), detailString(mergedDetails, QStringLiteral("releaseFilmStatus")));
    q.bindValue(QStringLiteral(":releaseFilmLayers"), detailString(mergedDetails, QStringLiteral("releaseFilmLayers")));
    q.bindValue(QStringLiteral(":releaseFilmTimes"), detailInt(mergedDetails, QStringLiteral("releaseFilmTimes")));
    q.bindValue(QStringLiteral(":releaseFilmStatusCode"), detailInt(mergedDetails, QStringLiteral("releaseFilmStatusCode")));
    q.bindValue(QStringLiteral(":updatedAt"), nowEpochSec());
    ok = q.exec();

    db.close();
  }
  QSqlDatabase::removeDatabase(connectionName);
  return ok;
}

} // namespace accloud
