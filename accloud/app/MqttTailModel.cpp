#include "MqttTailModel.h"

namespace accloud {

namespace {

const QHash<int, QByteArray>& mqttTailRoleNames() {
  static const QHash<int, QByteArray> kRoles{
      {MqttTailModel::TimestampRole, "timestamp"},
      {MqttTailModel::TopicRole, "topic"},
      {MqttTailModel::PayloadRole, "payload"},
      {MqttTailModel::LineRole, "line"},
      {MqttTailModel::PayloadSizeRole, "payloadSize"},
  };
  return kRoles;
}

}  // namespace

MqttTailModel::MqttTailModel(QObject* parent) : QAbstractListModel(parent) {}

int MqttTailModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_visibleRows.size());
}

QVariant MqttTailModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }
  const int sourceIndex = m_visibleRows[static_cast<std::size_t>(index.row())];
  const Entry& entry = m_entries[static_cast<std::size_t>(sourceIndex)];
  switch (role) {
    case TimestampRole:
      return entry.timestamp;
    case TopicRole:
      return entry.topic;
    case PayloadRole:
      return entry.payload;
    case LineRole:
      return entry.line;
    case PayloadSizeRole:
      return entry.payloadSize;
    default:
      return {};
  }
}

QHash<int, QByteArray> MqttTailModel::roleNames() const {
  return mqttTailRoleNames();
}

int MqttTailModel::count() const {
  return rowCount();
}

QString MqttTailModel::topicFilter() const {
  return m_topicFilter;
}

void MqttTailModel::setTopicFilter(const QString& value) {
  const QString next = value.trimmed().toLower();
  if (m_topicFilter == next) {
    return;
  }
  m_topicFilter = next;
  emit topicFilterChanged();
  resetVisible();
}

void MqttTailModel::appendMessage(const QString& timestamp,
                                  const QString& topic,
                                  const QString& payload,
                                  qsizetype payloadSize,
                                  const QString& line) {
  const int previousCount = count();
  m_entries.push_back(Entry{timestamp, topic, payload, line, payloadSize});
  while (static_cast<int>(m_entries.size()) > m_maxEntries) {
    m_entries.pop_front();
  }
  resetVisible();
  if (previousCount != count()) {
    emit countChanged();
  }
}

void MqttTailModel::clear() {
  if (m_entries.empty()) {
    return;
  }
  beginResetModel();
  m_entries.clear();
  m_visibleRows.clear();
  endResetModel();
  emit countChanged();
}

QString MqttTailModel::messagesForTopic(const QString& topic) const {
  const QString needle = topic.trimmed();
  QStringList out;
  for (const Entry& entry : m_entries) {
    if (needle.isEmpty() || entry.topic == needle) {
      out.push_back(entry.line);
    }
  }
  return out.join('\n');
}

bool MqttTailModel::matchesFilter(const Entry& entry) const {
  if (m_topicFilter.isEmpty()) {
    return true;
  }
  return entry.topic.toLower().contains(m_topicFilter) || entry.line.toLower().contains(m_topicFilter);
}

void MqttTailModel::rebuildVisible() {
  m_visibleRows.clear();
  for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
    if (matchesFilter(m_entries[static_cast<std::size_t>(i)])) {
      m_visibleRows.push_back(i);
    }
  }
}

void MqttTailModel::resetVisible() {
  beginResetModel();
  rebuildVisible();
  endResetModel();
}

}  // namespace accloud
