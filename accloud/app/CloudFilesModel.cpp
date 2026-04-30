#include "CloudFilesModel.h"

#include <QSet>

#include <algorithm>

namespace accloud {

namespace {

const QHash<int, QByteArray>& cloudFileRoleNames() {
  static const QHash<int, QByteArray> kRoles{
      {CloudFilesModel::FileIdRole, "fileId"},
      {CloudFilesModel::FileNameRole, "fileName"},
      {CloudFilesModel::ThumbnailUrlRole, "thumbnailUrl"},
      {CloudFilesModel::SizeTextRole, "sizeText"},
      {CloudFilesModel::FileTypeTextRole, "fileTypeText"},
      {CloudFilesModel::DateTextRole, "dateText"},
      {CloudFilesModel::StatusRole, "status"},
  };
  return kRoles;
}

}  // namespace

CloudFilesModel::CloudFilesModel(QObject* parent) : QAbstractListModel(parent) {}

int CloudFilesModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_visibleRows.size());
}

QVariant CloudFilesModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }
  const int sourceIndex = m_visibleRows[static_cast<std::size_t>(index.row())];
  const Row& row = m_all[static_cast<std::size_t>(sourceIndex)];
  switch (role) {
    case FileIdRole:
      return row.display.value(QStringLiteral("fileId"));
    case FileNameRole:
      return row.display.value(QStringLiteral("fileName"));
    case ThumbnailUrlRole:
      return row.display.value(QStringLiteral("thumbnailUrl"));
    case SizeTextRole:
      return row.display.value(QStringLiteral("sizeText"));
    case FileTypeTextRole:
      return row.display.value(QStringLiteral("fileTypeText"));
    case DateTextRole:
      return row.display.value(QStringLiteral("dateText"));
    case StatusRole:
      return row.display.value(QStringLiteral("status"));
    default:
      return {};
  }
}

QHash<int, QByteArray> CloudFilesModel::roleNames() const {
  return cloudFileRoleNames();
}

int CloudFilesModel::count() const {
  return rowCount();
}

int CloudFilesModel::visibleCount() const {
  return m_visibleCount;
}

int CloudFilesModel::totalPages() const {
  return m_totalPages;
}

int CloudFilesModel::currentPage() const {
  return m_currentPage;
}

int CloudFilesModel::pageSize() const {
  return m_pageSize;
}

QString CloudFilesModel::typeFilter() const {
  return m_typeFilter;
}

void CloudFilesModel::setCurrentPage(int value) {
  const int next = std::max(0, std::min(value, std::max(1, m_totalPages) - 1));
  if (m_currentPage == next) {
    return;
  }
  m_currentPage = next;
  emit currentPageChanged();
  rebuildVisibleReset();
}

void CloudFilesModel::setPageSize(int value) {
  const int next = std::max(1, value);
  if (m_pageSize == next) {
    return;
  }
  m_pageSize = next;
  emit pageSizeChanged();
  rebuildVisibleReset();
}

void CloudFilesModel::setTypeFilter(const QString& value) {
  const QString next = value.trimmed().isEmpty() ? QStringLiteral("all") : value.trimmed().toLower();
  if (m_typeFilter == next) {
    return;
  }
  m_typeFilter = next;
  m_currentPage = 0;
  emit typeFilterChanged();
  emit currentPageChanged();
  rebuildVisibleReset();
}

QVariantMap CloudFilesModel::get(int row) const {
  if (row < 0 || row >= rowCount()) {
    return {};
  }
  const int sourceIndex = m_visibleRows[static_cast<std::size_t>(row)];
  return m_all[static_cast<std::size_t>(sourceIndex)].display;
}

QVariantMap CloudFilesModel::fileDataById(const QString& fileId) const {
  const QString normalized = fileId.trimmed();
  for (const Row& row : m_all) {
    if (row.raw.value(QStringLiteral("fileId")).toString() == normalized) {
      return row.raw;
    }
  }
  return {};
}

QStringList CloudFilesModel::availableFileTypes(const QVariantList& supportedExtensions) const {
  QSet<QString> supported;
  for (const QVariant& item : supportedExtensions) {
    const QString ext = item.toString().trimmed().toLower();
    if (!ext.isEmpty()) {
      supported.insert(ext);
    }
  }

  QSet<QString> present;
  for (const Row& row : m_all) {
    const QString ext = fileExtension(row.raw.value(QStringLiteral("fileName")).toString());
    if (!ext.isEmpty() && supported.contains(ext)) {
      present.insert(ext);
    }
  }
  QStringList out = present.values();
  out.sort(Qt::CaseInsensitive);
  return out;
}

void CloudFilesModel::replaceFiles(const QVariantList& files) {
  const int previousAllCount = static_cast<int>(m_all.size());
  beginResetModel();
  m_all.clear();
  m_all.reserve(static_cast<std::size_t>(files.size()));
  for (const QVariant& item : files) {
    QVariantMap raw = item.toMap();
    const QString fileName = raw.value(QStringLiteral("fileName"), QStringLiteral("-")).toString();
    QVariantMap display;
    display.insert(QStringLiteral("fileId"), raw.value(QStringLiteral("fileId")).toString());
    display.insert(QStringLiteral("fileName"), fileName);
    display.insert(QStringLiteral("thumbnailUrl"), raw.value(QStringLiteral("thumbnailUrl")).toString());
    display.insert(QStringLiteral("sizeText"), raw.value(QStringLiteral("sizeText"), QStringLiteral("-")).toString());
    display.insert(QStringLiteral("fileTypeText"), fileTypeText(fileName));
    display.insert(QStringLiteral("dateText"), dateText(raw));
    display.insert(QStringLiteral("status"), raw.value(QStringLiteral("status")));
    m_all.push_back(Row{std::move(raw), std::move(display)});
  }
  rebuildVisible();
  endResetModel();
  if (previousAllCount != static_cast<int>(m_all.size())) {
    emit countChanged();
  }
  emit visibleChanged();
}

void CloudFilesModel::append(const QVariantMap& file) {
  QVariantList files;
  files.reserve(static_cast<qsizetype>(m_all.size()) + 1);
  for (const Row& row : m_all) {
    files.append(row.raw);
  }
  files.append(file);
  replaceFiles(files);
}

void CloudFilesModel::clear() {
  replaceFiles({});
}

QString CloudFilesModel::fileExtension(const QString& fileName) {
  const int dot = fileName.lastIndexOf('.');
  if (dot < 0 || dot + 1 >= fileName.size()) {
    return {};
  }
  return fileName.sliced(dot + 1).toLower();
}

QString CloudFilesModel::fileTypeText(const QString& fileName) {
  const QString ext = fileExtension(fileName);
  return ext.isEmpty() ? QStringLiteral("-") : ext.toUpper();
}

QString CloudFilesModel::dateText(const QVariantMap& file) {
  const QString value = file.value(QStringLiteral("uploadTime")).toString().trimmed();
  return value.isEmpty() ? QStringLiteral("-") : value;
}

bool CloudFilesModel::matchesFilter(const Row& row) const {
  if (m_typeFilter == QStringLiteral("all")) {
    return true;
  }
  return fileExtension(row.raw.value(QStringLiteral("fileName")).toString()) == m_typeFilter;
}

void CloudFilesModel::rebuildVisible() {
  std::vector<int> matching;
  matching.reserve(m_all.size());
  for (int i = 0; i < static_cast<int>(m_all.size()); ++i) {
    if (matchesFilter(m_all[static_cast<std::size_t>(i)])) {
      matching.push_back(i);
    }
  }
  m_visibleCount = static_cast<int>(matching.size());
  m_totalPages = m_visibleCount <= 0 ? 1 : std::max(1, (m_visibleCount + m_pageSize - 1) / m_pageSize);
  clampCurrentPage();
  const int start = m_currentPage * m_pageSize;
  const int end = std::min(static_cast<int>(matching.size()), start + m_pageSize);
  m_visibleRows.assign(matching.begin() + std::min(start, static_cast<int>(matching.size())),
                       matching.begin() + end);
}

void CloudFilesModel::rebuildVisibleReset() {
  beginResetModel();
  rebuildVisible();
  endResetModel();
  emit countChanged();
  emit visibleChanged();
}

void CloudFilesModel::clampCurrentPage() {
  const int clamped = std::max(0, std::min(m_currentPage, std::max(1, m_totalPages) - 1));
  if (clamped != m_currentPage) {
    m_currentPage = clamped;
    emit currentPageChanged();
  }
}

}  // namespace accloud
