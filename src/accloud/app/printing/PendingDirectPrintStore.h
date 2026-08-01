#pragma once

#include "domain/printing/DirectPrintOperation.h"

#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace accloud {

class LocalCacheStore;

class PendingDirectPrintStore {
 public:
  explicit PendingDirectPrintStore(LocalCacheStore& cacheStore);

  [[nodiscard]] std::vector<printing::DirectPrintOperation> loadAll() const;
  [[nodiscard]] bool save(const printing::DirectPrintOperation& operation) const;
  [[nodiscard]] bool remove(const std::string& printerId) const;

  [[nodiscard]] static printing::DirectPrintOperation fromVariantMap(const QVariantMap& map);
  [[nodiscard]] static QVariantMap toVariantMap(const printing::DirectPrintOperation& operation);

 private:
  LocalCacheStore& m_cacheStore;
};

} // namespace accloud
