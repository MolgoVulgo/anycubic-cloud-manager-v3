#include "PrepareRemotePrintUseCase.h"

#include "app/printing/PrinterFileCompatibility.h"

#include <QSet>

#include <utility>

namespace accloud::usecases::printing {
namespace {

bool compatibilityEntryAllows(const QVariantMap& entry) {
  bool ok = false;
  const int available = entry.value(QStringLiteral("available"), 1).toInt(&ok);
  return !ok || available > 0;
}

QString normalizedCompatibilityReason(const QString& raw) {
  QString reason = raw.trimmed();
  const QString prefix = QStringLiteral("unavailable reason:");
  if (reason.startsWith(prefix, Qt::CaseInsensitive)) {
    reason = reason.mid(prefix.size()).trimmed();
  }
  return reason;
}

QString firstServerBlockReason(const QVariantMap& compatibility) {
  const QVariantList rows = compatibility.value(QStringLiteral("printers")).toList();
  for (const QVariant& rowValue : rows) {
    const QVariantMap row = rowValue.toMap();
    if (compatibilityEntryAllows(row)) {
      continue;
    }
    const QString reason = normalizedCompatibilityReason(row.value(QStringLiteral("reason")).toString());
    if (!reason.isEmpty()) {
      return reason;
    }
  }
  return {};
}

QVariantMap uiPrinterRow(const QVariantMap& printer) {
  QVariantMap row;
  const QString id = printer.value(QStringLiteral("id")).toString().trimmed();
  row.insert(QStringLiteral("id"), id);
  row.insert(QStringLiteral("name"), printer.value(QStringLiteral("name"), id));
  row.insert(QStringLiteral("model"), printer.value(QStringLiteral("model")));
  row.insert(QStringLiteral("state"), printer.value(QStringLiteral("state"), QStringLiteral("READY")));
  row.insert(QStringLiteral("machineType"), printer.value(QStringLiteral("machineType")));
  return row;
}

QVariantMap blocked(QString reasonKey, QString reason = {}) {
  QVariantMap out;
  out.insert(QStringLiteral("ok"), false);
  out.insert(QStringLiteral("reasonKey"), std::move(reasonKey));
  out.insert(QStringLiteral("reason"), std::move(reason));
  return out;
}

} // namespace

QVariantMap PrepareRemotePrintUseCase::evaluateGuard(const QString& mode,
                                                     const QVariantMap& printer,
                                                     const QVariantMap& fileData) const {
  if (printer.isEmpty()) {
    return blocked(QStringLiteral("select_printer"), QStringLiteral("Select a printer first."));
  }
  const QString state = printer.value(QStringLiteral("state")).toString().trimmed().toUpper();
  if (state == QStringLiteral("OFFLINE")) {
    return blocked(QStringLiteral("printer_offline"), QStringLiteral("Printer offline."));
  }
  if (state == QStringLiteral("PRINTING")) {
    return blocked(QStringLiteral("printer_printing"), QStringLiteral("Printer is currently printing."));
  }
  if (state == QStringLiteral("ERROR")) {
    return blocked(QStringLiteral("printer_error"), QStringLiteral("Printer reported an error."));
  }
  if (mode.compare(QStringLiteral("direct"), Qt::CaseInsensitive) != 0 && fileData.isEmpty()) {
    return blocked(QStringLiteral("select_file"), QStringLiteral("Select a cloud file first."));
  }
  if (!accloud::printing::fileHasLocalCompatibilityMetadata(fileData)) {
    return {{QStringLiteral("ok"), true}, {QStringLiteral("reasonKey"), QString{}}, {QStringLiteral("reason"), QString{}}};
  }
  return accloud::printing::printerFileCompatibilityToVariantMap(
      accloud::printing::evaluatePrinterFileCompatibility(printer, fileData));
}

QVariantMap PrepareRemotePrintUseCase::execute(const QString& mode,
                                               const QString& fileId,
                                               const QString& fileName,
                                               const QVariantMap& fileData,
                                               const QVariantList& printers,
                                               const QString& preferredPrinterId,
                                               const QVariantMap& serverCompatibility,
                                               bool compatibilityChecked,
                                               bool compatibilityFailed) const {
  QVariantMap out;
  out.insert(QStringLiteral("fileId"), fileId.trimmed());
  out.insert(QStringLiteral("fileName"), fileName.trimmed());
  out.insert(QStringLiteral("compatibilityResult"), serverCompatibility);
  out.insert(QStringLiteral("compatibilityChecked"), compatibilityChecked);
  out.insert(QStringLiteral("compatibilityFailed"), compatibilityFailed);

  QSet<QString> serverAllowed;
  const bool hasServerCompatibility = serverCompatibility.value(QStringLiteral("ok")).toBool();
  if (hasServerCompatibility) {
    const QVariantList serverRows = serverCompatibility.value(QStringLiteral("printers")).toList();
    for (const QVariant& rowValue : serverRows) {
      const QVariantMap row = rowValue.toMap();
      const QString id = row.value(QStringLiteral("id")).toString().trimmed();
      if (!id.isEmpty() && compatibilityEntryAllows(row)) {
        serverAllowed.insert(id);
      }
    }
  }

  const bool hasLocalMetadata = accloud::printing::fileHasLocalCompatibilityMetadata(fileData);
  const bool filterLocally = !hasServerCompatibility && hasLocalMetadata;
  QVariantList compatiblePrinters;
  QVariantMap selectedPrinter;
  const QString preferred = preferredPrinterId.trimmed();

  for (const QVariant& printerValue : printers) {
    const QVariantMap printer = printerValue.toMap();
    const QString id = printer.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
      continue;
    }
    if (hasServerCompatibility && !serverAllowed.contains(id)) {
      continue;
    }
    if (filterLocally && !accloud::printing::evaluatePrinterFileCompatibility(printer, fileData).ok) {
      continue;
    }
    compatiblePrinters.push_back(uiPrinterRow(printer));
    if (selectedPrinter.isEmpty() || (!preferred.isEmpty() && id == preferred)) {
      selectedPrinter = printer;
      if (!preferred.isEmpty() && id == preferred) {
        // Preferred compatible printer wins; keep scanning only for model output.
      }
    }
  }

  out.insert(QStringLiteral("compatiblePrinters"), compatiblePrinters);
  if (compatiblePrinters.isEmpty()) {
    out.insert(QStringLiteral("allowed"), false);
    out.insert(QStringLiteral("selectedPrinterId"), QString{});
    if (hasServerCompatibility) {
      out.insert(QStringLiteral("blockReasonKey"), QStringLiteral("no_compatible_printer"));
      out.insert(QStringLiteral("blockReason"), firstServerBlockReason(serverCompatibility));
    } else if (filterLocally) {
      out.insert(QStringLiteral("blockReasonKey"), QStringLiteral("no_compatible_printer"));
      out.insert(QStringLiteral("blockReason"), QStringLiteral("No compatible printer available for this file."));
    } else {
      out.insert(QStringLiteral("blockReasonKey"), QStringLiteral("no_printer"));
      out.insert(QStringLiteral("blockReason"), QStringLiteral("No printer available for remote print."));
    }
    out.insert(QStringLiteral("bestEffortWarning"), compatibilityChecked && compatibilityFailed);
    return out;
  }

  QString selectedId;
  if (!preferred.isEmpty()) {
    for (const QVariant& rowValue : compatiblePrinters) {
      if (rowValue.toMap().value(QStringLiteral("id")).toString() == preferred) {
        selectedId = preferred;
        break;
      }
    }
  }
  if (selectedId.isEmpty()) {
    selectedId = compatiblePrinters.first().toMap().value(QStringLiteral("id")).toString();
  }
  for (const QVariant& printerValue : printers) {
    const QVariantMap printer = printerValue.toMap();
    if (printer.value(QStringLiteral("id")).toString() == selectedId) {
      selectedPrinter = printer;
      break;
    }
  }

  const QVariantMap guard = evaluateGuard(mode, selectedPrinter, fileData);
  out.insert(QStringLiteral("selectedPrinterId"), selectedId);
  out.insert(QStringLiteral("allowed"), guard.value(QStringLiteral("ok")).toBool());
  out.insert(QStringLiteral("blockReasonKey"), guard.value(QStringLiteral("reasonKey")));
  out.insert(QStringLiteral("blockReason"), guard.value(QStringLiteral("reason")));
  out.insert(QStringLiteral("bestEffortWarning"), compatibilityChecked && compatibilityFailed
             && !hasServerCompatibility);
  return out;
}

} // namespace accloud::usecases::printing
