#include "PrinterFilesModel.h"

#include <algorithm>
#include <utility>

namespace accloud {

namespace {

const QHash<int, QByteArray>& printerFileRoleNames() {
  static const QHash<int, QByteArray> kRoles{
      {PrinterFilesModel::FileIdRole, "fileId"},
      {PrinterFilesModel::FileNameRole, "fileName"},
      {PrinterFilesModel::SizeTextRole, "sizeText"},
      {PrinterFilesModel::StatusRole, "status"},
      {PrinterFilesModel::PrintTimeRole, "printTime"},
      {PrinterFilesModel::ResinUsageRole, "resinUsage"},
      {PrinterFilesModel::MachineRole, "machine"},
      {PrinterFilesModel::MachineTypeRole, "machineType"},
  };
  return kRoles;
}

}  // namespace

PrinterFilesModel::PrinterFilesModel(QObject* parent) : QAbstractListModel(parent) {}

int PrinterFilesModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_files.size());
}

QVariant PrinterFilesModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }
  return valueForRole(m_files[static_cast<std::size_t>(index.row())], role);
}

QHash<int, QByteArray> PrinterFilesModel::roleNames() const {
  return printerFileRoleNames();
}

int PrinterFilesModel::count() const {
  return rowCount();
}

QVariantMap PrinterFilesModel::get(int row) const {
  if (row < 0 || row >= rowCount()) {
    return {};
  }
  return m_files[static_cast<std::size_t>(row)];
}

bool PrinterFilesModel::replaceOrPatchFiles(const QVariantList& files) {
  std::vector<QVariantMap> next;
  next.reserve(static_cast<std::size_t>(files.size()));
  for (const QVariant& item : files) {
    next.push_back(item.toMap());
  }

  if (next == m_files) {
    return false;
  }

  const std::size_t commonCount = std::min(next.size(), m_files.size());
  bool sameIdentityPrefix = true;
  for (std::size_t i = 0; i < commonCount; ++i) {
    if (identityFor(next[i]) != identityFor(m_files[i])) {
      sameIdentityPrefix = false;
      break;
    }
  }

  if (!sameIdentityPrefix) {
    replaceAll(std::move(next));
    return true;
  }

  bool changed = false;
  for (std::size_t i = 0; i < commonCount; ++i) {
    if (m_files[i] == next[i]) {
      continue;
    }
    m_files[i] = next[i];
    const QModelIndex changedIndex = index(static_cast<int>(i), 0);
    emit dataChanged(changedIndex, changedIndex, printerFileRoleNames().keys());
    changed = true;
  }

  if (next.size() > m_files.size()) {
    const int first = static_cast<int>(m_files.size());
    const int last = static_cast<int>(next.size()) - 1;
    beginInsertRows(QModelIndex(), first, last);
    for (std::size_t i = m_files.size(); i < next.size(); ++i) {
      m_files.push_back(std::move(next[i]));
    }
    endInsertRows();
    emit countChanged();
    return true;
  }

  if (next.size() < m_files.size()) {
    const int first = static_cast<int>(next.size());
    const int last = static_cast<int>(m_files.size()) - 1;
    beginRemoveRows(QModelIndex(), first, last);
    m_files.resize(next.size());
    endRemoveRows();
    emit countChanged();
    return true;
  }

  return changed;
}

bool PrinterFilesModel::remove(int row) {
  if (row < 0 || row >= rowCount()) {
    return false;
  }
  beginRemoveRows(QModelIndex(), row, row);
  m_files.erase(m_files.begin() + row);
  endRemoveRows();
  emit countChanged();
  return true;
}

void PrinterFilesModel::clear() {
  if (m_files.empty()) {
    return;
  }
  beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
  m_files.clear();
  endRemoveRows();
  emit countChanged();
}

QString PrinterFilesModel::identityFor(const QVariantMap& file) {
  const QString fileId = file.value(QStringLiteral("fileId")).toString().trimmed();
  if (!fileId.isEmpty()) {
    return fileId;
  }
  return file.value(QStringLiteral("fileName")).toString().trimmed();
}

QVariant PrinterFilesModel::valueForRole(const QVariantMap& file, int role) {
  switch (role) {
    case FileIdRole:
      return file.value(QStringLiteral("fileId"));
    case FileNameRole:
      return file.value(QStringLiteral("fileName"));
    case SizeTextRole:
      return file.value(QStringLiteral("sizeText"));
    case StatusRole:
      return file.value(QStringLiteral("status"));
    case PrintTimeRole:
      return file.value(QStringLiteral("printTime"));
    case ResinUsageRole:
      return file.value(QStringLiteral("resinUsage"));
    case MachineRole:
      return file.value(QStringLiteral("machine"));
    case MachineTypeRole:
      return file.value(QStringLiteral("machineType"));
    default:
      return {};
  }
}

void PrinterFilesModel::replaceAll(std::vector<QVariantMap> files) {
  const int previousCount = count();
  beginResetModel();
  m_files = std::move(files);
  endResetModel();
  if (previousCount != count()) {
    emit countChanged();
  }
}

}  // namespace accloud
