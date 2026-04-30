#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace accloud {

class PrintersModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

 public:
  enum Role {
    IdRole = Qt::UserRole + 1,
    NameRole,
    ModelRole,
    TypeRole,
    StateRole,
    ReasonRole,
    AvailableRole,
    ProgressRole,
    CurrentFileRole,
    ElapsedSecRole,
    RemainingSecRole,
    CurrentLayerRole,
    TotalLayersRole,
    LastSeenRole,
    PrinterKeyRole,
    MachineTypeRole,
    DetailsRole,
    ProjectsRole,
    DetailsRawJsonRole,
    ProjectsRawJsonRole,
    ImgRole,
    ImageRole,
    PreviewRole,
    ThumbnailUrlRole
  };

  explicit PrintersModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;

  Q_INVOKABLE QVariantMap get(int row) const;
  Q_INVOKABLE bool replaceOrPatchPrinters(const QVariantList& printers);
  Q_INVOKABLE void clear();

 signals:
  void countChanged();

 private:
  [[nodiscard]] static QVariantMap cleanPrinter(const QVariantMap& source);
  [[nodiscard]] static QString idFor(const QVariantMap& printer);
  [[nodiscard]] static QString fingerprint(const QVariantMap& printer);
  [[nodiscard]] static QVariant valueForRole(const QVariantMap& printer, int role);
  void replaceAll(std::vector<QVariantMap> printers);

  std::vector<QVariantMap> m_printers;
};

}  // namespace accloud
