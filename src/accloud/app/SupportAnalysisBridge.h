#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QUrl>

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
  Q_PROPERTY(int layerCount READ layerCount NOTIFY bundleChanged)
  Q_PROPERTY(int currentLayer READ currentLayer WRITE setCurrentLayer NOTIFY currentLayerChanged)
  Q_PROPERTY(QUrl currentImageUrl READ currentImageUrl NOTIFY currentLayerChanged)
  Q_PROPERTY(QString currentLayerJson READ currentLayerJson NOTIFY currentLayerChanged)
  Q_PROPERTY(QString currentDecisionJson READ currentDecisionJson NOTIFY currentLayerChanged)
  Q_PROPERTY(QString analysisJson READ analysisJson NOTIFY bundleChanged)

public:
  explicit SupportAnalysisBridge(QObject* parent = nullptr);

  [[nodiscard]] bool running() const noexcept;
  [[nodiscard]] double progress() const noexcept;
  [[nodiscard]] QString phase() const;
  [[nodiscard]] QString errorString() const;
  [[nodiscard]] QString sourcePath() const;
  [[nodiscard]] QString bundlePath() const;
  [[nodiscard]] int layerCount() const noexcept;
  [[nodiscard]] int currentLayer() const noexcept;
  [[nodiscard]] QUrl currentImageUrl() const;
  [[nodiscard]] QString currentLayerJson() const;
  [[nodiscard]] QString currentDecisionJson() const;
  [[nodiscard]] QString analysisJson() const;

  Q_INVOKABLE void analyze(const QString& localPath);
  Q_INVOKABLE bool openBundle(const QString& localPath);
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void setCurrentLayer(int oneBasedLayer);

signals:
  void runningChanged();
  void progressChanged();
  void phaseChanged();
  void errorStringChanged();
  void sourcePathChanged();
  void bundlePathChanged();
  void bundleChanged();
  void currentLayerChanged();

private:
  void setRunning(bool value);
  void setProgress(double value);
  void setPhase(const QString& value);
  void setErrorString(const QString& value);
  void resetBundle();
  void consumeProcessOutput();
  void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  bool loadCurrentLayerData();
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
  int m_layerCount = 0;
  int m_currentLayer = 1;
  QJsonObject m_manifest;
  QJsonObject m_analysis;
  QJsonObject m_currentLayerData;
  QString m_stderrBuffer;
};

} // namespace accloud
