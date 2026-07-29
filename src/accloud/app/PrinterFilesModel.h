#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace accloud {

class PrinterFilesModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

 public:
  enum Role {
    FileIdRole = Qt::UserRole + 1,
    FileNameRole,
    SizeTextRole,
    StatusRole,
    PrintTimeRole,
    ResinUsageRole,
    MachineRole,
    MachineTypeRole
  };

  explicit PrinterFilesModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;

  Q_INVOKABLE QVariantMap get(int row) const;
  Q_INVOKABLE bool replaceOrPatchFiles(const QVariantList& files);
  Q_INVOKABLE bool remove(int row);
  Q_INVOKABLE void clear();

 signals:
  void countChanged();

 private:
  [[nodiscard]] static QString identityFor(const QVariantMap& file);
  [[nodiscard]] static QVariant valueForRole(const QVariantMap& file, int role);
  void replaceAll(std::vector<QVariantMap> files);

  std::vector<QVariantMap> m_files;
};

}  // namespace accloud
