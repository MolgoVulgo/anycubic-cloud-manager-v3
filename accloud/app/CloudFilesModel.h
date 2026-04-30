#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace accloud {

class CloudFilesModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(int visibleCount READ visibleCount NOTIFY visibleChanged)
  Q_PROPERTY(int totalPages READ totalPages NOTIFY visibleChanged)
  Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
  Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY pageSizeChanged)
  Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY typeFilterChanged)

 public:
  enum Role {
    FileIdRole = Qt::UserRole + 1,
    FileNameRole,
    ThumbnailUrlRole,
    SizeTextRole,
    FileTypeTextRole,
    DateTextRole,
    StatusRole
  };

  explicit CloudFilesModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;
  [[nodiscard]] int visibleCount() const;
  [[nodiscard]] int totalPages() const;
  [[nodiscard]] int currentPage() const;
  [[nodiscard]] int pageSize() const;
  [[nodiscard]] QString typeFilter() const;

  void setCurrentPage(int value);
  void setPageSize(int value);
  void setTypeFilter(const QString& value);

  Q_INVOKABLE QVariantMap get(int row) const;
  Q_INVOKABLE QVariantMap fileDataById(const QString& fileId) const;
  Q_INVOKABLE QStringList availableFileTypes(const QVariantList& supportedExtensions) const;
  Q_INVOKABLE void replaceFiles(const QVariantList& files);
  Q_INVOKABLE void append(const QVariantMap& file);
  Q_INVOKABLE void clear();

 signals:
  void countChanged();
  void visibleChanged();
  void currentPageChanged();
  void pageSizeChanged();
  void typeFilterChanged();

 private:
  struct Row {
    QVariantMap raw;
    QVariantMap display;
  };

  [[nodiscard]] static QString fileExtension(const QString& fileName);
  [[nodiscard]] static QString fileTypeText(const QString& fileName);
  [[nodiscard]] static QString dateText(const QVariantMap& file);
  [[nodiscard]] bool matchesFilter(const Row& row) const;
  void rebuildVisible();
  void rebuildVisibleReset();
  void clampCurrentPage();

  std::vector<Row> m_all;
  std::vector<int> m_visibleRows;
  int m_visibleCount{0};
  int m_totalPages{1};
  int m_currentPage{0};
  int m_pageSize{10};
  QString m_typeFilter{QStringLiteral("all")};
};

}  // namespace accloud
