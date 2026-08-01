#include "app/LocalCacheStore.h"
#include "app/cache/LocalCacheSql.h"

#include "infra/logging/JsonlLogger.h"

#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <string>

#include <QDir>
#include <QFileInfo>

namespace accloud {
using namespace cache;

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

} // namespace accloud
