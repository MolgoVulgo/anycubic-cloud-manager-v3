#include "PrinterFileCompatibility.h"

#include <QStringList>

#include <utility>

namespace accloud::printing {
namespace {

QString normalizedCompatText(const QVariant& value) {
  QString out;
  const QString raw = value.toString().trimmed().toLower();
  out.reserve(raw.size());
  bool previousSpace = false;
  for (const QChar ch : raw) {
    const bool separator = ch == QLatin1Char('_')
        || ch == QLatin1Char('-')
        || ch == QLatin1Char('.')
        || ch == QLatin1Char('/');
    const bool space = ch.isSpace() || separator;
    if (space) {
      if (!previousSpace && !out.isEmpty()) {
        out.append(QLatin1Char(' '));
      }
      previousSpace = true;
      continue;
    }
    out.append(ch);
    previousSpace = false;
  }
  return out.trimmed();
}

QStringList compatTokens(const QString& value) {
  static const QStringList stopTokens = {
      QStringLiteral("anycubic"), QStringLiteral("photon"), QStringLiteral("mono"),
      QStringLiteral("printer"), QStringLiteral("printers"), QStringLiteral("series"),
      QStringLiteral("resin"), QStringLiteral("lcd")};
  QStringList out;
  const QStringList rawTokens = normalizedCompatText(value).split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString& rawToken : rawTokens) {
    const QString token = rawToken.trimmed();
    if (token.size() <= 1 || stopTokens.contains(token) || out.contains(token)) {
      continue;
    }
    out.push_back(token);
  }
  return out;
}

int compatTokenOverlapCount(const QStringList& a, const QStringList& b) {
  int count = 0;
  for (const QString& token : b) {
    if (a.contains(token)) {
      ++count;
    }
  }
  return count;
}

bool compatTextContains(const QString& haystack, const QString& needle) {
  const QString h = normalizedCompatText(haystack);
  const QString n = normalizedCompatText(needle);
  return !h.isEmpty() && !n.isEmpty() && h.contains(n);
}

PrinterFileCompatibilityResult result(bool ok, int score, QString reason, QString reasonKey) {
  return {ok, score, std::move(reason), std::move(reasonKey)};
}

} // namespace

QString cloudSliceFileExtension(const QString& fileName) {
  const int dot = fileName.lastIndexOf(QLatin1Char('.'));
  if (dot < 0 || dot + 1 >= fileName.size()) {
    return {};
  }
  return fileName.mid(dot + 1).trimmed().toLower();
}

bool isKnownCloudSliceExtension(const QString& ext) {
  static const QStringList known = {
      QStringLiteral("photon"), QStringLiteral("pws"), QStringLiteral("pwsz"),
      QStringLiteral("photons"), QStringLiteral("pw0"), QStringLiteral("pwx"),
      QStringLiteral("pwmo"), QStringLiteral("pwma"), QStringLiteral("pwms"),
      QStringLiteral("pwmx"), QStringLiteral("pmx2"), QStringLiteral("pmsq"),
      QStringLiteral("dlp"), QStringLiteral("dl2p"), QStringLiteral("pwmb"),
      QStringLiteral("pm3"), QStringLiteral("pm3m"), QStringLiteral("pm3r"),
      QStringLiteral("pm3n"), QStringLiteral("px6s"), QStringLiteral("pm5"),
      QStringLiteral("pm5s"), QStringLiteral("m5sp")};
  const QString value = ext.trimmed().toLower();
  return !value.isEmpty() && known.contains(value);
}

bool fileHasLocalCompatibilityMetadata(const QVariantMap& file) {
  if (file.isEmpty()) {
    return false;
  }
  const QString machineText = normalizedCompatText(file.value(QStringLiteral("machine")));
  const QString printersText = normalizedCompatText(file.value(QStringLiteral("printers")));
  QString machineType = normalizedCompatText(file.value(QStringLiteral("machineType")));
  if (machineType.isEmpty()) {
    machineType = normalizedCompatText(file.value(QStringLiteral("machineTypeId")));
  }
  return !machineText.isEmpty() || !printersText.isEmpty() || !machineType.isEmpty();
}

PrinterFileCompatibilityResult evaluatePrinterFileCompatibility(
    const QVariantMap& printer,
    const QVariantMap& file) {
  if (printer.isEmpty()) {
    return result(false, 0, QStringLiteral("Select a printer first."), QStringLiteral("select_printer"));
  }
  if (file.isEmpty()) {
    return result(false, 0, QStringLiteral("Select a cloud file first."), QStringLiteral("select_file"));
  }

  const QString ext = cloudSliceFileExtension(file.value(QStringLiteral("fileName")).toString());
  if (!isKnownCloudSliceExtension(ext)) {
    return result(false, 0, QStringLiteral("Unsupported file format."), QStringLiteral("unsupported_format"));
  }

  const QString printerMachineType = normalizedCompatText(printer.value(QStringLiteral("machineType")));
  QString fileMachineType = normalizedCompatText(file.value(QStringLiteral("machineType")));
  if (fileMachineType.isEmpty()) {
    fileMachineType = normalizedCompatText(file.value(QStringLiteral("machineTypeId")));
  }
  if (!fileMachineType.isEmpty() && !printerMachineType.isEmpty()) {
    if (fileMachineType == printerMachineType) {
      return result(true, 500, {}, {});
    }
    return result(false, 0, QStringLiteral("Slice file does not match machine type."),
                  QStringLiteral("machine_type_mismatch"));
  }

  const QString machineText = normalizedCompatText(file.value(QStringLiteral("machine")));
  const QString printersText = normalizedCompatText(file.value(QStringLiteral("printers")));
  const QString metadataText = (machineText + QLatin1Char(' ') + printersText).trimmed();
  if (metadataText.isEmpty()) {
    return result(false, 0, QStringLiteral("Missing local compatibility metadata."),
                  QStringLiteral("missing_metadata"));
  }

  const QString printerModel = normalizedCompatText(printer.value(QStringLiteral("model")));
  const QString printerName = normalizedCompatText(printer.value(QStringLiteral("name")));
  if (!machineText.isEmpty() && (machineText == printerModel || machineText == printerName)) {
    return result(true, 420, {}, {});
  }
  if (compatTextContains(machineText, printerModel)
      || compatTextContains(printerModel, machineText)
      || compatTextContains(machineText, printerName)
      || compatTextContains(printerName, machineText)) {
    return result(true, 360, {}, {});
  }
  if (compatTextContains(printersText, printerModel)
      || compatTextContains(printersText, printerName)
      || compatTextContains(printerModel, printersText)
      || compatTextContains(printerName, printersText)) {
    return result(true, 330, {}, {});
  }

  const QStringList metadataTokens = compatTokens(metadataText);
  const QStringList printerTokens = compatTokens(printerModel + QLatin1Char(' ')
                                                + printerName + QLatin1Char(' ')
                                                + printerMachineType);
  const int overlapCount = compatTokenOverlapCount(metadataTokens, printerTokens);
  if (overlapCount > 0) {
    return result(true, 280 + overlapCount, {}, {});
  }
  return result(false, 0, QStringLiteral("Slice file does not match selected printer model."),
                QStringLiteral("model_mismatch"));
}

QVariantMap printerFileCompatibilityToVariantMap(const PrinterFileCompatibilityResult& value) {
  QVariantMap out;
  out.insert(QStringLiteral("ok"), value.ok);
  out.insert(QStringLiteral("score"), value.score);
  out.insert(QStringLiteral("reason"), value.reason);
  out.insert(QStringLiteral("reasonKey"), value.reasonKey);
  return out;
}

} // namespace accloud::printing
