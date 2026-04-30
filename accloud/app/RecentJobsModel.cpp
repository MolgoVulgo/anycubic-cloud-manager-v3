#include "RecentJobsModel.h"

#include <algorithm>

namespace accloud {

namespace {

const QHash<int, QByteArray>& recentJobRoleNames() {
  static const QHash<int, QByteArray> kRoles{
      {RecentJobsModel::TaskIdRole, "taskId"},
      {RecentJobsModel::GcodeNameRole, "gcodeName"},
      {RecentJobsModel::PrinterIdRole, "printerId"},
      {RecentJobsModel::PrinterNameRole, "printerName"},
      {RecentJobsModel::PrintStatusRole, "printStatus"},
      {RecentJobsModel::ProgressRole, "progress"},
      {RecentJobsModel::ElapsedSecRole, "elapsedSec"},
      {RecentJobsModel::RemainingSecRole, "remainingSec"},
      {RecentJobsModel::CurrentLayerRole, "currentLayer"},
      {RecentJobsModel::TotalLayersRole, "totalLayers"},
      {RecentJobsModel::CurrentFileRole, "currentFile"},
      {RecentJobsModel::ReasonRole, "reason"},
      {RecentJobsModel::CreateTimeRole, "createTime"},
      {RecentJobsModel::EndTimeRole, "endTime"},
      {RecentJobsModel::ImgRole, "img"},
      {RecentJobsModel::ImgRawRole, "imgRaw"},
      {RecentJobsModel::ImageRole, "image"},
      {RecentJobsModel::PreviewRole, "preview"},
      {RecentJobsModel::ThumbnailUrlRole, "thumbnailUrl"},
  };
  return kRoles;
}

}  // namespace

RecentJobsModel::RecentJobsModel(QObject* parent) : QAbstractListModel(parent) {}

int RecentJobsModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_jobs.size());
}

QVariant RecentJobsModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }
  return valueForRole(m_jobs[static_cast<std::size_t>(index.row())], role);
}

QHash<int, QByteArray> RecentJobsModel::roleNames() const {
  return recentJobRoleNames();
}

int RecentJobsModel::count() const {
  return rowCount();
}

QVariantMap RecentJobsModel::get(int row) const {
  if (row < 0 || row >= rowCount()) {
    return {};
  }
  return m_jobs[static_cast<std::size_t>(row)];
}

bool RecentJobsModel::replaceOrPatchJobs(const QVariantList& jobs) {
  std::vector<QVariantMap> next;
  next.reserve(static_cast<std::size_t>(jobs.size()));
  for (const QVariant& item : jobs) {
    next.push_back(cleanJob(item.toMap()));
  }

  if (next.size() != m_jobs.size()) {
    replaceAll(std::move(next));
    return true;
  }

  bool sameIdentityOrder = true;
  for (std::size_t i = 0; i < next.size(); ++i) {
    const QString currentId = taskIdFor(m_jobs[i]);
    const QString nextId = taskIdFor(next[i]);
    if (!currentId.isEmpty() || !nextId.isEmpty()) {
      if (currentId != nextId) {
        sameIdentityOrder = false;
        break;
      }
    }
  }

  if (!sameIdentityOrder) {
    replaceAll(std::move(next));
    return true;
  }

  bool changed = false;
  for (std::size_t i = 0; i < next.size(); ++i) {
    if (fingerprint(m_jobs[i]) == fingerprint(next[i])) {
      continue;
    }
    m_jobs[i] = std::move(next[i]);
    const QModelIndex changedIndex = index(static_cast<int>(i), 0);
    emit dataChanged(changedIndex, changedIndex, recentJobRoleNames().keys());
    changed = true;
  }
  return changed;
}

bool RecentJobsModel::mergeOrPatchJobs(const QVariantList& jobs) {
  QVariantList merged;
  merged.reserve(count() + jobs.size());

  QHash<QString, int> byTaskId;
  for (int i = 0; i < count(); ++i) {
    QVariantMap job = get(i);
    const QString taskId = taskIdFor(job);
    if (!taskId.isEmpty()) {
      byTaskId.insert(taskId, merged.size());
    }
    merged.append(job);
  }

  for (const QVariant& item : jobs) {
    const QVariantMap incoming = cleanJob(item.toMap());
    const QString taskId = taskIdFor(incoming);
    if (!taskId.isEmpty() && byTaskId.contains(taskId)) {
      merged[byTaskId.value(taskId)] = incoming;
      continue;
    }
    if (!taskId.isEmpty()) {
      byTaskId.insert(taskId, merged.size());
    }
    merged.append(incoming);
  }

  std::sort(merged.begin(), merged.end(), [](const QVariant& left, const QVariant& right) {
    const QVariantMap a = left.toMap();
    const QVariantMap b = right.toMap();
    const qlonglong at = a.value(QStringLiteral("createTime")).toLongLong();
    const qlonglong bt = b.value(QStringLiteral("createTime")).toLongLong();
    const bool aValid = at > 0;
    const bool bValid = bt > 0;
    if (aValid && bValid && at != bt) {
      return at > bt;
    }
    if (aValid != bValid) {
      return aValid;
    }
    return taskIdFor(a) < taskIdFor(b);
  });

  return replaceOrPatchJobs(merged);
}

void RecentJobsModel::clear() {
  if (m_jobs.empty()) {
    return;
  }
  beginResetModel();
  m_jobs.clear();
  endResetModel();
  emit countChanged();
}

QVariantMap RecentJobsModel::cleanJob(const QVariantMap& source) {
  QVariantMap out = source;
  out.remove(QStringLiteral("__mergeOrder"));
  return out;
}

QString RecentJobsModel::taskIdFor(const QVariantMap& job) {
  return job.value(QStringLiteral("taskId")).toString().trimmed();
}

QString RecentJobsModel::fingerprint(const QVariantMap& job) {
  QStringList values;
  values.reserve(recentJobRoleNames().size());
  const auto roles = recentJobRoleNames().keys();
  for (int role : roles) {
    values.push_back(valueForRole(job, role).toString());
  }
  return values.join(QChar(0x1f));
}

QVariant RecentJobsModel::valueForRole(const QVariantMap& job, int role) {
  switch (role) {
    case TaskIdRole:
      return job.value(QStringLiteral("taskId"));
    case GcodeNameRole:
      return job.value(QStringLiteral("gcodeName"));
    case PrinterIdRole:
      return job.value(QStringLiteral("printerId"));
    case PrinterNameRole:
      return job.value(QStringLiteral("printerName"));
    case PrintStatusRole:
      return job.value(QStringLiteral("printStatus"));
    case ProgressRole:
      return job.value(QStringLiteral("progress"));
    case ElapsedSecRole:
      return job.value(QStringLiteral("elapsedSec"));
    case RemainingSecRole:
      return job.value(QStringLiteral("remainingSec"));
    case CurrentLayerRole:
      return job.value(QStringLiteral("currentLayer"));
    case TotalLayersRole:
      return job.value(QStringLiteral("totalLayers"));
    case CurrentFileRole:
      return job.value(QStringLiteral("currentFile"));
    case ReasonRole:
      return job.value(QStringLiteral("reason"));
    case CreateTimeRole:
      return job.value(QStringLiteral("createTime"));
    case EndTimeRole:
      return job.value(QStringLiteral("endTime"));
    case ImgRole:
      return job.value(QStringLiteral("img"));
    case ImgRawRole:
      return job.value(QStringLiteral("imgRaw"));
    case ImageRole:
      return job.value(QStringLiteral("image"));
    case PreviewRole:
      return job.value(QStringLiteral("preview"));
    case ThumbnailUrlRole:
      return job.value(QStringLiteral("thumbnailUrl"));
    default:
      return {};
  }
}

void RecentJobsModel::replaceAll(std::vector<QVariantMap> jobs) {
  const int previousCount = count();
  beginResetModel();
  m_jobs = std::move(jobs);
  endResetModel();
  if (previousCount != count()) {
    emit countChanged();
  }
}

}  // namespace accloud
