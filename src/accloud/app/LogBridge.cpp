#include "LogBridge.h"

#include "LogTailModel.h"
#include "UiPerfTrace.h"
#include "infra/logging/JsonlLogger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QVariantList>
#include <QVector>

#include <algorithm>

namespace accloud {
namespace {

struct Entry {
  QString sink;
  QString ts;
  QString level;
  QString source;
  QString component;
  QString event;
  QString message;
  QString opId;
  QString formatted;
  qint64 sequence = 0;
};

QString normalizedLevel(const QString& levelRaw) {
  const QString level = levelRaw.trimmed().toUpper();
  if (level == "DEBUG" || level == "INFO" || level == "WARN" || level == "ERROR" ||
      level == "FATAL") {
    return level;
  }
  return QStringLiteral("INFO");
}

QString valueToString(const QJsonValue& value) {
  if (value.isString()) {
    return value.toString();
  }
  if (value.isBool()) {
    return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  if (value.isDouble()) {
    return QString::number(value.toDouble(), 'g', 12);
  }
  if (value.isNull()) {
    return QStringLiteral("null");
  }
  if (value.isObject()) {
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
  }
  if (value.isArray()) {
    return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
  }
  return {};
}

QString detectOpId(const QJsonObject& fields) {
  for (const QString& key : {QStringLiteral("op_id"), QStringLiteral("opId"),
                             QStringLiteral("operation_id"), QStringLiteral("operationId")}) {
    if (!fields.contains(key)) {
      continue;
    }
    const QString value = valueToString(fields.value(key)).trimmed();
    if (!value.isEmpty()) {
      return value;
    }
  }
  return {};
}

QString buildFormattedLine(const Entry& entry, const QJsonObject& fields) {
  QStringList parts;
  if (!entry.ts.isEmpty()) {
    parts << entry.ts;
  }
  parts << ("[" + entry.sink + "]");
  if (!entry.source.isEmpty()) {
    parts << entry.source;
  }
  parts << entry.level;

  QString componentEvent;
  if (!entry.component.isEmpty() && !entry.event.isEmpty()) {
    componentEvent = entry.component + "." + entry.event;
  } else if (!entry.component.isEmpty()) {
    componentEvent = entry.component;
  } else if (!entry.event.isEmpty()) {
    componentEvent = entry.event;
  }
  if (!componentEvent.isEmpty()) {
    parts << componentEvent;
  }

  QString line = parts.join(' ');
  if (!entry.message.isEmpty()) {
    line += " - " + entry.message;
  }

  QStringList fieldTokens;
  if (!entry.opId.isEmpty()) {
    fieldTokens << ("op_id=" + entry.opId);
  }
  QStringList keys = fields.keys();
  keys.sort(Qt::CaseInsensitive);
  for (const QString& key : keys) {
    if (key == QStringLiteral("op_id") || key == QStringLiteral("opId") ||
        key == QStringLiteral("operation_id") || key == QStringLiteral("operationId")) {
      continue;
    }
    const QString value = valueToString(fields.value(key)).simplified();
    if (!value.isEmpty()) {
      fieldTokens << (key + "=" + value);
    }
  }
  if (!fieldTokens.isEmpty()) {
    line += " " + fieldTokens.join(' ');
  }
  return line;
}

Entry parseLine(const QString& sink, const QString& line, qint64 sequence) {
  Entry entry;
  entry.sink = sink;
  entry.level = QStringLiteral("INFO");
  entry.source = sink;
  entry.formatted = "[" + sink + "] " + line;
  entry.sequence = sequence;

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(line.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return entry;
  }

  const QJsonObject root = document.object();
  entry.ts = root.value(QStringLiteral("ts")).toString();
  entry.level = normalizedLevel(root.value(QStringLiteral("level")).toString());

  const QString parsedSource = root.value(QStringLiteral("source")).toString().trimmed();
  if (!parsedSource.isEmpty()) {
    entry.source = parsedSource;
  }
  entry.component = root.value(QStringLiteral("component")).toString().trimmed();
  entry.event = root.value(QStringLiteral("event")).toString().trimmed();
  entry.message = root.value(QStringLiteral("message")).toString().trimmed();

  QJsonObject fields;
  const QJsonValue fieldsValue = root.value(QStringLiteral("fields"));
  if (fieldsValue.isObject()) {
    fields = fieldsValue.toObject();
  }
  entry.opId = detectOpId(fields);
  entry.formatted = buildFormattedLine(entry, fields);
  return entry;
}

QVariantMap toVariantMap(const Entry& entry) {
  QVariantMap out;
  out.insert(QStringLiteral("sink"), entry.sink);
  out.insert(QStringLiteral("ts"), entry.ts);
  out.insert(QStringLiteral("level"), entry.level);
  out.insert(QStringLiteral("source"), entry.source);
  out.insert(QStringLiteral("component"), entry.component);
  out.insert(QStringLiteral("event"), entry.event);
  out.insert(QStringLiteral("opId"), entry.opId);
  out.insert(QStringLiteral("message"), entry.message);
  out.insert(QStringLiteral("formatted"), entry.formatted);
  return out;
}

} // namespace

LogBridge::LogBridge(QObject* parent) : QObject(parent), m_tailModel(new LogTailModel(this)) {}

QAbstractListModel* LogBridge::tailModel() {
  return m_tailModel;
}

QVariantMap LogBridge::fetchSnapshot(int maxLines) const {
  UiPerfTrace perf("log_bridge.fetch_snapshot");
  perf.setField("max_lines", std::to_string(maxLines));
  QVariantMap out;
  const int cap = std::clamp(maxLines, 1, 5000);

  const QString logDirPath = QString::fromStdString(logging::logDirectory().string());
  QDir logDir(logDirPath);
  if (!logDir.exists()) {
    out.insert(QStringLiteral("ok"), false);
    out.insert(QStringLiteral("message"),
               QStringLiteral("Répertoire de logs introuvable: %1").arg(logDirPath));
    out.insert(QStringLiteral("logDir"), logDirPath);
    return out;
  }

  QFileInfoList logFiles =
      logDir.entryInfoList(QStringList{QStringLiteral("*.jsonl")}, QDir::Files | QDir::Readable,
                           QDir::Name | QDir::IgnoreCase);
  perf.setCount("log_files", logFiles.size());

  QSet<QString> sinks;
  QSet<QString> components;
  QSet<QString> events;
  QVector<Entry> entries;
  qint64 sequence = 0;

  for (const QFileInfo& fileInfo : logFiles) {
    const QString sink = fileInfo.completeBaseName();
    sinks.insert(sink);

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
      const QString rawLine = stream.readLine();
      if (rawLine.trimmed().isEmpty()) {
        continue;
      }
      Entry entry = parseLine(sink, rawLine, sequence++);
      if (!entry.component.isEmpty()) {
        components.insert(entry.component);
      }
      if (!entry.event.isEmpty()) {
        events.insert(entry.event);
      }
      entries.push_back(std::move(entry));
    }
  }

  std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
    const bool leftHasTs = !left.ts.isEmpty();
    const bool rightHasTs = !right.ts.isEmpty();
    if (leftHasTs != rightHasTs) {
      return leftHasTs;
    }
    if (left.ts != right.ts) {
      return left.ts < right.ts;
    }
    return left.sequence < right.sequence;
  });

  if (entries.size() > cap) {
    entries = entries.sliced(entries.size() - cap, cap);
  }
  perf.setCount("entries", entries.size());

  QStringList sinkList = sinks.values();
  sinkList.sort(Qt::CaseInsensitive);
  QStringList componentList = components.values();
  componentList.sort(Qt::CaseInsensitive);
  QStringList eventList = events.values();
  eventList.sort(Qt::CaseInsensitive);

  QVariantList entryList;
  entryList.reserve(entries.size());
  for (const Entry& entry : entries) {
    entryList.append(toVariantMap(entry));
  }

  out.insert(QStringLiteral("ok"), true);
  out.insert(QStringLiteral("message"), QStringLiteral("%1 ligne(s)").arg(entries.size()));
  out.insert(QStringLiteral("logDir"), logDirPath);
  out.insert(QStringLiteral("totalEntries"), entries.size());
  out.insert(QStringLiteral("sources"), sinkList);
  out.insert(QStringLiteral("components"), componentList);
  out.insert(QStringLiteral("events"), eventList);
  out.insert(QStringLiteral("entries"), entryList);
  return out;
}

} // namespace accloud
