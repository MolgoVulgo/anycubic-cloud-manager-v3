#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace accloud::usecases::printing {

class PrepareRemotePrintUseCase final {
 public:
  [[nodiscard]] QVariantMap execute(const QString& mode,
                                    const QString& fileId,
                                    const QString& fileName,
                                    const QVariantMap& fileData,
                                    const QVariantList& printers,
                                    const QString& preferredPrinterId,
                                    const QVariantMap& serverCompatibility,
                                    bool compatibilityChecked,
                                    bool compatibilityFailed) const;

  [[nodiscard]] QVariantMap evaluateGuard(const QString& mode,
                                          const QVariantMap& printer,
                                          const QVariantMap& fileData) const;
};

} // namespace accloud::usecases::printing
