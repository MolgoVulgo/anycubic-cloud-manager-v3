#pragma once

#include <QAbstractListModel>
#include <QString>

#include <deque>

namespace accloud {

class MqttTailModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(QString topicFilter READ topicFilter WRITE setTopicFilter NOTIFY topicFilterChanged)

 public:
  enum Role {
    TimestampRole = Qt::UserRole + 1,
    TopicRole,
    PayloadRole,
    LineRole,
    PayloadSizeRole
  };

  explicit MqttTailModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;
  [[nodiscard]] QString topicFilter() const;

  void setTopicFilter(const QString& value);
  void appendMessage(const QString& timestamp,
                     const QString& topic,
                     const QString& payload,
                     qsizetype payloadSize,
                     const QString& line);
  Q_INVOKABLE void clear();
  Q_INVOKABLE QString messagesForTopic(const QString& topic) const;

 signals:
  void countChanged();
  void topicFilterChanged();

 private:
  struct Entry {
    QString timestamp;
    QString topic;
    QString payload;
    QString line;
    qsizetype payloadSize{0};
  };

  [[nodiscard]] bool matchesFilter(const Entry& entry) const;
  void rebuildVisible();
  void resetVisible();

  std::deque<Entry> m_entries;
  std::vector<int> m_visibleRows;
  QString m_topicFilter;
  int m_maxEntries{1200};
};

}  // namespace accloud
