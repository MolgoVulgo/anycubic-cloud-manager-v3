#include "app/SupportAnalysisBridge.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QImage>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace accloud {
namespace {

bool readJsonObject(const QString& path, QJsonObject& output, QString& error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    error = SupportAnalysisBridge::tr("Cannot open JSON file: %1").arg(path);
    return false;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    error = SupportAnalysisBridge::tr("Invalid JSON file %1: %2")
                .arg(path, parseError.errorString());
    return false;
  }
  output = document.object();
  return true;
}

} // namespace

SupportAnalysisBridge::SupportAnalysisBridge(QObject* parent)
    : QObject(parent) {
  m_process.setProcessChannelMode(QProcess::SeparateChannels);
  connect(&m_process, &QProcess::readyReadStandardError,
          this, &SupportAnalysisBridge::consumeProcessOutput);
  connect(&m_process, &QProcess::readyReadStandardOutput,
          this, &SupportAnalysisBridge::consumeProcessOutput);
  connect(&m_process,
          qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, &SupportAnalysisBridge::handleProcessFinished);
  connect(&m_process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError) {
            if (!m_running) {
              return;
            }
            setErrorString(m_process.errorString());
            setRunning(false);
          });
}

bool SupportAnalysisBridge::running() const noexcept { return m_running; }
double SupportAnalysisBridge::progress() const noexcept { return m_progress; }
QString SupportAnalysisBridge::phase() const { return m_phase; }
QString SupportAnalysisBridge::errorString() const { return m_errorString; }
QString SupportAnalysisBridge::sourcePath() const { return m_sourcePath; }
QString SupportAnalysisBridge::bundlePath() const { return m_bundlePath; }
int SupportAnalysisBridge::layerCount() const noexcept { return m_layerCount; }
int SupportAnalysisBridge::currentLayer() const noexcept { return m_currentLayer; }

QString SupportAnalysisBridge::currentDiagnosticPath(
    const QString& panel) const {
  if (m_bundlePath.isEmpty() || m_layerCount <= 0) {
    return {};
  }

  const auto diagnostic = m_currentLayerData
                              .value(QStringLiteral("diagnostic"))
                              .toObject();
  const auto relativeFromLayer = diagnostic
                                     .value(QStringLiteral("images"))
                                     .toObject()
                                     .value(panel)
                                     .toString();
  if (!relativeFromLayer.isEmpty()) {
    return QDir(m_bundlePath).filePath(relativeFromLayer);
  }

  const auto images = m_manifest.value(QStringLiteral("images")).toArray();
  if (m_currentLayer < 1 || m_currentLayer > images.size()) {
    return {};
  }
  const auto entry = images.at(m_currentLayer - 1).toObject();
  QString field;
  if (panel == QStringLiteral("raw_mask")) {
    field = QStringLiteral("raw_path");
  } else if (panel == QStringLiteral("semantic_result")) {
    field = QStringLiteral("semantic_path");
  } else if (panel == QStringLiteral("decision_nodes")) {
    field = QStringLiteral("nodes_path");
  } else if (panel == QStringLiteral("pick_map")) {
    field = QStringLiteral("pick_path");
  }
  auto relative = entry.value(field).toString();
  if (relative.isEmpty() && panel == QStringLiteral("decision_nodes")) {
    relative = entry.value(QStringLiteral("path")).toString();
  }
  if (relative.isEmpty()) {
    return {};
  }
  return QDir(m_bundlePath).filePath(relative);
}

QUrl SupportAnalysisBridge::currentDiagnosticImageUrl(
    const QString& panel) const {
  const auto path = currentDiagnosticPath(panel);
  return path.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path);
}

QUrl SupportAnalysisBridge::currentImageUrl() const {
  return currentNodesImageUrl();
}

QUrl SupportAnalysisBridge::currentRawImageUrl() const {
  auto url = currentDiagnosticImageUrl(QStringLiteral("raw_mask"));
  if (url.isEmpty()) {
    url = currentNodesImageUrl();
  }
  return url;
}

QUrl SupportAnalysisBridge::currentSemanticImageUrl() const {
  auto url = currentDiagnosticImageUrl(QStringLiteral("semantic_result"));
  if (url.isEmpty()) {
    url = currentNodesImageUrl();
  }
  return url;
}

QUrl SupportAnalysisBridge::currentNodesImageUrl() const {
  return currentDiagnosticImageUrl(QStringLiteral("decision_nodes"));
}

QString SupportAnalysisBridge::currentLayerJson() const {
  if (m_currentLayerData.isEmpty()) {
    return QStringLiteral("{}");
  }
  auto layer = m_currentLayerData;
  layer.insert(QStringLiteral("raw_image_absolute"),
               currentRawImageUrl().toLocalFile());
  layer.insert(QStringLiteral("semantic_image_absolute"),
               currentSemanticImageUrl().toLocalFile());
  layer.insert(QStringLiteral("nodes_image_absolute"),
               currentNodesImageUrl().toLocalFile());
  return prettyJson(layer);
}

QString SupportAnalysisBridge::currentDecisionJson() const {
  const auto decisions = m_currentLayerData
                             .value(QStringLiteral("decisions"))
                             .toArray();
  if (m_selectedDecisionIndex >= 0
      && m_selectedDecisionIndex < decisions.size()) {
    return prettyJson(decisions.at(m_selectedDecisionIndex).toObject());
  }
  return prettyJson(decisions);
}

QString SupportAnalysisBridge::analysisJson() const {
  return prettyJson(m_analysis);
}

int SupportAnalysisBridge::selectedDecisionIndex() const noexcept {
  return m_selectedDecisionIndex;
}

qint64 SupportAnalysisBridge::selectedNodeId() const noexcept {
  const auto decisions = m_currentLayerData
                             .value(QStringLiteral("decisions"))
                             .toArray();
  if (m_selectedDecisionIndex < 0
      || m_selectedDecisionIndex >= decisions.size()) {
    return -1;
  }
  const auto value = decisions.at(m_selectedDecisionIndex)
                         .toObject()
                         .value(QStringLiteral("node_id"));
  return value.isDouble() ? static_cast<qint64>(value.toDouble()) : -1;
}

QString SupportAnalysisBridge::selectedSemantic() const {
  const auto decisions = m_currentLayerData
                             .value(QStringLiteral("decisions"))
                             .toArray();
  if (m_selectedDecisionIndex < 0
      || m_selectedDecisionIndex >= decisions.size()) {
    return {};
  }
  return decisions.at(m_selectedDecisionIndex)
      .toObject()
      .value(QStringLiteral("choice"))
      .toString();
}

QVariantMap SupportAnalysisBridge::selectedRegion() const {
  QVariantMap region{{QStringLiteral("valid"), false}};
  const auto decisions = m_currentLayerData
                             .value(QStringLiteral("decisions"))
                             .toArray();
  if (m_selectedDecisionIndex < 0
      || m_selectedDecisionIndex >= decisions.size()) {
    return region;
  }
  const auto diagnostic = m_currentLayerData
                              .value(QStringLiteral("diagnostic"))
                              .toObject();
  const auto sourceBounds = diagnostic
                                .value(QStringLiteral("source_bounds_pixels"))
                                .toObject();
  const auto decisionBounds = decisions.at(m_selectedDecisionIndex)
                                  .toObject()
                                  .value(QStringLiteral("bounds_pixels"))
                                  .toObject();
  const auto sourceMinX = sourceBounds.value(QStringLiteral("min_x")).toDouble();
  const auto sourceMinY = sourceBounds.value(QStringLiteral("min_y")).toDouble();
  const auto sourceMaxX = sourceBounds.value(QStringLiteral("max_x")).toDouble();
  const auto sourceMaxY = sourceBounds.value(QStringLiteral("max_y")).toDouble();
  const auto sourceWidth = sourceMaxX - sourceMinX;
  const auto sourceHeight = sourceMaxY - sourceMinY;
  if (sourceWidth <= 0.0 || sourceHeight <= 0.0) {
    return region;
  }
  const auto x = std::clamp(
      (decisionBounds.value(QStringLiteral("min_x")).toDouble() - sourceMinX)
          / sourceWidth,
      0.0, 1.0);
  const auto y = std::clamp(
      (decisionBounds.value(QStringLiteral("min_y")).toDouble() - sourceMinY)
          / sourceHeight,
      0.0, 1.0);
  const auto right = std::clamp(
      (decisionBounds.value(QStringLiteral("max_x")).toDouble() - sourceMinX)
          / sourceWidth,
      0.0, 1.0);
  const auto bottom = std::clamp(
      (decisionBounds.value(QStringLiteral("max_y")).toDouble() - sourceMinY)
          / sourceHeight,
      0.0, 1.0);
  region.insert(QStringLiteral("valid"), true);
  region.insert(QStringLiteral("x"), x);
  region.insert(QStringLiteral("y"), y);
  region.insert(QStringLiteral("width"), std::max(0.0, right - x));
  region.insert(QStringLiteral("height"), std::max(0.0, bottom - y));
  return region;
}

bool SupportAnalysisBridge::selectCurrentComponent(
    double normalizedX,
    double normalizedY) {
  if (!std::isfinite(normalizedX) || !std::isfinite(normalizedY)
      || normalizedX < 0.0 || normalizedX >= 1.0
      || normalizedY < 0.0 || normalizedY >= 1.0) {
    clearDecisionSelection();
    return false;
  }
  const auto pickPath = currentDiagnosticPath(QStringLiteral("pick_map"));
  QImage pickMap(pickPath);
  if (pickMap.isNull() || pickMap.width() <= 0 || pickMap.height() <= 0) {
    clearDecisionSelection();
    return false;
  }
  const auto x = std::clamp(
      static_cast<int>(std::floor(normalizedX * pickMap.width())),
      0, pickMap.width() - 1);
  const auto y = std::clamp(
      static_cast<int>(std::floor(normalizedY * pickMap.height())),
      0, pickMap.height() - 1);
  const auto pixel = pickMap.pixel(x, y);
  const auto encoded = (static_cast<std::uint32_t>(qRed(pixel)) << 16u)
                       | (static_cast<std::uint32_t>(qGreen(pixel)) << 8u)
                       | static_cast<std::uint32_t>(qBlue(pixel));
  if (encoded == 0u) {
    clearDecisionSelection();
    return false;
  }
  const auto componentId = static_cast<int>(encoded - 1u);
  const auto decisions = m_currentLayerData
                             .value(QStringLiteral("decisions"))
                             .toArray();
  for (int index = 0; index < decisions.size(); ++index) {
    if (decisions.at(index)
            .toObject()
            .value(QStringLiteral("component_id"))
            .toInt(-1) != componentId) {
      continue;
    }
    if (m_selectedDecisionIndex != index) {
      m_selectedDecisionIndex = index;
      emit decisionSelectionChanged();
    }
    return true;
  }
  clearDecisionSelection();
  return false;
}

void SupportAnalysisBridge::clearDecisionSelection() {
  if (m_selectedDecisionIndex < 0) {
    return;
  }
  m_selectedDecisionIndex = -1;
  emit decisionSelectionChanged();
}

void SupportAnalysisBridge::analyze(const QString& localPath) {
  if (m_running) {
    return;
  }
  const auto path = normalizeLocalPath(localPath);
  const QFileInfo source(path);
  if (!source.exists() || !source.isFile()
      || source.suffix().compare(QStringLiteral("pwsz"), Qt::CaseInsensitive) != 0) {
    setErrorString(tr("Select an existing .pwsz file."));
    return;
  }

  const auto executable = probeExecutable();
  if (!QFileInfo::exists(executable)) {
    setErrorString(tr("Support-analysis probe not found: %1")
                       .arg(executable));
    return;
  }

  resetBundle();
  setErrorString({});
  m_sourcePath = source.absoluteFilePath();
  emit sourcePathChanged();

  const auto baseDirectory = QStandardPaths::writableLocation(
      QStandardPaths::AppLocalDataLocation);
  const auto stamp = QDateTime::currentDateTimeUtc().toString(
      QStringLiteral("yyyyMMdd-HHmmss-zzz"));
  const auto safeBaseName = source.completeBaseName().replace(
      QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")),
      QStringLiteral("_"));
  m_bundlePath = QDir(baseDirectory).filePath(
      QStringLiteral("support-analysis/%1-%2").arg(safeBaseName, stamp));
  QDir().mkpath(m_bundlePath);
  emit bundlePathChanged();

  m_stderrBuffer.clear();
  setProgress(0.0);
  setPhase(tr("Analyzing native layers"));
  setRunning(true);

  m_process.setProgram(executable);
  m_process.setArguments({
      m_sourcePath,
      QStringLiteral("--bundle"), m_bundlePath,
      QStringLiteral("--downsample"), QStringLiteral("16"),
      QStringLiteral("--verify-materialization"),
  });
  m_process.start();
}

bool SupportAnalysisBridge::openBundle(const QString& localPath) {
  const auto path = normalizeLocalPath(localPath);
  const QDir directory(path);
  if (!directory.exists()) {
    setErrorString(tr("Analysis bundle directory does not exist."));
    return false;
  }

  QJsonObject manifest;
  QJsonObject analysis;
  QString error;
  if (!readJsonObject(directory.filePath(QStringLiteral("manifest.json")),
                      manifest, error)
      || !readJsonObject(directory.filePath(QStringLiteral("summary.json")),
                         analysis, error)) {
    setErrorString(error);
    return false;
  }

  m_manifest = manifest;
  m_analysis = analysis;

  m_bundlePath = directory.absolutePath();
  m_sourcePath = manifest.value(QStringLiteral("source_path")).toString();
  m_layerCount = manifest.value(QStringLiteral("layer_count")).toInt();
  m_currentLayer = std::clamp(m_currentLayer, 1, std::max(1, m_layerCount));
  m_selectedDecisionIndex = -1;
  if (!loadCurrentLayerData()) {
    return false;
  }
  setErrorString({});
  setProgress(1.0);
  setPhase(tr("Analysis bundle ready"));
  emit bundlePathChanged();
  emit sourcePathChanged();
  emit bundleChanged();
  emit currentLayerChanged();
  emit decisionSelectionChanged();
  return true;
}

void SupportAnalysisBridge::cancel() {
  if (!m_running) {
    return;
  }
  setRunning(false);
  m_process.kill();
  m_process.waitForFinished(1000);
  setPhase(tr("Analysis cancelled"));
}

void SupportAnalysisBridge::setCurrentLayer(int oneBasedLayer) {
  const auto normalized = std::clamp(oneBasedLayer, 1, std::max(1, m_layerCount));
  if (normalized == m_currentLayer) {
    return;
  }
  m_currentLayer = normalized;
  m_selectedDecisionIndex = -1;
  if (!loadCurrentLayerData()) {
    return;
  }
  emit currentLayerChanged();
  emit decisionSelectionChanged();
}

void SupportAnalysisBridge::setRunning(bool value) {
  if (m_running == value) {
    return;
  }
  m_running = value;
  emit runningChanged();
}

void SupportAnalysisBridge::setProgress(double value) {
  value = std::clamp(value, 0.0, 1.0);
  if (qFuzzyCompare(m_progress, value)) {
    return;
  }
  m_progress = value;
  emit progressChanged();
}

void SupportAnalysisBridge::setPhase(const QString& value) {
  if (m_phase == value) {
    return;
  }
  m_phase = value;
  emit phaseChanged();
}

void SupportAnalysisBridge::setErrorString(const QString& value) {
  if (m_errorString == value) {
    return;
  }
  m_errorString = value;
  emit errorStringChanged();
}

void SupportAnalysisBridge::resetBundle() {
  m_manifest = {};
  m_analysis = {};
  m_currentLayerData = {};
  m_layerCount = 0;
  m_currentLayer = 1;
  m_selectedDecisionIndex = -1;
  emit bundleChanged();
  emit currentLayerChanged();
  emit decisionSelectionChanged();
}

void SupportAnalysisBridge::consumeProcessOutput() {
  m_stderrBuffer += QString::fromUtf8(m_process.readAllStandardError());
  m_stderrBuffer += QString::fromUtf8(m_process.readAllStandardOutput());
  const auto lines = m_stderrBuffer.split('\n');
  m_stderrBuffer = lines.isEmpty() ? QString() : lines.last();
  for (int index = 0; index + 1 < lines.size(); ++index) {
    const auto fields = lines.at(index).trimmed().split(' ', Qt::SkipEmptyParts);
    if (fields.size() != 3) {
      continue;
    }
    bool okCompleted = false;
    bool okTotal = false;
    const auto completed = fields.at(1).toDouble(&okCompleted);
    const auto total = fields.at(2).toDouble(&okTotal);
    if (!okCompleted || !okTotal || total <= 0.0) {
      continue;
    }
    if (fields.at(0) == QStringLiteral("ANALYZE_PROGRESS")) {
      setPhase(tr("Analyzing native layers"));
      setProgress(0.5 * completed / total);
    } else if (fields.at(0) == QStringLiteral("IMAGE_PROGRESS")) {
      setPhase(tr("Generating diagnostic images"));
      setProgress(0.5 + 0.5 * completed / total);
    }
  }
}

void SupportAnalysisBridge::handleProcessFinished(
    int exitCode,
    QProcess::ExitStatus exitStatus) {
  consumeProcessOutput();
  setRunning(false);
  if (exitStatus != QProcess::NormalExit || exitCode != 0) {
    const auto details = m_stderrBuffer.trimmed();
    setErrorString(details.isEmpty()
                       ? tr("Support analysis failed with exit code %1.")
                             .arg(exitCode)
                       : details);
    setPhase(tr("Analysis failed"));
    return;
  }
  if (!openBundle(m_bundlePath)) {
    setPhase(tr("Bundle loading failed"));
    return;
  }
  setProgress(1.0);
}

bool SupportAnalysisBridge::loadCurrentLayerData() {
  m_currentLayerData = {};
  if (m_bundlePath.isEmpty() || m_currentLayer < 1 || m_layerCount <= 0) {
    return true;
  }
  const auto images = m_manifest.value(QStringLiteral("images")).toArray();
  if (m_currentLayer > images.size()) {
    setErrorString(tr("Selected layer is outside the bundle."));
    return false;
  }
  const auto relative = images.at(m_currentLayer - 1)
                            .toObject()
                            .value(QStringLiteral("layer_json"))
                            .toString();
  if (relative.isEmpty()) {
    setErrorString(tr("Layer JSON reference is missing."));
    return false;
  }
  QString error;
  if (!readJsonObject(QDir(m_bundlePath).filePath(relative),
                      m_currentLayerData, error)) {
    setErrorString(error);
    return false;
  }
  return true;
}

QString SupportAnalysisBridge::probeExecutable() const {
  QString name = QStringLiteral("accloud_support_analysis_probe");
#ifdef Q_OS_WIN
  name += QStringLiteral(".exe");
#endif
  return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

QString SupportAnalysisBridge::normalizeLocalPath(const QString& value) {
  const QUrl url(value);
  if (url.isLocalFile()) {
    return QFileInfo(url.toLocalFile()).absoluteFilePath();
  }
  return QFileInfo(value).absoluteFilePath();
}

QString SupportAnalysisBridge::prettyJson(const QJsonObject& object) {
  return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QString SupportAnalysisBridge::prettyJson(const QJsonArray& array) {
  return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

} // namespace accloud
