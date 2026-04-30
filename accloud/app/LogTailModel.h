#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace accloud {

class LogTailModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
  Q_PROPERTY(QString minLevel READ minLevel WRITE setMinLevel NOTIFY filtersChanged)
  Q_PROPERTY(QString sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY filtersChanged)
  Q_PROPERTY(QString componentFilter READ componentFilter WRITE setComponentFilter NOTIFY filtersChanged)
  Q_PROPERTY(QString eventFilter READ eventFilter WRITE setEventFilter NOTIFY filtersChanged)
  Q_PROPERTY(QString opIdFilter READ opIdFilter WRITE setOpIdFilter NOTIFY filtersChanged)
  Q_PROPERTY(QString queryFilter READ queryFilter WRITE setQueryFilter NOTIFY filtersChanged)

 public:
  enum Role {
    SinkRole = Qt::UserRole + 1,
    TimestampRole,
    LevelRole,
    SourceRole,
    ComponentRole,
    EventRole,
    OpIdRole,
    MessageRole,
    FormattedRole,
    LogicalSourceRole
  };

  explicit LogTailModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;
  [[nodiscard]] int totalCount() const;

  [[nodiscard]] QString minLevel() const;
  [[nodiscard]] QString sourceFilter() const;
  [[nodiscard]] QString componentFilter() const;
  [[nodiscard]] QString eventFilter() const;
  [[nodiscard]] QString opIdFilter() const;
  [[nodiscard]] QString queryFilter() const;

  void setMinLevel(const QString& value);
  void setSourceFilter(const QString& value);
  void setComponentFilter(const QString& value);
  void setEventFilter(const QString& value);
  void setOpIdFilter(const QString& value);
  void setQueryFilter(const QString& value);

  Q_INVOKABLE void replaceEntries(const QVariantList& entries);
  Q_INVOKABLE void clear();
  Q_INVOKABLE QString visibleText() const;

 signals:
  void countChanged();
  void filtersChanged();

 private:
  [[nodiscard]] static QVariantMap normalizeEntry(const QVariantMap& entry);
  [[nodiscard]] static QString logicalSourceFor(const QVariantMap& entry);
  [[nodiscard]] static int levelRank(const QString& level);
  [[nodiscard]] bool matchesFilters(const QVariantMap& entry) const;
  void rebuildVisible();
  void resetVisible();

  std::vector<QVariantMap> m_entries;
  std::vector<int> m_visibleRows;
  QString m_minLevel{QStringLiteral("all")};
  QString m_sourceFilter{QStringLiteral("__all__")};
  QString m_componentFilter{QStringLiteral("__all__")};
  QString m_eventFilter{QStringLiteral("__all__")};
  QString m_opIdFilter;
  QString m_queryFilter;
};

}  // namespace accloud
