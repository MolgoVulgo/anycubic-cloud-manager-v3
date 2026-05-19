#include "LogTailModel.h"

#include <QStringList>

namespace accloud {

namespace {

const QHash<int, QByteArray>& logTailRoleNames() {
  static const QHash<int, QByteArray> kRoles{
      {LogTailModel::SinkRole, "sink"},
      {LogTailModel::TimestampRole, "timestamp"},
      {LogTailModel::LevelRole, "level"},
      {LogTailModel::SourceRole, "source"},
      {LogTailModel::ComponentRole, "component"},
      {LogTailModel::EventRole, "event"},
      {LogTailModel::OpIdRole, "opId"},
      {LogTailModel::MessageRole, "message"},
      {LogTailModel::FormattedRole, "formatted"},
      {LogTailModel::LogicalSourceRole, "logicalSource"},
  };
  return kRoles;
}

QString normalizedFilter(QString value, const QString& fallback) {
  value = value.trimmed();
  return value.isEmpty() ? fallback : value;
}

}  // namespace

LogTailModel::LogTailModel(QObject* parent) : QAbstractListModel(parent) {}

int LogTailModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_visibleRows.size());
}

QVariant LogTailModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }
  const int sourceIndex = m_visibleRows[static_cast<std::size_t>(index.row())];
  const QVariantMap& entry = m_entries[static_cast<std::size_t>(sourceIndex)];
  switch (role) {
    case SinkRole:
      return entry.value(QStringLiteral("sink"));
    case TimestampRole:
      return entry.value(QStringLiteral("ts"));
    case LevelRole:
      return entry.value(QStringLiteral("level"));
    case SourceRole:
      return entry.value(QStringLiteral("source"));
    case ComponentRole:
      return entry.value(QStringLiteral("component"));
    case EventRole:
      return entry.value(QStringLiteral("event"));
    case OpIdRole:
      return entry.value(QStringLiteral("opId"));
    case MessageRole:
      return entry.value(QStringLiteral("message"));
    case FormattedRole:
      return entry.value(QStringLiteral("formatted"));
    case LogicalSourceRole:
      return entry.value(QStringLiteral("logicalSource"));
    default:
      return {};
  }
}

QHash<int, QByteArray> LogTailModel::roleNames() const {
  return logTailRoleNames();
}

int LogTailModel::count() const {
  return rowCount();
}

int LogTailModel::totalCount() const {
  return static_cast<int>(m_entries.size());
}

QString LogTailModel::minLevel() const { return m_minLevel; }
QString LogTailModel::sourceFilter() const { return m_sourceFilter; }
QString LogTailModel::componentFilter() const { return m_componentFilter; }
QString LogTailModel::eventFilter() const { return m_eventFilter; }
QString LogTailModel::opIdFilter() const { return m_opIdFilter; }
QString LogTailModel::queryFilter() const { return m_queryFilter; }

void LogTailModel::setMinLevel(const QString& value) {
  const QString next = normalizedFilter(value, QStringLiteral("all"));
  if (m_minLevel == next) return;
  m_minLevel = next;
  emit filtersChanged();
  resetVisible();
}

void LogTailModel::setSourceFilter(const QString& value) {
  const QString next = normalizedFilter(value, QStringLiteral("__all__"));
  if (m_sourceFilter == next) return;
  m_sourceFilter = next;
  emit filtersChanged();
  resetVisible();
}

void LogTailModel::setComponentFilter(const QString& value) {
  const QString next = normalizedFilter(value, QStringLiteral("__all__"));
  if (m_componentFilter == next) return;
  m_componentFilter = next;
  emit filtersChanged();
  resetVisible();
}

void LogTailModel::setEventFilter(const QString& value) {
  const QString next = normalizedFilter(value, QStringLiteral("__all__"));
  if (m_eventFilter == next) return;
  m_eventFilter = next;
  emit filtersChanged();
  resetVisible();
}

void LogTailModel::setOpIdFilter(const QString& value) {
  const QString next = value.trimmed();
  if (m_opIdFilter == next) return;
  m_opIdFilter = next;
  emit filtersChanged();
  resetVisible();
}

void LogTailModel::setQueryFilter(const QString& value) {
  const QString next = value.trimmed().toLower();
  if (m_queryFilter == next) return;
  m_queryFilter = next;
  emit filtersChanged();
  resetVisible();
}

void LogTailModel::replaceEntries(const QVariantList& entries) {
  beginResetModel();
  m_entries.clear();
  m_entries.reserve(static_cast<std::size_t>(entries.size()));
  for (const QVariant& item : entries) {
    m_entries.push_back(normalizeEntry(item.toMap()));
  }
  rebuildVisible();
  endResetModel();
  emit countChanged();
}

void LogTailModel::clear() {
  replaceEntries({});
}

QString LogTailModel::visibleText() const {
  QStringList out;
  out.reserve(static_cast<qsizetype>(m_visibleRows.size()));
  for (int sourceIndex : m_visibleRows) {
    out.push_back(m_entries[static_cast<std::size_t>(sourceIndex)].value(QStringLiteral("formatted")).toString());
  }
  return out.join('\n');
}

QVariantMap LogTailModel::normalizeEntry(const QVariantMap& entry) {
  QVariantMap out;
  out.insert(QStringLiteral("sink"), entry.value(QStringLiteral("sink"), QStringLiteral("app")).toString());
  out.insert(QStringLiteral("ts"), entry.value(QStringLiteral("ts")).toString());
  out.insert(QStringLiteral("level"), entry.value(QStringLiteral("level"), QStringLiteral("INFO")).toString());
  out.insert(QStringLiteral("source"), entry.value(QStringLiteral("source"), out.value(QStringLiteral("sink"))).toString());
  out.insert(QStringLiteral("component"), entry.value(QStringLiteral("component")).toString());
  out.insert(QStringLiteral("event"), entry.value(QStringLiteral("event")).toString());
  out.insert(QStringLiteral("opId"), entry.value(QStringLiteral("opId")).toString());
  out.insert(QStringLiteral("message"), entry.value(QStringLiteral("message")).toString());
  out.insert(QStringLiteral("formatted"),
             entry.value(QStringLiteral("formatted"),
                         QStringLiteral("[%1] %2 %3").arg(out.value(QStringLiteral("sink")).toString(),
                                                          out.value(QStringLiteral("level")).toString(),
                                                          out.value(QStringLiteral("message")).toString()))
                 .toString());
  out.insert(QStringLiteral("logicalSource"), logicalSourceFor(out));
  return out;
}

QString LogTailModel::logicalSourceFor(const QVariantMap& entry) {
  const QString sink = entry.value(QStringLiteral("sink")).toString().toLower().trimmed();
  const QString source = entry.value(QStringLiteral("source"), sink).toString().toLower().trimmed();
  const QString component = entry.value(QStringLiteral("component")).toString().toLower().trimmed();
  const QString eventName = entry.value(QStringLiteral("event")).toString().toLower().trimmed();
  if (sink == QStringLiteral("mqtt") || source == QStringLiteral("mqtt")) return QStringLiteral("mqtt");
  if (component.startsWith(QStringLiteral("mqtt")) || eventName.startsWith(QStringLiteral("mqtt"))) return QStringLiteral("mqtt");
  if (component.contains(QStringLiteral("mqtt_")) || eventName.contains(QStringLiteral("mqtt_"))) return QStringLiteral("mqtt");
  if (!source.isEmpty()) return source;
  if (!sink.isEmpty()) return sink;
  return QStringLiteral("app");
}

int LogTailModel::levelRank(const QString& level) {
  const QString upper = level.toUpper();
  if (upper == QStringLiteral("DEBUG")) return 0;
  if (upper == QStringLiteral("INFO")) return 1;
  if (upper == QStringLiteral("WARN")) return 2;
  if (upper == QStringLiteral("ERROR")) return 3;
  if (upper == QStringLiteral("FATAL")) return 4;
  return 1;
}

bool LogTailModel::matchesFilters(const QVariantMap& entry) const {
  int requiredRank = 0;
  if (m_minLevel == QStringLiteral("info_plus")) requiredRank = 1;
  else if (m_minLevel == QStringLiteral("warn_plus")) requiredRank = 2;
  else if (m_minLevel == QStringLiteral("error")) requiredRank = 3;
  if (levelRank(entry.value(QStringLiteral("level")).toString()) < requiredRank) return false;
  if (m_sourceFilter != QStringLiteral("__all__")
      && entry.value(QStringLiteral("logicalSource")).toString() != m_sourceFilter) return false;
  if (m_componentFilter != QStringLiteral("__all__")
      && entry.value(QStringLiteral("component")).toString() != m_componentFilter) return false;
  if (m_eventFilter != QStringLiteral("__all__")
      && entry.value(QStringLiteral("event")).toString() != m_eventFilter) return false;
  if (!m_opIdFilter.isEmpty() && entry.value(QStringLiteral("opId")).toString() != m_opIdFilter) return false;
  if (!m_queryFilter.isEmpty()
      && !entry.value(QStringLiteral("formatted")).toString().toLower().contains(m_queryFilter)) return false;
  return true;
}

void LogTailModel::rebuildVisible() {
  m_visibleRows.clear();
  for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
    if (matchesFilters(m_entries[static_cast<std::size_t>(i)])) {
      m_visibleRows.push_back(i);
    }
  }
}

void LogTailModel::resetVisible() {
  beginResetModel();
  rebuildVisible();
  endResetModel();
  emit countChanged();
}

}  // namespace accloud
