#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace accloud {

class LocalCacheStore {
 public:
  struct SyncState {
    bool hasSuccess = false;
    qint64 lastSuccessAt = 0;
    qint64 lastAttemptAt = 0;
    bool lastStatusOk = false;
    QString lastError;
  };

  LocalCacheStore();

  [[nodiscard]] bool isAvailable() const;
  [[nodiscard]] QString databasePath() const;

  [[nodiscard]] QVariantList loadFiles(int page, int limit) const;
  [[nodiscard]] QVariantList loadPrinters() const;
  [[nodiscard]] QVariantMap loadQuota() const;

  bool replaceFiles(const QVariantList& files) const;
  bool replacePrinters(const QVariantList& printers) const;
  bool saveQuota(const QVariantMap& quota) const;

  void removeFile(const QString& fileId) const;
  void updateSyncState(const QString& scope, bool ok, const QString& errorMessage) const;
  [[nodiscard]] std::optional<SyncState> syncState(const QString& scope) const;
  void invalidateScope(const QString& scope) const;

 private:
  [[nodiscard]] bool ensureReady() const;
  [[nodiscard]] bool migrate() const;

  [[nodiscard]] QVariantMap parsePayload(const QString& payload) const;
  [[nodiscard]] QString encodePayload(const QVariantMap& map) const;

  void cleanupRetention() const;
  qint64 nowEpochSec() const;

  QString m_dbPath;
  mutable bool m_ready = false;
};

} // namespace accloud
