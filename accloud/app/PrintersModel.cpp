#include "PrintersModel.h"

namespace accloud {

namespace {

const QHash<int, QByteArray>& printerRoleNames() {
  static const QHash<int, QByteArray> kRoles{
      {PrintersModel::IdRole, "id"},
      {PrintersModel::NameRole, "name"},
      {PrintersModel::ModelRole, "model"},
      {PrintersModel::TypeRole, "type"},
      {PrintersModel::StateRole, "state"},
      {PrintersModel::ReasonRole, "reason"},
      {PrintersModel::AvailableRole, "available"},
      {PrintersModel::ProgressRole, "progress"},
      {PrintersModel::CurrentFileRole, "currentFile"},
      {PrintersModel::ElapsedSecRole, "elapsedSec"},
      {PrintersModel::RemainingSecRole, "remainingSec"},
      {PrintersModel::CurrentLayerRole, "currentLayer"},
      {PrintersModel::TotalLayersRole, "totalLayers"},
      {PrintersModel::LastSeenRole, "lastSeen"},
      {PrintersModel::PrinterKeyRole, "printerKey"},
      {PrintersModel::MachineTypeRole, "machineType"},
      {PrintersModel::DetailsRole, "details"},
      {PrintersModel::ProjectsRole, "projects"},
      {PrintersModel::DetailsRawJsonRole, "detailsRawJson"},
      {PrintersModel::ProjectsRawJsonRole, "projectsRawJson"},
      {PrintersModel::ImgRole, "img"},
      {PrintersModel::ImageRole, "image"},
      {PrintersModel::PreviewRole, "preview"},
      {PrintersModel::ThumbnailUrlRole, "thumbnailUrl"},
  };
  return kRoles;
}

}  // namespace

PrintersModel::PrintersModel(QObject* parent) : QAbstractListModel(parent) {}

int PrintersModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_printers.size());
}

QVariant PrintersModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }
  return valueForRole(m_printers[static_cast<std::size_t>(index.row())], role);
}

QHash<int, QByteArray> PrintersModel::roleNames() const {
  return printerRoleNames();
}

int PrintersModel::count() const {
  return rowCount();
}

QVariantMap PrintersModel::get(int row) const {
  if (row < 0 || row >= rowCount()) {
    return {};
  }
  return m_printers[static_cast<std::size_t>(row)];
}

bool PrintersModel::replaceOrPatchPrinters(const QVariantList& printers) {
  std::vector<QVariantMap> next;
  next.reserve(static_cast<std::size_t>(printers.size()));
  for (const QVariant& item : printers) {
    next.push_back(cleanPrinter(item.toMap()));
  }

  if (next.size() != m_printers.size()) {
    replaceAll(std::move(next));
    return true;
  }

  for (std::size_t i = 0; i < next.size(); ++i) {
    if (idFor(m_printers[i]) != idFor(next[i])) {
      replaceAll(std::move(next));
      return true;
    }
  }

  bool changed = false;
  for (std::size_t i = 0; i < next.size(); ++i) {
    if (fingerprint(m_printers[i]) == fingerprint(next[i])) {
      continue;
    }
    m_printers[i] = std::move(next[i]);
    const QModelIndex changedIndex = index(static_cast<int>(i), 0);
    emit dataChanged(changedIndex, changedIndex, printerRoleNames().keys());
    changed = true;
  }
  return changed;
}

void PrintersModel::clear() {
  if (m_printers.empty()) {
    return;
  }
  beginResetModel();
  m_printers.clear();
  endResetModel();
  emit countChanged();
}

QVariantMap PrintersModel::cleanPrinter(const QVariantMap& source) {
  return source;
}

QString PrintersModel::idFor(const QVariantMap& printer) {
  return printer.value(QStringLiteral("id")).toString().trimmed();
}

QString PrintersModel::fingerprint(const QVariantMap& printer) {
  QStringList values;
  values.reserve(printerRoleNames().size());
  const auto roles = printerRoleNames().keys();
  for (int role : roles) {
    values.push_back(valueForRole(printer, role).toString());
  }
  return values.join(QChar(0x1f));
}

QVariant PrintersModel::valueForRole(const QVariantMap& printer, int role) {
  switch (role) {
    case IdRole:
      return printer.value(QStringLiteral("id"));
    case NameRole:
      return printer.value(QStringLiteral("name"));
    case ModelRole:
      return printer.value(QStringLiteral("model"));
    case TypeRole:
      return printer.value(QStringLiteral("type"));
    case StateRole:
      return printer.value(QStringLiteral("state"));
    case ReasonRole:
      return printer.value(QStringLiteral("reason"));
    case AvailableRole:
      return printer.value(QStringLiteral("available"));
    case ProgressRole:
      return printer.value(QStringLiteral("progress"));
    case CurrentFileRole:
      return printer.value(QStringLiteral("currentFile"));
    case ElapsedSecRole:
      return printer.value(QStringLiteral("elapsedSec"));
    case RemainingSecRole:
      return printer.value(QStringLiteral("remainingSec"));
    case CurrentLayerRole:
      return printer.value(QStringLiteral("currentLayer"));
    case TotalLayersRole:
      return printer.value(QStringLiteral("totalLayers"));
    case LastSeenRole:
      return printer.value(QStringLiteral("lastSeen"));
    case PrinterKeyRole:
      return printer.value(QStringLiteral("printerKey"));
    case MachineTypeRole:
      return printer.value(QStringLiteral("machineType"));
    case DetailsRole:
      return printer.value(QStringLiteral("details"));
    case ProjectsRole:
      return printer.value(QStringLiteral("projects"));
    case DetailsRawJsonRole:
      return printer.value(QStringLiteral("detailsRawJson"));
    case ProjectsRawJsonRole:
      return printer.value(QStringLiteral("projectsRawJson"));
    case ImgRole:
      return printer.value(QStringLiteral("img"));
    case ImageRole:
      return printer.value(QStringLiteral("image"));
    case PreviewRole:
      return printer.value(QStringLiteral("preview"));
    case ThumbnailUrlRole:
      return printer.value(QStringLiteral("thumbnailUrl"));
    default:
      return {};
  }
}

void PrintersModel::replaceAll(std::vector<QVariantMap> printers) {
  const int previousCount = count();
  beginResetModel();
  m_printers = std::move(printers);
  endResetModel();
  if (previousCount != count()) {
    emit countChanged();
  }
}

}  // namespace accloud
