#pragma once

#include <QString>
#include <QVariantMap>

namespace accloud::printing {

struct PrinterFileCompatibilityResult {
  bool ok{false};
  int score{0};
  QString reason;
  QString reasonKey;
};

[[nodiscard]] QString cloudSliceFileExtension(const QString& fileName);
[[nodiscard]] bool isKnownCloudSliceExtension(const QString& ext);
[[nodiscard]] bool fileHasLocalCompatibilityMetadata(const QVariantMap& file);
[[nodiscard]] PrinterFileCompatibilityResult evaluatePrinterFileCompatibility(
    const QVariantMap& printer,
    const QVariantMap& file);
[[nodiscard]] QVariantMap printerFileCompatibilityToVariantMap(
    const PrinterFileCompatibilityResult& result);

} // namespace accloud::printing
