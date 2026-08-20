#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QProcess;

namespace accloud {

class SupportAnalysisBridge final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool running READ running NOTIFY runningChanged)
  Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
  Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
  Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourcePathChanged)
  Q_PROPERTY(QString bundlePath READ bundlePath NOTIFY bundlePathChanged)
  Q_PROPERTY(QString computeMode READ computeMode WRITE setComputeMode NOTIFY computeModeChanged)
  Q_PROPERTY(int workerCount READ workerCount WRITE setWorkerCount NOTIFY workerCountChanged)
  Q_PROPERTY(QString analyzedComputeMode READ analyzedComputeMode NOTIFY bundleChanged)
  Q_PROPERTY(bool vulkanCompiled READ vulkanCompiled NOTIFY computeStatusChanged)
  Q_PROPERTY(bool vulkanActive READ vulkanActive NOTIFY computeStatusChanged)
  Q_PROPERTY(QString computeBackend READ computeBackend NOTIFY computeStatusChanged)
  Q_PROPERTY(QString vulkanDevice READ vulkanDevice NOTIFY computeStatusChanged)
  Q_PROPERTY(QString computeDiagnostic READ computeDiagnostic NOTIFY computeStatusChanged)
  Q_PROPERTY(qulonglong vulkanGpuJobs READ vulkanGpuJobs NOTIFY bundleChanged)
  Q_PROPERTY(qulonglong vulkanCpuFallbackJobs READ vulkanCpuFallbackJobs NOTIFY bundleChanged)
  Q_PROPERTY(qulonglong vulkanDispatches READ vulkanDispatches NOTIFY bundleChanged)
  Q_PROPERTY(qulonglong vulkanDispatchFailures READ vulkanDispatchFailures NOTIFY bundleChanged)
  Q_PROPERTY(int preparationWindow READ preparationWindow NOTIFY bundleChanged)
  Q_PROPERTY(int maximumPreparationInflight READ maximumPreparationInflight NOTIFY bundleChanged)
  Q_PROPERTY(int layerCount READ layerCount NOTIFY bundleChanged)
  Q_PROPERTY(int currentLayer READ currentLayer WRITE setCurrentLayer NOTIFY currentLayerChanged)
  Q_PROPERTY(QUrl currentImageUrl READ currentImageUrl NOTIFY currentLayerChanged)
  Q_PROPERTY(QUrl currentRawImageUrl READ currentRawImageUrl NOTIFY currentLayerChanged)
  Q_PROPERTY(QUrl currentSemanticImageUrl READ currentSemanticImageUrl NOTIFY currentLayerChanged)
  Q_PROPERTY(QUrl currentNodesImageUrl READ currentNodesImageUrl NOTIFY currentLayerChanged)
  Q_PROPERTY(QString currentLayerJson READ currentLayerJson NOTIFY currentLayerChanged)
  Q_PROPERTY(QString currentDecisionJson READ currentDecisionJson NOTIFY decisionSelectionChanged)
  Q_PROPERTY(QString analysisJson READ analysisJson NOTIFY bundleChanged)
  Q_PROPERTY(int selectedDecisionIndex READ selectedDecisionIndex NOTIFY decisionSelectionChanged)
  Q_PROPERTY(qint64 selectedNodeId READ selectedNodeId NOTIFY decisionSelectionChanged)
  Q_PROPERTY(QString selectedSemantic READ selectedSemantic NOTIFY decisionSelectionChanged)
  Q_PROPERTY(QString selectedRegionId READ selectedRegionId NOTIFY decisionSelectionChanged)
  Q_PROPERTY(QString selectedLineageId READ selectedLineageId NOTIFY decisionSelectionChanged)
  Q_PROPERTY(QString selectedRegionSemantic READ selectedRegionSemantic NOTIFY decisionSelectionChanged)
  Q_PROPERTY(QVariantMap selectedRegion READ selectedRegion NOTIFY decisionSelectionChanged)

public:
  explicit SupportAnalysisBridge(QObject* parent = nullptr);

  [[nodiscard]] bool running() const noexcept;
  [[nodiscard]] double progress() const noexcept;
  [[nodiscard]] QString phase() const;
  [[nodiscard]] QString errorString() const;
  [[nodiscard]] QString sourcePath() const;
  [[nodiscard]] QString bundlePath() const;
  [[nodiscard]] QString computeMode() const;
  [[nodiscard]] int workerCount() const noexcept;
  [[nodiscard]] QString analyzedComputeMode() const;
  [[nodiscard]] bool vulkanCompiled() const noexcept;
  [[nodiscard]] bool vulkanActive() const noexcept;
  [[nodiscard]] QString computeBackend() const;
  [[nodiscard]] QString vulkanDevice() const;
  [[nodiscard]] QString computeDiagnostic() const;
  [[nodiscard]] qulonglong vulkanGpuJobs() const noexcept;
  [[nodiscard]] qulonglong vulkanCpuFallbackJobs() const noexcept;
  [[nodiscard]] qulonglong vulkanDispatches() const noexcept;
  [[nodiscard]] qulonglong vulkanDispatchFailures() const noexcept;
  [[nodiscard]] int preparationWindow() const noexcept;
  [[nodiscard]] int maximumPreparationInflight() const noexcept;
  [[nodiscard]] int layerCount() const noexcept;
  [[nodiscard]] int currentLayer() const noexcept;
  [[nodiscard]] QUrl currentImageUrl() const;
  [[nodiscard]] QUrl currentRawImageUrl() const;
  [[nodiscard]] QUrl currentSemanticImageUrl() const;
  [[nodiscard]] QUrl currentNodesImageUrl() const;
  [[nodiscard]] QString currentLayerJson() const;
  [[nodiscard]] QString currentDecisionJson() const;
  [[nodiscard]] QString analysisJson() const;
  [[nodiscard]] int selectedDecisionIndex() const noexcept;
  [[nodiscard]] qint64 selectedNodeId() const noexcept;
  [[nodiscard]] QString selectedSemantic() const;
  [[nodiscard]] QString selectedRegionId() const;
  [[nodiscard]] QString selectedLineageId() const;
  [[nodiscard]] QString selectedRegionSemantic() const;
  [[nodiscard]] QVariantMap selectedRegion() const;

  Q_INVOKABLE void analyze(const QString& localPath);
  void setComputeMode(const QString& mode);
  void setWorkerCount(int count);
  Q_INVOKABLE bool openBundle(const QString& localPath);
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void setCurrentLayer(int oneBasedLayer);
  Q_INVOKABLE bool selectCurrentComponent(double normalizedX, double normalizedY);
  Q_INVOKABLE void clearDecisionSelection();

signals:
  void runningChanged();
  void progressChanged();
  void phaseChanged();
  void errorStringChanged();
  void sourcePathChanged();
  void bundlePathChanged();
  void computeModeChanged();
  void workerCountChanged();
  void computeStatusChanged();
  void bundleChanged();
  void currentLayerChanged();
  void decisionSelectionChanged();

private:
  void setRunning(bool value);
  void setProgress(double value);
  void setPhase(const QString& value);
  void setErrorString(const QString& value);
  void resetBundle();
  void consumeProcessOutput();
  void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  bool loadCurrentLayerData();
  void resetComputeStatus();
  void updateComputeStatus(bool compiled, bool active, const QString& backend,
                           const QString& device, const QString& diagnostic);
  [[nodiscard]] QJsonObject summaryObject() const;
  [[nodiscard]] QJsonObject optionsObject() const;
  [[nodiscard]] QUrl currentDiagnosticImageUrl(const QString& panel) const;
  [[nodiscard]] QString currentDiagnosticPath(const QString& panel) const;
  [[nodiscard]] QString probeExecutable() const;
  [[nodiscard]] static QString normalizeLocalPath(const QString& value);
  [[nodiscard]] static QString prettyJson(const QJsonObject& object);
  [[nodiscard]] static QString prettyJson(const QJsonArray& array);

  QProcess m_process;
  bool m_running = false;
  double m_progress = 0.0;
  QString m_phase;
  QString m_errorString;
  QString m_sourcePath;
  QString m_bundlePath;
  QString m_computeMode = QStringLiteral("auto");
  int m_workerCount = 4;
  bool m_vulkanCompiled = false;
  bool m_vulkanActive = false;
  QString m_computeBackend = QStringLiteral("cpu");
  QString m_vulkanDevice;
  QString m_computeDiagnostic;
  int m_layerCount = 0;
  int m_currentLayer = 1;
  int m_selectedDecisionIndex = -1;
  QVariantMap m_selectedRegionOverride;
  QJsonObject m_selectedSemanticRegion;
  QJsonObject m_manifest;
  QJsonObject m_analysis;
  QJsonObject m_currentLayerData;
  QString m_stderrBuffer;
};

} // namespace accloud
