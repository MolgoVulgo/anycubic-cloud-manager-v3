#pragma once

#include "domain/photons/MeshChunk.h"
#include "render3d/core/OrbitCamera.h"
#include "render3d/gl/UploadQueue.h"

#include <QColor>
#include <QQuickFramebufferObject>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

namespace accloud::render3d {

class GlFramebufferRenderer;

class QmlGlItem : public QQuickFramebufferObject {
  Q_OBJECT
  Q_PROPERTY(QString sourcePath READ sourcePath WRITE setSourcePath NOTIFY sourcePathChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
  Q_PROPERTY(QString machineName READ machineName NOTIFY documentChanged)
  Q_PROPERTY(int totalLayers READ totalLayers NOTIFY documentChanged)
  Q_PROPERTY(int firstLayer READ firstLayer WRITE setFirstLayer NOTIFY visibleRangeChanged)
  Q_PROPERTY(int lastLayer READ lastLayer WRITE setLastLayer NOTIFY visibleRangeChanged)
  Q_PROPERTY(qreal layerHeightMm READ layerHeightMm NOTIFY documentChanged)
  Q_PROPERTY(int layerStep READ layerStep WRITE setLayerStep NOTIFY layerStepChanged)
  Q_PROPERTY(int workerCount READ workerCount WRITE setWorkerCount NOTIFY workerCountChanged)
  Q_PROPERTY(int loadedChunkCount READ loadedChunkCount NOTIFY meshStatsChanged)
  Q_PROPERTY(qulonglong triangleCount READ triangleCount NOTIFY meshStatsChanged)
  Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
  Q_PROPERTY(QColor meshColor READ meshColor WRITE setMeshColor NOTIFY meshColorChanged)

public:
  explicit QmlGlItem(QQuickItem* parent = nullptr);
  ~QmlGlItem() override;

  [[nodiscard]] Renderer* createRenderer() const override;

  [[nodiscard]] QString sourcePath() const { return sourcePath_; }
  void setSourcePath(const QString& sourcePath);

  [[nodiscard]] bool loading() const noexcept { return loading_; }
  [[nodiscard]] qreal progress() const noexcept { return progress_; }
  [[nodiscard]] QString errorString() const { return errorString_; }
  [[nodiscard]] QString machineName() const { return machineName_; }
  [[nodiscard]] int totalLayers() const noexcept { return totalLayers_; }
  [[nodiscard]] int firstLayer() const noexcept { return firstLayer_; }
  [[nodiscard]] int lastLayer() const noexcept { return lastLayer_; }
  [[nodiscard]] qreal layerHeightMm() const noexcept { return layerHeightMm_; }
  [[nodiscard]] int layerStep() const noexcept { return layerStep_; }
  [[nodiscard]] int workerCount() const noexcept { return workerCount_; }
  [[nodiscard]] int loadedChunkCount() const noexcept { return loadedChunkCount_; }
  [[nodiscard]] qulonglong triangleCount() const noexcept { return triangleCount_; }
  [[nodiscard]] QColor backgroundColor() const { return backgroundColor_; }
  [[nodiscard]] QColor meshColor() const { return meshColor_; }

  void setFirstLayer(int layer);
  void setLastLayer(int layer);
  void setLayerStep(int step);
  void setWorkerCount(int count);
  void setBackgroundColor(const QColor& color);
  void setMeshColor(const QColor& color);

  Q_INVOKABLE void load();
  Q_INVOKABLE void orbitPixels(qreal deltaX, qreal deltaY);
  Q_INVOKABLE void panPixels(qreal deltaX, qreal deltaY);
  Q_INVOKABLE void zoomSteps(qreal steps);
  Q_INVOKABLE void resetView();

signals:
  void sourcePathChanged();
  void loadingChanged();
  void progressChanged();
  void errorStringChanged();
  void documentChanged();
  void visibleRangeChanged();
  void layerStepChanged();
  void workerCountChanged();
  void meshStatsChanged();
  void backgroundColorChanged();
  void meshColorChanged();

private:
  friend class GlFramebufferRenderer;

  void stopWorker();
  void resetDocumentState();
  void applyDocumentMetadata(
      std::uint64_t generation,
      int totalLayers,
      qreal pitchZMm,
      QString machineName);
  void applyProgress(std::uint64_t generation, qreal progress);
  void applyChunkStats(
      std::uint64_t generation,
      const photons::MeshBounds& bounds,
      std::size_t triangles);
  void finishLoad(std::uint64_t generation, QString errorString, bool cancelled);
  void includeBounds(const photons::MeshBounds& bounds) noexcept;
  void scheduleRender();

  QString sourcePath_;
  bool loading_ = false;
  qreal progress_ = 0.0;
  QString errorString_;
  QString machineName_;
  int totalLayers_ = 0;
  int firstLayer_ = 0;
  int lastLayer_ = 0;
  qreal layerHeightMm_ = 0.05;
  int layerStep_ = 2;
  int workerCount_ = 4;
  int loadedChunkCount_ = 0;
  qulonglong triangleCount_ = 0;
  QColor backgroundColor_{QStringLiteral("#171a1f")};
  QColor meshColor_{QStringLiteral("#55b7c6")};
  photons::MeshBounds bounds_;
  OrbitCamera camera_;
  bool cameraTouched_ = false;
  std::uint64_t sceneGeneration_ = 0;
  std::shared_ptr<UploadQueue> uploadQueue_;
  std::jthread worker_;
};

} // namespace accloud::render3d
