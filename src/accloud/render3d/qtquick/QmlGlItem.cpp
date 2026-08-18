#include "render3d/qtquick/QmlGlItem.h"

#include "render3d/qtquick/CompactShaderSources.h"
#include "render3d/analysis/SupportAnalyzer.h"
#include "render3d/core/LayerSectionCache.h"

#include "infra/logging/JsonlLogger.h"
#include "infra/photons/drivers/pwsz/PwszArchiveReader.h"
#include "render3d/meshing/LayerStackMesher.h"

#include <QFile>
#include <QMatrix4x4>
#include <QMetaObject>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QPointer>
#include <QQuickOpenGLUtils>
#include <QUrl>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace accloud::render3d {
namespace {

constexpr std::size_t kChunkLayers = 8;
constexpr std::size_t kUploadQueueChunks = 8;
constexpr std::size_t kUploadQueueBytes = 256u * 1024u * 1024u;
constexpr std::size_t kCutSurfaceBatchBytes = 256u * 1024u * 1024u;
constexpr std::size_t kLayerSectionCacheBytes = 64u * 1024u * 1024u;
constexpr std::size_t kLayerSectionCacheEntries = 2048;
constexpr std::size_t kGpuBudgetBytes = std::size_t{2} * 1024u * 1024u * 1024u;
constexpr float kVerticalFovDegrees = 45.0F;
constexpr std::string_view kRender3dLogSource = "render3d";

template <typename T>
std::string text(T value) {
  return std::to_string(value);
}

std::int64_t elapsedMilliseconds(std::chrono::steady_clock::time_point started) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - started)
      .count();
}

void trace3d(
    logging::Level level,
    std::string component,
    std::string event,
    std::string message = {},
    logging::FieldMap fields = {}) {
  logging::log(
      level,
      std::string(kRender3dLogSource),
      std::move(component),
      std::move(event),
      std::move(message),
      std::move(fields));
}

QString localPathFromInput(const QString& sourcePath) {
  const QUrl url(sourcePath);
  if (url.isLocalFile()) {
    return url.toLocalFile();
  }
  return sourcePath;
}

QVector3D toVector(const Vec3& value) {
  return QVector3D(
      static_cast<float>(value.x),
      static_cast<float>(value.y),
      static_cast<float>(value.z));
}

constexpr std::size_t kViewerLayerStride = 2;

std::size_t sampledMaskLayer(
    std::size_t layer,
    const SupportAnalysisResult* supportAnalysis = nullptr) {
  std::size_t sampled = (layer / kViewerLayerStride) * kViewerLayerStride;
  if (supportAnalysis == nullptr || supportAnalysis->forcedSampleLayers.empty()) {
    return sampled;
  }
  const auto upper = std::upper_bound(
      supportAnalysis->forcedSampleLayers.begin(),
      supportAnalysis->forcedSampleLayers.end(),
      layer);
  if (upper != supportAnalysis->forcedSampleLayers.begin()) {
    sampled = std::max(sampled, *std::prev(upper));
  }
  return sampled;
}

std::size_t sampledLayerCount(std::size_t totalLayers) {
  if (totalLayers == 0) {
    return 0;
  }
  const std::size_t lastLayer = totalLayers - 1;
  return 1 + (lastLayer + kViewerLayerStride - 1) / kViewerLayerStride;
}

std::size_t forcedSemanticSampleCount(
    std::size_t totalLayers,
    const std::vector<std::size_t>& forcedLayers) {
  if (totalLayers == 0) {
    return 0;
  }
  const std::size_t lastLayer = totalLayers - 1;
  return static_cast<std::size_t>(std::count_if(
      forcedLayers.begin(), forcedLayers.end(), [&](std::size_t layer) {
        return layer < totalLayers
               && layer != lastLayer
               && (layer % kViewerLayerStride) != 0u;
      }));
}

bool materializeSupportMask(
    const SupportAnalysisResult& analysis,
    std::size_t layer,
    const photons::BinaryMask& material,
    photons::BinaryMask& supportMask,
    std::string& error) {
  if (layer >= analysis.layers.size()) {
    error = "support semantic index does not cover the requested layer";
    return false;
  }

  std::vector<SemanticRun> runs;
  if (!SupportAnalyzer{}.materializeLayerSemantics(
          material,
          analysis.layers[layer],
          runs,
          error)) {
    return false;
  }

  supportMask = photons::BinaryMask(material.width(), material.height());
  for (const auto& run : runs) {
    if (run.semantic == MaterialSemantic::Model) {
      continue;
    }
    supportMask.setRun(
        static_cast<std::size_t>(run.y) * material.width() + run.firstX,
        run.lastX - run.firstX);
  }
  return true;
}

bool intersects(const photons::LayerRange& left, int firstLayer, int lastLayer) {
  if (!left.valid() || firstLayer <= 0 || lastLayer < firstLayer) {
    return false;
  }
  const photons::LayerRange right{
      static_cast<std::size_t>(firstLayer - 1),
      static_cast<std::size_t>(lastLayer - 1),
  };
  return left.intersects(right);
}

struct GpuChunk {
  photons::LayerRange layers;
  photons::MeshBounds bounds;
  std::unique_ptr<QOpenGLBuffer> instances;
  std::unique_ptr<QOpenGLVertexArrayObject> vertexArray;
  GLsizei instanceCount = 0;
  std::size_t bytes = 0;
  float pitchXMm = 1.0F;
  float pitchYMm = 1.0F;
  float pitchZMm = 1.0F;
  float baseLayer = 0.0F;
};

} // namespace

class GlFramebufferRenderer final : public QQuickFramebufferObject::Renderer {
public:
  GlFramebufferRenderer() : gpuBudget_(kGpuBudgetBytes) {}

  QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    return new QOpenGLFramebufferObject(size, format);
  }

  void synchronize(QQuickFramebufferObject* item) override {
    auto* viewer = static_cast<QmlGlItem*>(item);
    viewer_ = viewer;
    const bool sceneChanged = generation_ != viewer->sceneGeneration_;
    if (sceneChanged) {
      generation_ = viewer->sceneGeneration_;
      clearRequested_ = true;
      gpuFailureReported_ = false;
      pendingUploads_.clear();
      pendingCutBatch_.reset();
      activeCutGeneration_ = 0;
      displayedFirstLayer_ = viewer->firstLayer_;
      displayedLastLayer_ = viewer->lastLayer_;
      trace3d(
          logging::Level::kDebug,
          "gpu",
          "scene_reset_requested",
          {},
          {{"generation", text(generation_)}});
    }

    requestedCutGeneration_ = viewer->cutGeneration_.load(std::memory_order_acquire);
    if (pendingCutBatch_ && pendingCutBatch_->generation != requestedCutGeneration_) {
      pendingCutBatch_.reset();
    }
    {
      std::scoped_lock resultLock(viewer->cutResultMutex_);
      if (viewer->readyCutBatch_) {
        if (viewer->readyCutBatch_->sceneGeneration == generation_
            && viewer->readyCutBatch_->generation == requestedCutGeneration_) {
          pendingCutBatch_ = std::move(*viewer->readyCutBatch_);
        }
        viewer->readyCutBatch_.reset();
      }
    }

    if (viewer->uploadQueue_ && !gpuFailureReported_) {
      auto queued = viewer->uploadQueue_->takeAll();
      for (auto& chunk : queued) {
        pendingUploads_.push_back(std::move(chunk));
      }
    }

    requestedFirstLayer_ = viewer->firstLayer_;
    requestedLastLayer_ = viewer->lastLayer_;
    totalLayers_ = viewer->totalLayers_;
    if (displayedFirstLayer_ <= 0 && requestedFirstLayer_ > 0) {
      displayedFirstLayer_ = requestedFirstLayer_;
      displayedLastLayer_ = requestedLastLayer_;
    }
    pitchZMm_ = static_cast<float>(viewer->layerHeightMm_);
    backgroundColor_ = viewer->backgroundColor_;
    meshColor_ = viewer->meshColor_;
    supportColor_ = viewer->supportColor_;
    supportColoringEnabled_ = viewer->supportColoringEnabled_;
    cameraPosition_ = toVector(viewer->camera_.position());
    cameraTarget_ = toVector(viewer->camera_.target());
    cameraDistance_ = static_cast<float>(viewer->camera_.distance());
    loading_ = viewer->loading_;
  }

  void render() override {
    initializeIfNeeded();
    if (!program_) {
      return;
    }

    if (clearRequested_) {
      const std::size_t previousChunks = gpuChunks_.size() + cutGpuChunks_.size();
      const std::size_t previousBytes = gpuBudget_.residentBytes();
      gpuChunks_.clear();
      cutGpuChunks_.clear();
      gpuBudget_.reset();
      clearRequested_ = false;
      pendingCutBatch_.reset();
      activeCutGeneration_ = 0;
      trace3d(
          logging::Level::kDebug,
          "gpu",
          "scene_reset",
          {},
          {{"generation", text(generation_)},
           {"released_chunks", text(previousChunks)},
           {"released_bytes", text(previousBytes)}});
    }
    uploadPending();
    commitPendingCutBatch();

    auto* context = QOpenGLContext::currentContext();
    if (context == nullptr) {
      return;
    }
    QOpenGLExtraFunctions* functions = context->extraFunctions();
    const QSize renderSize = framebufferObject()->size();
    functions->glViewport(0, 0, renderSize.width(), renderSize.height());
    functions->glEnable(GL_DEPTH_TEST);
    functions->glDepthFunc(GL_LEQUAL);
    functions->glDisable(GL_CULL_FACE);
    functions->glClearColor(
        backgroundColor_.redF(),
        backgroundColor_.greenF(),
        backgroundColor_.blueF(),
        backgroundColor_.alphaF());
    functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    const float aspect = renderSize.height() > 0
                             ? static_cast<float>(renderSize.width())
                                   / static_cast<float>(renderSize.height())
                             : 1.0F;
    QMatrix4x4 projection;
    const float nearPlane = std::max(0.001F, cameraDistance_ * 0.001F);
    const float farPlane = std::max(1000.0F, cameraDistance_ * 20.0F);
    projection.perspective(kVerticalFovDegrees, aspect, nearPlane, farPlane);
    QMatrix4x4 view;
    view.lookAt(cameraPosition_, cameraTarget_, QVector3D(0.0F, 0.0F, 1.0F));
    const QMatrix4x4 mvp = projection * view;

    const float minimumZ = displayedFirstLayer_ > 0
                               ? static_cast<float>(displayedFirstLayer_ - 1) * pitchZMm_
                               : 0.0F;
    const float maximumZ = displayedLastLayer_ > 0
                               ? static_cast<float>(displayedLastLayer_) * pitchZMm_
                               : 0.0F;

    program_->bind();
    program_->setUniformValue("u_mvp", mvp);
    program_->setUniformValue("u_meshColor", QVector4D(
        meshColor_.redF(), meshColor_.greenF(), meshColor_.blueF(), meshColor_.alphaF()));
    program_->setUniformValue("u_supportColor", QVector4D(
        supportColor_.redF(), supportColor_.greenF(), supportColor_.blueF(), supportColor_.alphaF()));
    program_->setUniformValue("u_supportColoringEnabled", supportColoringEnabled_);
    program_->setUniformValue("u_lightDirection", QVector3D(-0.4F, -0.6F, -0.7F).normalized());
    program_->setUniformValue("u_clipZ", QVector2D(minimumZ, maximumZ));
    program_->setUniformValue("u_hasLowerCut", displayedFirstLayer_ > 1);
    program_->setUniformValue(
        "u_hasUpperCut",
        displayedLastLayer_ > 0 && displayedLastLayer_ < totalLayers_);
    program_->setUniformValue(
        "u_clipEpsilon",
        std::max(0.00001F, pitchZMm_ * 0.001F));
    program_->setUniformValue("u_cutSurfacePass", false);

    for (const auto& chunk : gpuChunks_) {
      if (!intersects(chunk.layers, displayedFirstLayer_, displayedLastLayer_)) {
        continue;
      }
      program_->setUniformValue(
          "u_pitch", QVector3D(chunk.pitchXMm, chunk.pitchYMm, chunk.pitchZMm));
      program_->setUniformValue("u_baseLayer", chunk.baseLayer);
      chunk.vertexArray->bind();
      functions->glDrawArraysInstanced(
          GL_TRIANGLES,
          0,
          6,
          chunk.instanceCount);
      chunk.vertexArray->release();
    }

    program_->setUniformValue("u_cutSurfacePass", true);
    for (const auto& chunk : cutGpuChunks_) {
      program_->setUniformValue(
          "u_pitch", QVector3D(chunk.pitchXMm, chunk.pitchYMm, chunk.pitchZMm));
      program_->setUniformValue("u_baseLayer", chunk.baseLayer);
      chunk.vertexArray->bind();
      functions->glDrawArraysInstanced(
          GL_TRIANGLES,
          0,
          6,
          chunk.instanceCount);
      chunk.vertexArray->release();
    }
    program_->release();

    functions->glDisable(GL_DEPTH_TEST);
    QQuickOpenGLUtils::resetOpenGLState();
    if (loading_ || !pendingUploads_.empty() || pendingCutBatch_.has_value()) {
      update();
    }
  }

private:
  void initializeIfNeeded() {
    if (program_ || shaderInitializationFailed_) {
      return;
    }

    auto program = std::make_unique<QOpenGLShaderProgram>();

    if (!program->addShaderFromSourceCode(
            QOpenGLShader::Vertex, shader::kCompactVertexShader)
        || !program->addShaderFromSourceCode(
            QOpenGLShader::Fragment, shader::kCompactFragmentShader)
        || !program->link()) {
      shaderInitializationFailed_ = true;
      qWarning("Render3D shader initialization failed: %s", qPrintable(program->log()));
      trace3d(
          logging::Level::kError,
          "gpu",
          "shader_initialization_failed",
          "Unable to initialize the compact 3D shader program",
          {{"generation", text(generation_)},
           {"shader_log", program->log().toStdString()}});
      reportGpuFailure(QmlGlItem::tr("Unable to initialize the 3D renderer."));
      return;
    }
    program_ = std::move(program);
    trace3d(
        logging::Level::kDebug,
        "gpu",
        "compact_shader_ready",
        {},
        {{"generation", text(generation_)},
         {"instance_bytes", text(sizeof(photons::PackedSurfaceQuad))},
         {"gpu_budget_bytes", text(gpuBudget_.maximumBytes())}});
  }

  void reportGpuFailure(QString message) {
    if (gpuFailureReported_) {
      return;
    }
    gpuFailureReported_ = true;
    pendingUploads_.clear();
    pendingCutBatch_.reset();
    loading_ = false;
    const auto generation = generation_;
    QPointer<QmlGlItem> guard = viewer_;
    if (guard) {
      QMetaObject::invokeMethod(
          guard.data(),
          [guard, generation, message = std::move(message)]() mutable {
            if (guard) {
              guard->applyGpuFailure(generation, std::move(message));
            }
          },
          Qt::QueuedConnection);
    }
  }

  [[nodiscard]] std::size_t gpuBytes(
      const std::vector<GpuChunk>& chunks) const noexcept {
    std::size_t total = 0;
    for (const auto& chunk : chunks) {
      total += chunk.bytes;
    }
    return total;
  }

  void releaseGpuChunks(std::vector<GpuChunk>& chunks) {
    const std::size_t releasedBytes = gpuBytes(chunks);
    chunks.clear();
    gpuBudget_.release(releasedBytes);
  }

  bool uploadChunks(
      std::vector<photons::MeshChunk>& pending,
      std::vector<GpuChunk>& destination,
      bool cutSurface,
      std::uint64_t cutGenerationForLog) {
    auto* context = QOpenGLContext::currentContext();
    if (context == nullptr) {
      return true;
    }
    QOpenGLExtraFunctions* functions = context->extraFunctions();

    for (auto& chunk : pending) {
      if (chunk.empty()) {
        continue;
      }

      const auto uploadStarted = std::chrono::steady_clock::now();
      const std::size_t surfaceCount = chunk.surfaceQuadCount();
      const std::size_t triangleCount = chunk.triangleCount();
      const std::size_t uploadBytes = UploadQueue::byteSize(chunk);
      const std::size_t legacyBytes = UploadQueue::legacyEquivalentByteSize(chunk);
      const photons::LayerRange layerRange = chunk.layers;

      if (surfaceCount > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())
          || uploadBytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        trace3d(
            logging::Level::kError,
            "gpu",
            cutSurface ? "cut_surface_too_large" : "compact_chunk_too_large",
            "A compact 3D surface buffer exceeds the OpenGL upload limits",
            {{"generation", text(generation_)},
             {"cut_generation", text(cutGenerationForLog)},
             {"first_layer", text(layerRange.first + 1)},
             {"last_layer", text(layerRange.last + 1)},
             {"surface_quads", text(surfaceCount)},
             {"compact_bytes", text(uploadBytes)}});
        pending.clear();
        reportGpuFailure(QmlGlItem::tr("The 3D model contains a chunk that is too large for OpenGL."));
        return false;
      }

      if (!gpuBudget_.tryReserve(uploadBytes)) {
        trace3d(
            logging::Level::kError,
            "gpu",
            "budget_exceeded",
            "The compact 3D mesh exceeds the configured GPU memory budget",
            {{"generation", text(generation_)},
             {"cut_generation", text(cutGenerationForLog)},
             {"cut_surface", cutSurface ? "true" : "false"},
             {"requested_bytes", text(uploadBytes)},
             {"resident_bytes", text(gpuBudget_.residentBytes())},
             {"budget_bytes", text(gpuBudget_.maximumBytes())},
             {"legacy_equivalent_bytes", text(legacyBytes)}});
        pending.clear();
        reportGpuFailure(QmlGlItem::tr(
            "The complete 3D model exceeds the safe GPU memory budget. No partial model was kept."));
        return false;
      }

      GpuChunk gpu;
      gpu.layers = chunk.layers;
      gpu.bounds = chunk.bounds;
      gpu.instanceCount = static_cast<GLsizei>(surfaceCount);
      gpu.bytes = uploadBytes;
      gpu.pitchXMm = chunk.pitchXMm;
      gpu.pitchYMm = chunk.pitchYMm;
      gpu.pitchZMm = chunk.pitchZMm;
      gpu.baseLayer = static_cast<float>(chunk.layers.first);
      gpu.instances = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
      gpu.vertexArray = std::make_unique<QOpenGLVertexArrayObject>();

      if (!gpu.instances->create() || !gpu.vertexArray->create()) {
        gpuBudget_.release(uploadBytes);
        trace3d(
            logging::Level::kError,
            "gpu",
            cutSurface ? "cut_surface_buffer_allocation_failed" : "buffer_allocation_failed",
            "Unable to allocate a compact GPU instance buffer",
            {{"generation", text(generation_)},
             {"cut_generation", text(cutGenerationForLog)},
             {"first_layer", text(layerRange.first + 1)},
             {"last_layer", text(layerRange.last + 1)},
             {"compact_bytes", text(uploadBytes)}});
        pending.clear();
        reportGpuFailure(QmlGlItem::tr("Unable to allocate memory for the complete 3D model."));
        return false;
      }

      while (functions->glGetError() != GL_NO_ERROR) {
      }
      gpu.vertexArray->bind();
      program_->bind();
      gpu.instances->bind();
      gpu.instances->setUsagePattern(QOpenGLBuffer::StaticDraw);
      gpu.instances->allocate(chunk.surfaces.data(), static_cast<int>(uploadBytes));
      functions->glEnableVertexAttribArray(0);
      functions->glVertexAttribIPointer(
          0,
          2,
          GL_UNSIGNED_INT,
          static_cast<GLsizei>(sizeof(photons::PackedSurfaceQuad)),
          nullptr);
      functions->glVertexAttribDivisor(0, 1);
      const GLenum uploadError = functions->glGetError();
      gpu.vertexArray->release();
      gpu.instances->release();
      program_->release();

      if (uploadError != GL_NO_ERROR) {
        gpuBudget_.release(uploadBytes);
        trace3d(
            logging::Level::kError,
            "gpu",
            cutSurface ? "cut_surface_upload_failed" : "compact_upload_failed",
            "OpenGL rejected a compact 3D instance buffer",
            {{"generation", text(generation_)},
             {"cut_generation", text(cutGenerationForLog)},
             {"first_layer", text(layerRange.first + 1)},
             {"last_layer", text(layerRange.last + 1)},
             {"compact_bytes", text(uploadBytes)},
             {"gl_error", text(uploadError)}});
        pending.clear();
        reportGpuFailure(QmlGlItem::tr("OpenGL rejected the complete 3D model before rendering."));
        return false;
      }

      destination.push_back(std::move(gpu));
      const double compressionRatio = uploadBytes == 0
                                          ? 0.0
                                          : static_cast<double>(legacyBytes)
                                                / static_cast<double>(uploadBytes);
      trace3d(
          logging::Level::kDebug,
          "gpu",
          cutSurface ? "cut_surface_uploaded" : "compact_chunk_uploaded",
          {},
          {{"generation", text(generation_)},
           {"cut_generation", text(cutGenerationForLog)},
           {"first_layer", text(layerRange.first + 1)},
           {"last_layer", text(layerRange.last + 1)},
           {"surface_quads", text(surfaceCount)},
           {"triangles", text(triangleCount)},
           {"compact_bytes", text(uploadBytes)},
           {"legacy_equivalent_bytes", text(legacyBytes)},
           {"compression_ratio", text(compressionRatio)},
           {"resident_bytes", text(gpuBudget_.residentBytes())},
           {"budget_bytes", text(gpuBudget_.maximumBytes())},
           {"duration_ms", text(elapsedMilliseconds(uploadStarted))},
           {"gpu_chunk_count", text(destination.size())}});
    }
    pending.clear();
    return true;
  }

  void uploadPending() {
    if (!program_ || gpuFailureReported_) {
      pendingUploads_.clear();
      pendingCutBatch_.reset();
      return;
    }
    (void)uploadChunks(pendingUploads_, gpuChunks_, false, 0);
  }

  void commitPendingCutBatch() {
    if (!pendingCutBatch_ || gpuFailureReported_) {
      return;
    }
    if (pendingCutBatch_->sceneGeneration != generation_
        || pendingCutBatch_->generation != requestedCutGeneration_) {
      pendingCutBatch_.reset();
      return;
    }

    auto batch = std::move(*pendingCutBatch_);
    pendingCutBatch_.reset();
    std::vector<GpuChunk> staging;
    staging.reserve(batch.surfaces.size());
    if (!uploadChunks(batch.surfaces, staging, true, batch.generation)) {
      releaseGpuChunks(staging);
      return;
    }

    const std::size_t previousChunks = cutGpuChunks_.size();
    const std::size_t previousBytes = gpuBytes(cutGpuChunks_);
    cutGpuChunks_.swap(staging);
    releaseGpuChunks(staging);
    displayedFirstLayer_ = batch.firstLayer;
    displayedLastLayer_ = batch.lastLayer;
    activeCutGeneration_ = batch.generation;
    trace3d(
        logging::Level::kDebug,
        "gpu",
        "cut_surface_swap_committed",
        {},
        {{"generation", text(generation_)},
         {"cut_generation", text(activeCutGeneration_)},
         {"first_layer", text(displayedFirstLayer_)},
         {"last_layer", text(displayedLastLayer_)},
         {"new_chunks", text(cutGpuChunks_.size())},
         {"new_bytes", text(gpuBytes(cutGpuChunks_))},
         {"released_chunks", text(previousChunks)},
         {"released_bytes", text(previousBytes)},
         {"cache_hits", text(batch.cacheHits)},
         {"cache_misses", text(batch.cacheMisses)}});
  }

  std::unique_ptr<QOpenGLShaderProgram> program_;
  std::vector<GpuChunk> gpuChunks_;
  std::vector<GpuChunk> cutGpuChunks_;
  std::vector<photons::MeshChunk> pendingUploads_;
  std::optional<QmlGlItem::CutSurfaceBatch> pendingCutBatch_;
  GpuMemoryBudget gpuBudget_;
  QPointer<QmlGlItem> viewer_;
  std::uint64_t generation_ = 0;
  std::uint64_t requestedCutGeneration_ = 0;
  std::uint64_t activeCutGeneration_ = 0;
  bool clearRequested_ = false;
  bool loading_ = false;
  bool gpuFailureReported_ = false;
  bool shaderInitializationFailed_ = false;
  int requestedFirstLayer_ = 0;
  int requestedLastLayer_ = 0;
  int displayedFirstLayer_ = 0;
  int displayedLastLayer_ = 0;
  int totalLayers_ = 0;
  float pitchZMm_ = 0.05F;
  float cameraDistance_ = 100.0F;
  QColor backgroundColor_;
  QColor meshColor_;
  QColor supportColor_;
  bool supportColoringEnabled_ = true;
  QVector3D cameraPosition_;
  QVector3D cameraTarget_;
};

QmlGlItem::QmlGlItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent),
      uploadQueue_(std::make_shared<UploadQueue>(
          kUploadQueueChunks,
          kUploadQueueBytes)) {
  setMirrorVertically(true);
  cutWorker_ = std::jthread([this](std::stop_token stopToken) {
    runCutWorker(stopToken);
  });
}

QmlGlItem::~QmlGlItem() {
  stopWorker();
  stopCutWorker();
}

QQuickFramebufferObject::Renderer* QmlGlItem::createRenderer() const {
  return new GlFramebufferRenderer();
}

void QmlGlItem::setSourcePath(const QString& sourcePath) {
  if (sourcePath_ == sourcePath) {
    return;
  }
  sourcePath_ = sourcePath;
  emit sourcePathChanged();
}

void QmlGlItem::setFirstLayer(int layer) {
  if (totalLayers_ <= 0) {
    layer = 0;
  } else {
    layer = std::clamp(layer, 1, std::max(1, lastLayer_));
  }
  if (firstLayer_ == layer) {
    return;
  }
  firstLayer_ = layer;
  emit visibleRangeChanged();
  requestCutSurfaceRebuild();
}

void QmlGlItem::setLastLayer(int layer) {
  if (totalLayers_ <= 0) {
    layer = 0;
  } else {
    layer = std::clamp(layer, std::max(1, firstLayer_), totalLayers_);
  }
  if (lastLayer_ == layer) {
    return;
  }
  lastLayer_ = layer;
  emit visibleRangeChanged();
  requestCutSurfaceRebuild();
}

void QmlGlItem::setWorkerCount(int count) {
  count = std::clamp(
      count,
      static_cast<int>(kMinimumMeshWorkerCount),
      static_cast<int>(kMaximumMeshWorkerCount));
  if (workerCount_ == count) {
    return;
  }
  workerCount_ = count;
  emit workerCountChanged();
  trace3d(
      logging::Level::kDebug,
      "viewer",
      "worker_count_changed",
      {},
      {{"generation", text(sceneGeneration_)}, {"worker_count", text(workerCount_)}});
}

void QmlGlItem::setBackgroundColor(const QColor& color) {
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  emit backgroundColorChanged();
  scheduleRender();
}

void QmlGlItem::setMeshColor(const QColor& color) {
  if (meshColor_ == color) {
    return;
  }
  meshColor_ = color;
  emit meshColorChanged();
  scheduleRender();
}

void QmlGlItem::setSupportColor(const QColor& color) {
  if (supportColor_ == color) {
    return;
  }
  supportColor_ = color;
  emit supportColorChanged();
  scheduleRender();
}

void QmlGlItem::setSupportColoringEnabled(bool enabled) {
  if (supportColoringEnabled_ == enabled) {
    return;
  }
  supportColoringEnabled_ = enabled;
  emit supportColoringEnabledChanged();

  const bool requiresAnalysis = enabled
                                && !loading_
                                && totalLayers_ > 0
                                && !sceneHasSupportSemantics_
                                && !sourcePath_.trimmed().isEmpty();
  trace3d(
      logging::Level::kDebug,
      "viewer",
      "support_option_changed",
      {},
      {{"generation", text(sceneGeneration_)},
       {"enabled", enabled ? "true" : "false"},
       {"analysis_available", sceneHasSupportSemantics_ ? "true" : "false"},
       {"reload_required", requiresAnalysis ? "true" : "false"}});
  scheduleRender();
  if (requiresAnalysis) {
    QMetaObject::invokeMethod(this, [this] { load(); }, Qt::QueuedConnection);
  }
}

void QmlGlItem::stopWorker() {
  if (!worker_.joinable()) {
    return;
  }
  worker_.request_stop();
  uploadQueue_->clear();
  worker_.join();
}

void QmlGlItem::stopCutWorker() {
  if (!cutWorker_.joinable()) {
    return;
  }
  cutWorker_.request_stop();
  cutRequestChanged_.notify_all();
  cutWorker_.join();
  std::scoped_lock resultLock(cutResultMutex_);
  readyCutBatch_.reset();
}

void QmlGlItem::requestCutSurfaceRebuild() {
  std::uint64_t generation = 0;
  {
    // Generation publication, stale-result removal and request replacement are
    // one transaction. The active GPU section is intentionally preserved until
    // a complete new batch has been decoded and uploaded.
    std::scoped_lock lock(cutRequestMutex_);
    generation = cutGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;
    {
      std::scoped_lock resultLock(cutResultMutex_);
      readyCutBatch_.reset();
    }
    if (!loading_ && archiveReader_ && totalLayers_ > 0
        && firstLayer_ > 0 && lastLayer_ >= firstLayer_) {
      cutRequest_ = CutSurfaceRequest{
          sceneGeneration_,
          generation,
          firstLayer_,
          lastLayer_,
          totalLayers_,
          archiveReader_,
          supportAnalysis_,
      };
    } else {
      cutRequest_.reset();
    }
  }
  cutRequestChanged_.notify_all();
  trace3d(
      logging::Level::kDebug,
      "cut_surface",
      "rebuild_requested",
      {},
      {{"generation", text(sceneGeneration_)},
       {"cut_generation", text(generation)},
       {"first_layer", text(firstLayer_)},
       {"last_layer", text(lastLayer_)},
       {"layer_step", text(kViewerLayerStride)}});
  scheduleRender();
}

void QmlGlItem::runCutWorker(std::stop_token stopToken) {
  LayerSectionCache sectionCache(
      kLayerSectionCacheBytes,
      kLayerSectionCacheEntries);
  std::uint64_t cachedSceneGeneration = 0;

  while (!stopToken.stop_requested()) {
    CutSurfaceRequest request;
    {
      std::unique_lock lock(cutRequestMutex_);
      cutRequestChanged_.wait(
          lock,
          stopToken,
          [&] { return cutRequest_.has_value(); });
      if (stopToken.stop_requested()) {
        break;
      }
      request = std::move(*cutRequest_);
      cutRequest_.reset();
    }

    const auto stillCurrent = [&] {
      return !stopToken.stop_requested()
             && cutGeneration_.load(std::memory_order_acquire) == request.generation;
    };
    if (!request.reader || !stillCurrent()) {
      continue;
    }
    if (cachedSceneGeneration != request.sceneGeneration) {
      sectionCache.clear();
      cachedSceneGeneration = request.sceneGeneration;
    }

    const auto started = std::chrono::steady_clock::now();
    const auto cacheBefore = sectionCache.stats();
    const auto& meta = request.reader->meta();
    MeshBuildOptions options;
    options.pitchXMm = meta.pitchXMm.value_or(meta.pitchXYMm.value_or(1.0));
    options.pitchYMm = meta.pitchYMm.value_or(meta.pitchXYMm.value_or(options.pitchXMm));
    options.pitchZMm = meta.pitchZMm.value_or(0.05);
    options.chunkLayerCount = 1;
    options.layerStride = kViewerLayerStride;
    options.workerCount = 1;
    options.classifySupports = static_cast<bool>(request.supportAnalysis);
    if (request.supportAnalysis) {
      const auto analysis = request.supportAnalysis;
      const auto sceneGeneration = request.sceneGeneration;
      const auto cutGeneration = request.generation;
      options.supportMaskProvider = [analysis, sceneGeneration, cutGeneration](
          std::size_t layer,
          const photons::BinaryMask& material,
          photons::BinaryMask& supportMask,
          std::string& error) {
        const bool ok = materializeSupportMask(
            *analysis, layer, material, supportMask, error);
        if (!ok) {
          trace3d(
              logging::Level::kError,
              "support_analysis",
              "materialization_failed",
              error,
              {{"generation", text(sceneGeneration)},
               {"cut_generation", text(cutGeneration)},
               {"layer", text(layer + 1)}});
        }
        return ok;
      };
    }

    LayerStackMesher mesher;
    std::vector<photons::MeshChunk> surfaces;
    std::size_t decodedLayers = 0;
    std::size_t surfaceQuads = 0;
    std::size_t compactBytes = 0;
    bool failed = false;
    std::string failure;

    const auto buildBoundary = [&](std::size_t maskLayer,
                                   std::size_t planeLayer,
                                   CutSurfaceBoundary boundary) {
      if (!stillCurrent() || failed) {
        return;
      }

      auto section = sectionCache.find(maskLayer);
      bool cacheHit = static_cast<bool>(section);
      if (!section) {
        auto result = mesher.buildCutSurface(
            *request.reader,
            maskLayer,
            0,
            CutSurfaceBoundary::Upper,
            options);
        decodedLayers += result.decodedLayerCount;
        if (!result.ok) {
          failed = true;
          failure = std::move(result.error);
          return;
        }
        LayerSectionTemplate built;
        if (!makeLayerSectionTemplate(result.chunk, built, failure)) {
          failed = true;
          return;
        }
        section = sectionCache.insert(maskLayer, std::move(built));
      }

      auto chunk = materializeLayerSection(
          *section,
          planeLayer,
          boundary,
          static_cast<float>(options.pitchXMm),
          static_cast<float>(options.pitchYMm),
          static_cast<float>(options.pitchZMm));
      const std::size_t boundarySurfaceQuads = chunk.surfaceQuadCount();
      const std::size_t boundaryBytes = chunk.compactByteSize();
      if (boundaryBytes > kCutSurfaceBatchBytes - std::min(kCutSurfaceBatchBytes, compactBytes)) {
        failed = true;
        failure = "visible-layer section batch exceeds the bounded CPU staging budget";
        return;
      }
      surfaceQuads += boundarySurfaceQuads;
      compactBytes += boundaryBytes;
      if (!chunk.empty()) {
        surfaces.push_back(std::move(chunk));
      }
      trace3d(
          logging::Level::kDebug,
          "cut_surface",
          "boundary_built",
          {},
          {{"generation", text(request.sceneGeneration)},
           {"cut_generation", text(request.generation)},
           {"boundary", boundary == CutSurfaceBoundary::Lower ? "lower" : "upper"},
           {"mask_layer", text(maskLayer + 1)},
           {"plane_layer", text(planeLayer)},
           {"surface_quads", text(boundarySurfaceQuads)},
           {"compact_bytes", text(boundaryBytes)},
           {"cache_hit", cacheHit ? "true" : "false"}});
    };

    if (request.firstLayer > 1) {
      const auto planeLayer = static_cast<std::size_t>(request.firstLayer - 1);
      const auto maskLayer = sampledMaskLayer(
          planeLayer, request.supportAnalysis.get());
      buildBoundary(maskLayer, planeLayer, CutSurfaceBoundary::Lower);
    }
    if (!failed && request.lastLayer < request.totalLayers) {
      const auto planeLayer = static_cast<std::size_t>(request.lastLayer);
      const auto materialLayer = static_cast<std::size_t>(request.lastLayer - 1);
      const auto maskLayer = sampledMaskLayer(
          materialLayer, request.supportAnalysis.get());
      buildBoundary(maskLayer, planeLayer, CutSurfaceBoundary::Upper);
    }

    if (!stillCurrent()) {
      trace3d(
          logging::Level::kDebug,
          "cut_surface",
          "build_discarded",
          {},
          {{"generation", text(request.sceneGeneration)},
           {"cut_generation", text(request.generation)},
           {"duration_ms", text(elapsedMilliseconds(started))}});
      continue;
    }

    if (failed) {
      trace3d(
          logging::Level::kError,
          "cut_surface",
          "build_failed",
          failure,
          {{"generation", text(request.sceneGeneration)},
           {"cut_generation", text(request.generation)},
           {"duration_ms", text(elapsedMilliseconds(started))}});
      QPointer<QmlGlItem> guard(this);
      QMetaObject::invokeMethod(
          this,
          [guard, generation = request.generation] {
            if (!guard
                || guard->cutGeneration_.load(std::memory_order_acquire) != generation) {
              return;
            }
            guard->errorString_ = QmlGlItem::tr(
                "Unable to build the visible-layer section surfaces.");
            emit guard->errorStringChanged();
          },
          Qt::QueuedConnection);
      continue;
    }

    const auto cacheAfter = sectionCache.stats();
    CutSurfaceBatch batch;
    batch.sceneGeneration = request.sceneGeneration;
    batch.generation = request.generation;
    batch.firstLayer = request.firstLayer;
    batch.lastLayer = request.lastLayer;
    batch.totalLayers = request.totalLayers;
    batch.surfaces = std::move(surfaces);
    batch.compactBytes = compactBytes;
    batch.cacheHits = cacheAfter.hits - cacheBefore.hits;
    batch.cacheMisses = cacheAfter.misses - cacheBefore.misses;

    bool published = false;
    {
      // Publication is serialized with request replacement. The renderer only
      // sees complete batches; the previous clipping range remains active until
      // this batch has also been uploaded successfully.
      std::scoped_lock requestLock(cutRequestMutex_);
      if (stillCurrent()) {
        std::scoped_lock resultLock(cutResultMutex_);
        readyCutBatch_ = std::move(batch);
        published = true;
      }
    }
    if (!published) {
      continue;
    }

    trace3d(
        logging::Level::kDebug,
        "cut_surface",
        "build_completed",
        {},
        {{"generation", text(request.sceneGeneration)},
         {"cut_generation", text(request.generation)},
         {"first_layer", text(request.firstLayer)},
         {"last_layer", text(request.lastLayer)},
         {"decoded_layers", text(decodedLayers)},
         {"surface_quads", text(surfaceQuads)},
         {"compact_bytes", text(compactBytes)},
         {"cache_hits", text(cacheAfter.hits - cacheBefore.hits)},
         {"cache_misses", text(cacheAfter.misses - cacheBefore.misses)},
         {"cache_entries", text(cacheAfter.entries)},
         {"cache_resident_bytes", text(cacheAfter.residentBytes)},
         {"duration_ms", text(elapsedMilliseconds(started))}});
    QPointer<QmlGlItem> guard(this);
    QMetaObject::invokeMethod(
        this,
        [guard] {
          if (guard) {
            guard->scheduleRender();
          }
        },
        Qt::QueuedConnection);
  }
}

void QmlGlItem::resetDocumentState() {
  loading_ = true;
  progress_ = 0.0;
  loadingPhase_ = tr("Creating 3D view…");
  errorString_.clear();
  machineName_.clear();
  totalLayers_ = 0;
  firstLayer_ = 0;
  lastLayer_ = 0;
  loadedChunkCount_ = 0;
  triangleCount_ = 0;
  bounds_ = {};
  camera_ = OrbitCamera{};
  cameraTouched_ = false;
  uploadQueue_->clear();
  archiveReader_.reset();
  supportAnalysis_.reset();
  sceneHasSupportSemantics_ = false;
  {
    std::scoped_lock lock(cutRequestMutex_);
    cutGeneration_.fetch_add(1, std::memory_order_release);
    cutRequest_.reset();
    std::scoped_lock resultLock(cutResultMutex_);
    readyCutBatch_.reset();
  }
  cutRequestChanged_.notify_all();
  ++sceneGeneration_;
  emit loadingChanged();
  emit progressChanged();
  emit loadingPhaseChanged();
  emit errorStringChanged();
  emit documentChanged();
  emit visibleRangeChanged();
  emit meshStatsChanged();
  scheduleRender();
}

void QmlGlItem::cancelLoad() {
  if (!loading_) {
    return;
  }

  const std::uint64_t cancelledGeneration = sceneGeneration_;
  if (worker_.joinable()) {
    worker_.request_stop();
  }
  uploadQueue_->clear();
  {
    std::scoped_lock lock(cutRequestMutex_);
    cutGeneration_.fetch_add(1, std::memory_order_release);
    cutRequest_.reset();
    std::scoped_lock resultLock(cutResultMutex_);
    readyCutBatch_.reset();
  }
  cutRequestChanged_.notify_all();

  loading_ = false;
  progress_ = 0.0;
  loadingPhase_.clear();
  errorString_.clear();
  machineName_.clear();
  totalLayers_ = 0;
  firstLayer_ = 0;
  lastLayer_ = 0;
  layerHeightMm_ = 0.05;
  loadedChunkCount_ = 0;
  triangleCount_ = 0;
  bounds_ = {};
  camera_ = OrbitCamera{};
  cameraTouched_ = false;
  archiveReader_.reset();
  supportAnalysis_.reset();
  sceneHasSupportSemantics_ = false;
  ++sceneGeneration_;

  trace3d(
      logging::Level::kInfo,
      "viewer",
      "load_cancel_requested",
      {},
      {{"cancelled_generation", text(cancelledGeneration)},
       {"next_generation", text(sceneGeneration_)}});

  emit loadingChanged();
  emit progressChanged();
  emit loadingPhaseChanged();
  emit errorStringChanged();
  emit documentChanged();
  emit visibleRangeChanged();
  emit meshStatsChanged();
  scheduleRender();
}

void QmlGlItem::load() {
  stopWorker();
  resetDocumentState();
  const std::uint64_t generation = sceneGeneration_;
  const int workerCount = workerCount_;
  const bool analyzeSupports = supportColoringEnabled_;
  const QString inputPath = localPathFromInput(sourcePath_).trimmed();
  trace3d(
      logging::Level::kDebug,
      "viewer",
      "load_requested",
      {},
      {{"generation", text(generation)},
       {"layer_step", text(kViewerLayerStride)},
       {"worker_count", text(workerCount)},
       {"support_analysis_enabled", analyzeSupports ? "true" : "false"}});
  if (inputPath.isEmpty()) {
    trace3d(
        logging::Level::kWarn,
        "viewer",
        "load_rejected",
        "No PWSZ source path was provided",
        {{"generation", text(generation)}});
    finishLoad(generation, tr("Select a PWSZ file first."), false);
    return;
  }

  const auto queue = uploadQueue_;
  QPointer<QmlGlItem> guard(this);
  worker_ = std::jthread(
      [guard, queue, generation, inputPath, workerCount, analyzeSupports](
          std::stop_token stopToken) {
        const auto loadStarted = std::chrono::steady_clock::now();
        const auto post = [&](auto callback) {
          if (guard) {
            QMetaObject::invokeMethod(
                guard.data(),
                [guard, callback = std::move(callback)]() mutable {
                  if (guard) {
                    callback(*guard);
                  }
                },
                Qt::QueuedConnection);
          }
        };

        auto reader = std::make_shared<photons::pwsz::PwszArchiveReader>();
        std::string error;
        const QByteArray pathBytes = QFile::encodeName(inputPath);
        const std::filesystem::path archivePath(pathBytes.constData());
        std::error_code fileSizeError;
        const auto archiveBytes = std::filesystem::file_size(archivePath, fileSizeError);
        const auto archiveOpenStarted = std::chrono::steady_clock::now();
        trace3d(
            logging::Level::kDebug,
            "archive",
            "open_started",
            {},
            {{"generation", text(generation)},
             {"archive_bytes", fileSizeError ? "unknown" : text(archiveBytes)}});
        if (!reader->open(archivePath, error)) {
          trace3d(
              logging::Level::kError,
              "archive",
              "open_failed",
              "Unable to open the PWSZ archive",
              {{"generation", text(generation)},
               {"duration_ms", text(elapsedMilliseconds(archiveOpenStarted))},
               {"reason", error}});
          post([generation, message = QString::fromStdString(error)](QmlGlItem& item) {
            item.finishLoad(generation, message, false);
          });
          return;
        }

        const auto& meta = reader->meta();
        const int totalLayers = static_cast<int>(reader->layerCount());
        const qreal pitchZ = meta.pitchZMm.value_or(0.05);
        const qreal pitchX = meta.pitchXMm.value_or(meta.pitchXYMm.value_or(1.0));
        const qreal pitchY = meta.pitchYMm.value_or(meta.pitchXYMm.value_or(pitchX));
        trace3d(
            logging::Level::kDebug,
            "archive",
            "open_completed",
            {},
            {{"generation", text(generation)},
             {"duration_ms", text(elapsedMilliseconds(archiveOpenStarted))},
             {"layers", text(totalLayers)},
             {"width", text(reader->width())},
             {"height", text(reader->height())},
             {"pitch_x_mm", text(static_cast<double>(pitchX))},
             {"pitch_y_mm", text(static_cast<double>(pitchY))},
             {"pitch_z_mm", text(static_cast<double>(pitchZ))}});
        post([generation,
              totalLayers,
              pitchZ,
              machine = QString::fromStdString(meta.machineName),
              reader](QmlGlItem& item) mutable {
          item.applyDocumentMetadata(
              generation,
              totalLayers,
              pitchZ,
              std::move(machine),
              std::move(reader));
        });

        if (totalLayers <= 0) {
          post([generation](QmlGlItem& item) {
            item.finishLoad(generation, QmlGlItem::tr("The PWSZ archive contains no layers."), false);
          });
          return;
        }

        // P6.5 reports support analysis as geometry/evidence preparation plus
        // the forward and reverse reconciliation phases.
        const std::size_t analysisWork = analyzeSupports
            ? reader->layerCount() * 3u
            : 0u;
        const std::size_t baseMeshWork = sampledLayerCount(reader->layerCount());
        std::size_t meshWork = baseMeshWork;
        std::size_t totalWork = analysisWork + meshWork;
        const qreal analysisCompletionFloor = totalWork == 0
                                                  ? 0.0
                                                  : static_cast<qreal>(analysisWork)
                                                        / static_cast<qreal>(totalWork);
        std::shared_ptr<const SupportAnalysisResult> supportAnalysis;

        if (analyzeSupports) {
          post([generation](QmlGlItem& item) {
            item.applyLoadingPhase(generation, QmlGlItem::tr("Analyzing supports…"));
          });

          SupportAnalysisOptions analysisOptions;
          analysisOptions.pitchXMillimetres = static_cast<double>(pitchX);
          analysisOptions.pitchYMillimetres = static_cast<double>(pitchY);
          analysisOptions.pitchZMillimetres = static_cast<double>(pitchZ);
          analysisOptions.workerCount = static_cast<std::size_t>(std::clamp(
              workerCount,
              static_cast<int>(kMinimumSupportAnalysisWorkerCount),
              static_cast<int>(kMaximumSupportAnalysisWorkerCount)));
          const auto analysisStarted = std::chrono::steady_clock::now();
          trace3d(
              logging::Level::kDebug,
              "support_analysis",
              "started",
              {},
              {{"generation", text(generation)},
               {"source_layers", text(reader->layerCount())},
               {"width", text(reader->width())},
               {"height", text(reader->height())},
               {"requested_workers", text(analysisOptions.workerCount)},
               {"concurrent_mask_loads",
                reader->supportsConcurrentMaskLoads() ? "true" : "false"}});

          int lastAnalysisReportedPercent = -1;
          int lastAnalysisLoggedDecile = -1;
          compute::SupportComputeTelemetry latestComputeTelemetry;
          SupportAnalysisPerformanceTelemetry latestPerformanceTelemetry;
          SupportAnalysisCallbacks analysisCallbacks;
          analysisCallbacks.isCancelled = [&] { return stopToken.stop_requested(); };
          analysisCallbacks.computeStatus = [&](bool compiled,
                                                bool active,
                                                const std::string& backend,
                                                const std::string& device,
                                                const std::string& diagnostic) {
            trace3d(
                logging::Level::kDebug,
                "support_analysis",
                "compute_status",
                diagnostic,
                {{"generation", text(generation)},
                 {"vulkan_compute_compiled", compiled ? "true" : "false"},
                 {"vulkan_compute_active", active ? "true" : "false"},
                 {"compute_backend", backend},
                 {"compute_device", device}});
          };
          analysisCallbacks.computeTelemetry = [&](const auto& telemetry) {
            latestComputeTelemetry = telemetry;
          };
          analysisCallbacks.performanceTelemetry = [&](const auto& telemetry) {
            latestPerformanceTelemetry = telemetry;
          };
          analysisCallbacks.progress = [&](std::size_t completed, std::size_t total) {
            const int percent = total == 0
                                    ? 100
                                    : static_cast<int>((completed * 100) / total);
            const int decile = percent / 10;
            if (decile != lastAnalysisLoggedDecile || completed == total) {
              lastAnalysisLoggedDecile = decile;
              trace3d(
                  logging::Level::kDebug,
                  "support_analysis",
                  "progress",
                  {},
                  {{"generation", text(generation)},
                   {"completed_layers", text(completed)},
                   {"total_layers", text(total)},
                   {"percent", text(percent)},
                   {"duration_ms", text(elapsedMilliseconds(analysisStarted))},
                   {"vulkan_eligible_jobs", text(latestComputeTelemetry.eligibleJobs)},
                   {"vulkan_gpu_jobs", text(latestComputeTelemetry.completedGpuJobs)},
                   {"vulkan_cpu_fallback_jobs",
                    text(latestComputeTelemetry.cpuFallbackJobs)},
                   {"vulkan_dispatches",
                    text(latestComputeTelemetry.successfulDispatches)},
                   {"vulkan_max_batch_jobs",
                    text(latestComputeTelemetry.maximumBatchJobs)},
                   {"vulkan_upload_bytes", text(latestComputeTelemetry.uploadBytes)},
                   {"vulkan_readback_bytes", text(latestComputeTelemetry.readbackBytes)},
                   {"vulkan_host_prepare_us",
                    text(latestComputeTelemetry.hostPreparationNanoseconds / 1000u)},
                   {"vulkan_queue_wait_us",
                    text(latestComputeTelemetry.queueWaitNanoseconds / 1000u)},
                   {"vulkan_batch_execution_us",
                    text(latestComputeTelemetry.batchExecutionNanoseconds / 1000u)},
                   {"vulkan_run_source_jobs",
                    text(latestComputeTelemetry.runSourceJobs)},
                   {"vulkan_resident_reference_uploads",
                    text(latestComputeTelemetry.residentReferenceUploads)},
                   {"vulkan_resident_reference_reuses",
                    text(latestComputeTelemetry.residentReferenceReuses)},
                   {"vulkan_submitted_workgroups",
                    text(latestComputeTelemetry.submittedWorkgroups)},
                   {"vulkan_semantic_layer_batch_calls",
                    text(latestComputeTelemetry.semanticLayerBatchCalls)},
                   {"vulkan_semantic_layer_batch_jobs",
                    text(latestComputeTelemetry.semanticLayerBatchJobs)},
                   {"support_preparation_window",
                    text(latestPerformanceTelemetry.preparationWindowCapacity)},
                   {"support_prepared_layers",
                    text(latestPerformanceTelemetry.preparedLayerCount)},
                   {"support_max_preparation_inflight",
                    text(latestPerformanceTelemetry.maximumPreparationInflight)},
                   {"support_prepare_load_us",
                    text(latestPerformanceTelemetry.preparationLoadMicroseconds)},
                   {"support_prepare_describe_us",
                    text(latestPerformanceTelemetry.preparationDescribeMicroseconds)},
                   {"support_forward_semantic_us",
                    text(latestPerformanceTelemetry.forwardSemanticMicroseconds)},
                   {"support_reverse_semantic_us",
                    text(latestPerformanceTelemetry.reverseSemanticMicroseconds)},
                   {"support_forward_classification_us",
                    text(latestPerformanceTelemetry.forwardClassificationMicroseconds)},
                   {"support_forward_commit_us",
                    text(latestPerformanceTelemetry.forwardCommitMicroseconds)},
                   {"support_forward_lineage_us",
                    text(latestPerformanceTelemetry.forwardLineageMicroseconds)},
                   {"support_forward_lineage_commit_us",
                    text(latestPerformanceTelemetry.forwardLineageCommitMicroseconds)},
                   {"support_reverse_prepare_us",
                    text(latestPerformanceTelemetry.reversePreparationMicroseconds)},
                   {"support_reverse_commit_us",
                    text(latestPerformanceTelemetry.reverseCommitMicroseconds)},
                   {"support_semantic_evidence_us",
                    text(latestPerformanceTelemetry.semanticEvidenceMicroseconds)},
                   {"support_semantic_evidence_lots",
                    text(latestPerformanceTelemetry.semanticEvidenceLotCount)},
                   {"support_semantic_evidence_layer_pairs",
                    text(latestPerformanceTelemetry.semanticEvidenceLayerPairCount)},
                   {"support_semantic_evidence_edges",
                    text(latestPerformanceTelemetry.semanticEvidenceEdgeCount)}});
            }
            if (percent == lastAnalysisReportedPercent && completed != total) {
              return;
            }
            lastAnalysisReportedPercent = percent;
            post([generation,
                  completed,
                  totalWork](QmlGlItem& item) {
              const qreal value = totalWork == 0
                                      ? 1.0
                                      : static_cast<qreal>(completed)
                                            / static_cast<qreal>(totalWork);
              item.applyProgress(generation, value);
            });
          };

          auto analysisResult = SupportAnalyzer{}.analyze(
              *reader,
              analysisOptions,
              analysisCallbacks);
          const bool analysisCancelled = analysisResult.cancelled
                                         || stopToken.stop_requested();
          if (!analysisResult.ok) {
            trace3d(
                analysisCancelled ? logging::Level::kWarn : logging::Level::kError,
                "support_analysis",
                analysisCancelled ? "cancelled" : "failed",
                analysisResult.error,
                {{"generation", text(generation)},
                 {"duration_ms", text(elapsedMilliseconds(analysisStarted))},
                 {"source_layers", text(reader->layerCount())}});
            post([generation,
                  analysisCancelled,
                  message = QString::fromStdString(analysisResult.error)](
                     QmlGlItem& item) mutable {
              item.finishLoad(
                  generation,
                  std::move(message),
                  analysisCancelled);
            });
            return;
          }

          const auto& summary = analysisResult.summary;
          trace3d(
              logging::Level::kDebug,
              "support_analysis",
              "phase_detected",
              {},
              {{"generation", text(generation)},
               {"raft_last_layer", text(summary.raftLastLayer + 1u)},
               {"first_model_layer", text(summary.firstModelLayer + 1u)},
               {"last_support_layer", text(summary.lastSupportLayer + 1u)}});
          trace3d(
              logging::Level::kDebug,
              "support_analysis",
              "completed",
              {},
              {{"generation", text(generation)},
               {"duration_ms", text(elapsedMilliseconds(analysisStarted))},
               {"source_layers", text(reader->layerCount())},
               {"components", text(summary.componentCount)},
               {"candidate_nodes", text(summary.candidateNodeCount)},
               {"accepted_nodes", text(summary.acceptedNodeCount)},
               {"continuations", text(summary.continuationEdgeCount)},
               {"splits", text(summary.splitEdgeCount)},
               {"braces", text(summary.braceEdgeCount)},
               {"model_contacts", text(summary.modelContactEdgeCount)},
               {"raft_runs", text(summary.raftRunCount)},
               {"support_runs", text(summary.supportRunCount)},
               {"free_support_runs", text(summary.freeSupportRunCount)},
               {"projected_support_runs", text(summary.projectedSupportRunCount)},
               {"projected_contact_pixels", text(summary.projectedContactPixelCount)},
               {"rejected_projection_runs", text(summary.rejectedProjectionRunCount)},
               {"rejected_growth_pixels", text(summary.rejectedGrowthPixelCount)},
               {"untapered_model_contacts", text(summary.untaperedModelContactCount)},
               {"contacts_without_valid_projection",
                text(summary.contactsWithoutValidProjectionCount)},
               {"maximum_contact_growth_ratio",
                text(summary.maximumContactGrowthRatio)},
               {"terminal_support_stops", text(summary.terminalSupportStopCount)},
               {"expanding_model_contacts",
                text(summary.expandingModelContactCount)},
               {"maximum_model_expansion_ratio",
                text(summary.maximumModelExpansionRatio)},
               {"forced_semantic_samples",
                text(summary.forcedSemanticSampleCount)},
               {"reverse_model_seeds", text(summary.reverseModelSeedCount)},
               {"reverse_model_continuations",
                text(summary.reverseModelContinuationCount)},
               {"bidirectional_mixed_components",
                text(summary.bidirectionalMixedComponentCount)},
               {"vulkan_compute_compiled",
                summary.vulkanComputeCompiled ? "true" : "false"},
               {"vulkan_compute_active",
                summary.vulkanComputeActive ? "true" : "false"},
               {"vulkan_device", summary.vulkanDeviceName},
               {"vulkan_eligible_jobs", text(summary.vulkanEligibleJobCount)},
               {"vulkan_submitted_jobs", text(summary.vulkanSubmittedJobCount)},
               {"vulkan_gpu_jobs", text(summary.vulkanGpuJobCount)},
               {"vulkan_cpu_fallback_jobs",
                text(summary.vulkanCpuFallbackJobCount)},
               {"vulkan_dispatches", text(summary.vulkanDispatchCount)},
               {"vulkan_dispatch_failures",
                text(summary.vulkanDispatchFailureCount)},
               {"vulkan_max_batch_jobs",
                text(summary.vulkanMaximumBatchJobCount)},
               {"vulkan_upload_bytes", text(summary.vulkanUploadBytes)},
               {"vulkan_readback_bytes", text(summary.vulkanReadbackBytes)},
               {"vulkan_host_prepare_us",
                text(summary.vulkanHostPreparationMicroseconds)},
               {"vulkan_queue_wait_us",
                text(summary.vulkanQueueWaitMicroseconds)},
               {"vulkan_batch_execution_us",
                text(summary.vulkanBatchExecutionMicroseconds)},
               {"vulkan_run_source_jobs", text(summary.vulkanRunSourceJobCount)},
               {"vulkan_resident_reference_uploads",
                text(summary.vulkanResidentReferenceUploadCount)},
               {"vulkan_resident_reference_reuses",
                text(summary.vulkanResidentReferenceReuseCount)},
               {"vulkan_submitted_workgroups",
                text(summary.vulkanSubmittedWorkgroupCount)},
               {"vulkan_semantic_layer_batch_calls",
                text(summary.vulkanSemanticLayerBatchCallCount)},
               {"vulkan_semantic_layer_batch_jobs",
                text(summary.vulkanSemanticLayerBatchJobCount)},
               {"support_preparation_window",
                text(summary.supportPreparationWindowCapacity)},
               {"support_prepared_layers", text(summary.supportPreparedLayerCount)},
               {"support_max_preparation_inflight",
                text(summary.supportMaximumPreparationInflight)},
               {"support_prepare_load_us",
                text(summary.supportPreparationLoadMicroseconds)},
               {"support_prepare_describe_us",
                text(summary.supportPreparationDescribeMicroseconds)},
               {"support_forward_semantic_us",
                text(summary.supportForwardSemanticMicroseconds)},
               {"support_reverse_semantic_us",
                text(summary.supportReverseSemanticMicroseconds)},
               {"support_forward_classification_us",
                text(summary.supportForwardClassificationMicroseconds)},
               {"support_forward_commit_us",
                text(summary.supportForwardCommitMicroseconds)},
               {"support_forward_lineage_us",
                text(summary.supportForwardLineageMicroseconds)},
               {"support_forward_lineage_commit_us",
                text(summary.supportForwardLineageCommitMicroseconds)},
               {"support_reverse_prepare_us",
                text(summary.supportReversePreparationMicroseconds)},
               {"support_reverse_commit_us",
                text(summary.supportReverseCommitMicroseconds)},
               {"support_semantic_evidence_us",
                text(summary.supportSemanticEvidenceMicroseconds)},
               {"support_semantic_evidence_lots",
                text(summary.supportSemanticEvidenceLotCount)},
               {"support_semantic_evidence_layer_pairs",
                text(summary.supportSemanticEvidenceLayerPairCount)},
               {"support_semantic_evidence_edges",
                text(summary.supportSemanticEvidenceEdgeCount)}});
          supportAnalysis = std::make_shared<SupportAnalysisResult>(
              std::move(analysisResult));
          meshWork = baseMeshWork + forcedSemanticSampleCount(
              reader->layerCount(), supportAnalysis->forcedSampleLayers);
          totalWork = analysisWork + meshWork;
        } else {
          trace3d(
              logging::Level::kDebug,
              "support_analysis",
              "skipped",
              {},
              {{"generation", text(generation)},
               {"reason", "option_disabled"}});
        }

        post([generation](QmlGlItem& item) {
          item.applyLoadingPhase(generation, QmlGlItem::tr("Creating 3D view…"));
        });

        LayerStackMesher mesher;
        MeshBuildOptions options;
        options.pitchXMm = pitchX;
        options.pitchYMm = pitchY;
        options.pitchZMm = pitchZ;
        options.chunkLayerCount = kChunkLayers;
        options.layerStride = kViewerLayerStride;
        options.workerCount = static_cast<std::size_t>(std::clamp(
            workerCount,
            static_cast<int>(kMinimumMeshWorkerCount),
            static_cast<int>(kMaximumMeshWorkerCount)));
        options.cutSurfaceMode = CutSurfaceMode::Open;
        options.classifySupports = static_cast<bool>(supportAnalysis);
        if (supportAnalysis) {
          const auto analysis = supportAnalysis;
          options.forcedSampleLayers = analysis->forcedSampleLayers;
          options.supportMaskProvider = [analysis, generation](
              std::size_t layer,
              const photons::BinaryMask& material,
              photons::BinaryMask& supportMask,
              std::string& providerError) {
            const bool ok = materializeSupportMask(
                *analysis, layer, material, supportMask, providerError);
            if (!ok) {
              trace3d(
                  logging::Level::kError,
                  "support_analysis",
                  "materialization_failed",
                  providerError,
                  {{"generation", text(generation)},
                   {"layer", text(layer + 1)}});
            }
            return ok;
          };
        }

        const auto meshStarted = std::chrono::steady_clock::now();
        std::size_t generatedChunks = 0;
        std::size_t generatedSurfaceQuads = 0;
        std::size_t generatedTriangles = 0;
        std::size_t generatedCompactBytes = 0;
        std::size_t generatedLegacyBytes = 0;
        trace3d(
            logging::Level::kDebug,
            "mesher",
            "build_started",
            {},
            {{"generation", text(generation)},
             {"source_layers", text(reader->layerCount())},
             {"layer_step", text(options.layerStride)},
             {"chunk_layers", text(options.chunkLayerCount)},
             {"requested_workers", text(options.workerCount)},
             {"support_semantics", supportAnalysis ? "global" : "disabled"},
             {"base_sample_count", text(baseMeshWork)},
             {"forced_semantic_sample_count", text(meshWork - baseMeshWork)},
             {"effective_sample_count", text(meshWork)}});

        int lastReportedPercent = -1;
        int lastLoggedDecile = -1;
        MeshBuildCallbacks callbacks;
        callbacks.isCancelled = [&] { return stopToken.stop_requested(); };
        callbacks.progress = [&](std::size_t completed, std::size_t total) {
          const int percent = total == 0 ? 100 : static_cast<int>((completed * 100) / total);
          const int decile = percent / 10;
          if (decile != lastLoggedDecile || completed == total) {
            lastLoggedDecile = decile;
            trace3d(
                logging::Level::kDebug,
                "mesher",
                "build_progress",
                {},
                {{"generation", text(generation)},
                 {"completed_samples", text(completed)},
                 {"total_samples", text(total)},
                 {"percent", text(percent)},
                 {"duration_ms", text(elapsedMilliseconds(meshStarted))}});
          }
          if (percent == lastReportedPercent && completed != total) {
            return;
          }
          lastReportedPercent = percent;
          post([generation,
                analysisWork,
                analysisCompletionFloor,
                completed,
                totalWork](QmlGlItem& item) {
            const qreal rawValue = totalWork == 0
                                       ? 1.0
                                       : static_cast<qreal>(analysisWork + completed)
                                             / static_cast<qreal>(totalWork);
            item.applyProgress(generation, std::max(analysisCompletionFloor, rawValue));
          });
        };
        callbacks.consumeChunk = [&](photons::MeshChunk&& chunk) {
          const auto bounds = chunk.bounds;
          const auto layers = chunk.layers;
          const auto surfaceQuads = chunk.surfaceQuadCount();
          const auto triangles = chunk.triangleCount();
          const auto compactBytes = UploadQueue::byteSize(chunk);
          const auto legacyBytes = UploadQueue::legacyEquivalentByteSize(chunk);
          const auto queueWaitStarted = std::chrono::steady_clock::now();
          while (!stopToken.stop_requested() && guard
                 && !queue->tryPush(std::move(chunk))) {
            post([](QmlGlItem& item) { item.scheduleRender(); });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
          }
          const auto queueWaitMs = elapsedMilliseconds(queueWaitStarted);
          if (stopToken.stop_requested() || !guard) {
            return false;
          }
          ++generatedChunks;
          generatedSurfaceQuads += surfaceQuads;
          generatedTriangles += triangles;
          generatedCompactBytes += compactBytes;
          generatedLegacyBytes += legacyBytes;
          trace3d(
              logging::Level::kDebug,
              "mesher",
              "chunk_ready",
              {},
              {{"generation", text(generation)},
               {"first_layer", text(layers.first + 1)},
               {"last_layer", text(layers.last + 1)},
               {"surface_quads", text(surfaceQuads)},
               {"triangles", text(triangles)},
               {"compact_bytes", text(compactBytes)},
               {"legacy_equivalent_bytes", text(legacyBytes)},
               {"compression_ratio", text(
                    compactBytes == 0 ? 0.0
                                      : static_cast<double>(legacyBytes)
                                            / static_cast<double>(compactBytes))},
               {"queue_wait_ms", text(queueWaitMs)},
               {"chunk_index", text(generatedChunks)}});
          post([generation, bounds, triangles](QmlGlItem& item) {
            item.applyChunkStats(generation, bounds, triangles);
          });
          return true;
        };

        const auto result = mesher.build(
            *reader,
            photons::LayerRange{0, reader->layerCount() - 1},
            options,
            callbacks);
        const bool cancelled = result.cancelled || stopToken.stop_requested();
        trace3d(
            result.ok ? logging::Level::kDebug
                      : (cancelled ? logging::Level::kWarn : logging::Level::kError),
            "mesher",
            result.ok ? "build_completed" : (cancelled ? "build_cancelled" : "build_failed"),
            result.ok ? std::string{} : result.error,
            {{"generation", text(generation)},
             {"duration_ms", text(elapsedMilliseconds(meshStarted))},
             {"total_duration_ms", text(elapsedMilliseconds(loadStarted))},
             {"decoded_layers", text(result.decodedLayerCount)},
             {"source_layers", text(reader->layerCount())},
             {"layer_step", text(options.layerStride)},
             {"requested_workers", text(options.workerCount)},
             {"effective_workers", text(result.effectiveWorkerCount)},
             {"support_semantics", supportAnalysis ? "global" : "disabled"},
             {"base_sample_count", text(result.baseSampleCount)},
             {"forced_semantic_sample_count",
              text(result.forcedSemanticSampleCount)},
             {"effective_sample_count", text(result.effectiveSampleCount)},
             {"chunks", text(generatedChunks)},
             {"surface_quads", text(generatedSurfaceQuads)},
             {"triangles", text(generatedTriangles)},
             {"compact_bytes", text(generatedCompactBytes)},
             {"legacy_equivalent_bytes", text(generatedLegacyBytes)},
             {"compression_ratio", text(
                  generatedCompactBytes == 0 ? 0.0
                                             : static_cast<double>(generatedLegacyBytes)
                                                   / static_cast<double>(generatedCompactBytes))}});
        for (const auto& workerStats : result.workerStats) {
          trace3d(
              logging::Level::kDebug,
              "mesher",
              "worker_completed",
              {},
              {{"generation", text(generation)},
               {"worker_index", text(workerStats.workerIndex + 1)},
               {"tasks", text(workerStats.taskCount)},
               {"decoded_layers", text(workerStats.decodedLayerCount)},
               {"chunks", text(workerStats.chunkCount)},
               {"duration_ms", text(workerStats.durationMs)}});
        }
        post([generation,
              cancelled,
              supportAnalysis,
              message = QString::fromStdString(result.error)](QmlGlItem& item) mutable {
          item.finishLoad(
              generation,
              std::move(message),
              cancelled,
              std::move(supportAnalysis));
        });
      });
}

void QmlGlItem::applyDocumentMetadata(
    std::uint64_t generation,
    int totalLayers,
    qreal pitchZMm,
    QString machineName,
    std::shared_ptr<photons::pwsz::PwszArchiveReader> reader) {
  if (generation != sceneGeneration_) {
    return;
  }
  totalLayers_ = std::max(0, totalLayers);
  firstLayer_ = totalLayers_ > 0 ? 1 : 0;
  lastLayer_ = totalLayers_;
  layerHeightMm_ = pitchZMm > 0.0 ? pitchZMm : 0.05;
  machineName_ = std::move(machineName);
  archiveReader_ = std::move(reader);
  emit documentChanged();
  emit visibleRangeChanged();
}

void QmlGlItem::applyProgress(std::uint64_t generation, qreal progress) {
  if (generation != sceneGeneration_) {
    return;
  }
  const qreal bounded = std::clamp(progress, qreal{0.0}, qreal{1.0});
  if (qFuzzyCompare(progress_ + 1.0, bounded + 1.0)) {
    return;
  }
  progress_ = bounded;
  emit progressChanged();
}

void QmlGlItem::applyLoadingPhase(
    std::uint64_t generation,
    QString phase) {
  if (generation != sceneGeneration_ || loadingPhase_ == phase) {
    return;
  }
  loadingPhase_ = std::move(phase);
  emit loadingPhaseChanged();
}

void QmlGlItem::includeBounds(const photons::MeshBounds& bounds) noexcept {
  if (!bounds.valid()) {
    return;
  }
  bounds_.include(bounds.minX, bounds.minY, bounds.minZ);
  bounds_.include(bounds.maxX, bounds.maxY, bounds.maxZ);
}

void QmlGlItem::applyChunkStats(
    std::uint64_t generation,
    const photons::MeshBounds& bounds,
    std::size_t triangles) {
  if (generation != sceneGeneration_) {
    return;
  }
  includeBounds(bounds);
  ++loadedChunkCount_;
  triangleCount_ += static_cast<qulonglong>(triangles);
  if (!cameraTouched_ && bounds_.valid()) {
    camera_.fit(bounds_);
  }
  emit meshStatsChanged();
  scheduleRender();
}

void QmlGlItem::applyGpuFailure(
    std::uint64_t generation,
    QString errorString) {
  if (generation != sceneGeneration_) {
    return;
  }
  if (worker_.joinable()) {
    worker_.request_stop();
  }
  uploadQueue_->clear();
  {
    std::scoped_lock lock(cutRequestMutex_);
    cutGeneration_.fetch_add(1, std::memory_order_release);
    cutRequest_.reset();
    std::scoped_lock resultLock(cutResultMutex_);
    readyCutBatch_.reset();
  }
  cutRequestChanged_.notify_all();
  loading_ = false;
  supportAnalysis_.reset();
  sceneHasSupportSemantics_ = false;
  errorString_ = std::move(errorString);
  ++sceneGeneration_;
  trace3d(
      logging::Level::kError,
      "viewer",
      "gpu_generation_aborted",
      errorString_.toStdString(),
      {{"failed_generation", text(generation)},
       {"next_generation", text(sceneGeneration_)}});
  emit loadingChanged();
  emit errorStringChanged();
  scheduleRender();
}

void QmlGlItem::finishLoad(
    std::uint64_t generation,
    QString errorString,
    bool cancelled,
    std::shared_ptr<const SupportAnalysisResult> supportAnalysis) {
  if (generation != sceneGeneration_ || cancelled) {
    return;
  }
  loading_ = false;
  if (!errorString.isEmpty()) {
    supportAnalysis_.reset();
    sceneHasSupportSemantics_ = false;
    errorString_ = std::move(errorString);
    emit errorStringChanged();
  } else {
    supportAnalysis_ = std::move(supportAnalysis);
    sceneHasSupportSemantics_ = static_cast<bool>(supportAnalysis_);
    progress_ = 1.0;
    if (!cameraTouched_ && bounds_.valid()) {
      camera_.fit(bounds_);
    }
    emit progressChanged();
  }
  emit loadingChanged();
  if (errorString_.isEmpty()) {
    requestCutSurfaceRebuild();
  } else {
    scheduleRender();
  }
}

void QmlGlItem::orbitPixels(qreal deltaX, qreal deltaY) {
  cameraTouched_ = true;
  camera_.orbit(-static_cast<double>(deltaX) * 0.008,
                -static_cast<double>(deltaY) * 0.008);
  scheduleRender();
}

void QmlGlItem::panPixels(qreal deltaX, qreal deltaY) {
  cameraTouched_ = true;
  const double scale = std::max(0.0001, camera_.distance() * 0.0015);
  camera_.pan(-static_cast<double>(deltaX) * scale,
              static_cast<double>(deltaY) * scale);
  scheduleRender();
}

void QmlGlItem::zoomSteps(qreal steps) {
  cameraTouched_ = true;
  camera_.zoom(static_cast<double>(steps));
  scheduleRender();
}

void QmlGlItem::resetView() {
  cameraTouched_ = false;
  if (bounds_.valid()) {
    camera_.fit(bounds_);
  }
  scheduleRender();
}

void QmlGlItem::scheduleRender() {
  update();
}

} // namespace accloud::render3d
