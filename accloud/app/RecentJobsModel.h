#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace accloud {

class RecentJobsModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

 public:
  enum Role {
    TaskIdRole = Qt::UserRole + 1,
    GcodeNameRole,
    PrinterIdRole,
    PrinterNameRole,
    PrintStatusRole,
    ProgressRole,
    ElapsedSecRole,
    RemainingSecRole,
    CurrentLayerRole,
    TotalLayersRole,
    CurrentFileRole,
    ReasonRole,
    CreateTimeRole,
    EndTimeRole,
    ImgRole,
    ImgRawRole,
    ImageRole,
    PreviewRole,
    ThumbnailUrlRole
  };

  explicit RecentJobsModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int count() const;

  Q_INVOKABLE QVariantMap get(int row) const;
  Q_INVOKABLE bool replaceOrPatchJobs(const QVariantList& jobs);
  Q_INVOKABLE bool mergeOrPatchJobs(const QVariantList& jobs);
  Q_INVOKABLE void clear();

 signals:
  void countChanged();

 private:
  [[nodiscard]] static QVariantMap cleanJob(const QVariantMap& source);
  [[nodiscard]] static QString taskIdFor(const QVariantMap& job);
  [[nodiscard]] static QString fingerprint(const QVariantMap& job);
  [[nodiscard]] static QVariant valueForRole(const QVariantMap& job, int role);
  void replaceAll(std::vector<QVariantMap> jobs);

  std::vector<QVariantMap> m_jobs;
};

}  // namespace accloud
