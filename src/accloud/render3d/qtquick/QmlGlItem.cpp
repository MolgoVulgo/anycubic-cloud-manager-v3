#include "render3d/qtquick/QmlGlItem.h"

#include "infra/logging/JsonlLogger.h"
#include "infra/photons/drivers/pwsz/PwszArchiveReader.h"
#include "render3d/meshing/LayerStackMesher.h"

#include <QFile>
#include <QMatrix4x4>
#include <QMetaObject>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
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
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace accloud::render3d {
namespace {

constexpr std::size_t kChunkLayers = 32;
constexpr std::size_t kUploadQueueChunks = 8;
constexpr std::size_t kUploadQueueBytes = 256u * 1024u * 1024u;
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
  std::unique_ptr<QOpenGLBuffer> vertices;
  std::unique_ptr<QOpenGLBuffer> indices;
  std::unique_ptr<QOpenGLVertexArrayObject> vertexArray;
  GLsizei indexCount = 0;
};

} // namespace

class GlFramebufferRenderer final : public QQuickFramebufferObject::Renderer {
public:
  QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    return new QOpenGLFramebufferObject(size, format);
  }

  void synchronize(QQuickFramebufferObject* item) override {
    auto* viewer = static_cast<QmlGlItem*>(item);
    if (generation_ != viewer->sceneGeneration_) {
      generation_ = viewer->sceneGeneration_;
      clearRequested_ = true;
      pendingUploads_.clear();
      trace3d(
          logging::Level::kDebug,
          "gpu",
          "scene_reset",
          {},
          {{"generation", text(generation_)}});
    }

    if (viewer->uploadQueue_) {
      auto queued = viewer->uploadQueue_->takeAll();
      for (auto& chunk : queued) {
        pendingUploads_.push_back(std::move(chunk));
      }
    }

    firstLayer_ = viewer->firstLayer_;
    lastLayer_ = viewer->lastLayer_;
    pitchZMm_ = static_cast<float>(viewer->layerHeightMm_);
    backgroundColor_ = viewer->backgroundColor_;
    meshColor_ = viewer->meshColor_;
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
      gpuChunks_.clear();
      clearRequested_ = false;
    }
    uploadPending();

    auto* context = QOpenGLContext::currentContext();
    if (context == nullptr) {
      return;
    }
    QOpenGLFunctions* functions = context->functions();
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

    const float minimumZ = firstLayer_ > 0
                               ? static_cast<float>(firstLayer_ - 1) * pitchZMm_
                               : 0.0F;
    const float maximumZ = lastLayer_ > 0
                               ? static_cast<float>(lastLayer_) * pitchZMm_
                               : 0.0F;

    program_->bind();
    program_->setUniformValue("u_mvp", mvp);
    program_->setUniformValue("u_meshColor", QVector4D(
        meshColor_.redF(), meshColor_.greenF(), meshColor_.blueF(), meshColor_.alphaF()));
    program_->setUniformValue("u_lightDirection", QVector3D(-0.4F, -0.6F, -0.7F).normalized());
    program_->setUniformValue("u_clipZ", QVector2D(minimumZ, maximumZ));

    for (const auto& chunk : gpuChunks_) {
      if (!intersects(chunk.layers, firstLayer_, lastLayer_)) {
        continue;
      }
      chunk.vertexArray->bind();
      functions->glDrawElements(
          GL_TRIANGLES,
          chunk.indexCount,
          GL_UNSIGNED_INT,
          nullptr);
      chunk.vertexArray->release();
    }
    program_->release();

    functions->glDisable(GL_DEPTH_TEST);
    QQuickOpenGLUtils::resetOpenGLState();
    if (loading_ || !pendingUploads_.empty()) {
      update();
    }
  }

private:
  void initializeIfNeeded() {
    if (program_) {
      return;
    }

    auto program = std::make_unique<QOpenGLShaderProgram>();
    static constexpr auto vertexShader = R"glsl(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
uniform mat4 u_mvp;
out vec3 v_normal;
out float v_worldZ;
void main() {
  v_normal = a_normal;
  v_worldZ = a_position.z;
  gl_Position = u_mvp * vec4(a_position, 1.0);
}
)glsl";
    static constexpr auto fragmentShader = R"glsl(
#version 330 core
in vec3 v_normal;
in float v_worldZ;
uniform vec4 u_meshColor;
uniform vec3 u_lightDirection;
uniform vec2 u_clipZ;
out vec4 fragColor;
void main() {
  if (v_worldZ < u_clipZ.x || v_worldZ > u_clipZ.y)
    discard;
  float diffuse = abs(dot(normalize(v_normal), -u_lightDirection));
  float light = 0.28 + 0.72 * diffuse;
  fragColor = vec4(u_meshColor.rgb * light, u_meshColor.a);
}
)glsl";

    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader)
        || !program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader)
        || !program->link()) {
      qWarning("Render3D shader initialization failed: %s", qPrintable(program->log()));
      trace3d(
          logging::Level::kError,
          "gpu",
          "shader_initialization_failed",
          "Unable to initialize the 3D shader program",
          {{"generation", text(generation_)},
           {"shader_log", program->log().toStdString()}});
      return;
    }
    program_ = std::move(program);
    trace3d(
        logging::Level::kDebug,
        "gpu",
        "shader_ready",
        {},
        {{"generation", text(generation_)}});
  }

  void uploadPending() {
    if (!program_) {
      return;
    }
    for (auto& chunk : pendingUploads_) {
      if (chunk.empty()) {
        continue;
      }

      const auto uploadStarted = std::chrono::steady_clock::now();
      const std::size_t vertexCount = chunk.vertices.size();
      const std::size_t triangleCount = chunk.triangleCount();
      const std::size_t uploadBytes = UploadQueue::byteSize(chunk);
      const photons::LayerRange layerRange = chunk.layers;

      GpuChunk gpu;
      gpu.layers = chunk.layers;
      gpu.bounds = chunk.bounds;
      gpu.indexCount = static_cast<GLsizei>(chunk.indices.size());
      gpu.vertices = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
      gpu.indices = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
      gpu.vertexArray = std::make_unique<QOpenGLVertexArrayObject>();

      if (!gpu.vertices->create() || !gpu.indices->create() || !gpu.vertexArray->create()) {
        qWarning("Render3D GPU buffer allocation failed");
        trace3d(
            logging::Level::kError,
            "gpu",
            "buffer_allocation_failed",
            "Unable to allocate GPU buffers for a 3D mesh chunk",
            {{"generation", text(generation_)},
             {"first_layer", text(layerRange.first + 1)},
             {"last_layer", text(layerRange.last + 1)},
             {"bytes", text(uploadBytes)}});
        continue;
      }

      gpu.vertexArray->bind();
      program_->bind();
      gpu.vertices->bind();
      gpu.vertices->setUsagePattern(QOpenGLBuffer::StaticDraw);
      gpu.vertices->allocate(
          chunk.vertices.data(),
          static_cast<int>(chunk.vertices.size() * sizeof(photons::MeshVertex)));
      program_->enableAttributeArray(0);
      program_->setAttributeBuffer(
          0,
          GL_FLOAT,
          static_cast<int>(offsetof(photons::MeshVertex, x)),
          3,
          sizeof(photons::MeshVertex));
      program_->enableAttributeArray(1);
      program_->setAttributeBuffer(
          1,
          GL_FLOAT,
          static_cast<int>(offsetof(photons::MeshVertex, nx)),
          3,
          sizeof(photons::MeshVertex));

      gpu.indices->bind();
      gpu.indices->setUsagePattern(QOpenGLBuffer::StaticDraw);
      gpu.indices->allocate(
          chunk.indices.data(),
          static_cast<int>(chunk.indices.size() * sizeof(std::uint32_t)));

      gpu.vertexArray->release();
      gpu.vertices->release();
      gpu.indices->release();
      program_->release();
      gpuChunks_.push_back(std::move(gpu));
      trace3d(
          logging::Level::kDebug,
          "gpu",
          "chunk_uploaded",
          {},
          {{"generation", text(generation_)},
           {"first_layer", text(layerRange.first + 1)},
           {"last_layer", text(layerRange.last + 1)},
           {"vertices", text(vertexCount)},
           {"triangles", text(triangleCount)},
           {"bytes", text(uploadBytes)},
           {"duration_ms", text(elapsedMilliseconds(uploadStarted))},
           {"gpu_chunk_count", text(gpuChunks_.size())}});
    }
    pendingUploads_.clear();
  }

  std::unique_ptr<QOpenGLShaderProgram> program_;
  std::vector<GpuChunk> gpuChunks_;
  std::vector<photons::MeshChunk> pendingUploads_;
  std::uint64_t generation_ = 0;
  bool clearRequested_ = false;
  bool loading_ = false;
  int firstLayer_ = 0;
  int lastLayer_ = 0;
  float pitchZMm_ = 0.05F;
  float cameraDistance_ = 100.0F;
  QColor backgroundColor_;
  QColor meshColor_;
  QVector3D cameraPosition_;
  QVector3D cameraTarget_;
};

QmlGlItem::QmlGlItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent),
      uploadQueue_(std::make_shared<UploadQueue>(
          kUploadQueueChunks,
          kUploadQueueBytes)) {
  setMirrorVertically(true);
}

QmlGlItem::~QmlGlItem() {
  stopWorker();
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
  scheduleRender();
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
  scheduleRender();
}

void QmlGlItem::setLayerStep(int step) {
  step = std::clamp(step, 1, 8);
  if (layerStep_ == step) {
    return;
  }
  layerStep_ = step;
  emit layerStepChanged();
  trace3d(
      logging::Level::kDebug,
      "viewer",
      "sampling_changed",
      {},
      {{"generation", text(sceneGeneration_)}, {"layer_step", text(layerStep_)}});
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

void QmlGlItem::stopWorker() {
  if (!worker_.joinable()) {
    return;
  }
  worker_.request_stop();
  uploadQueue_->clear();
  worker_.join();
}

void QmlGlItem::resetDocumentState() {
  loading_ = true;
  progress_ = 0.0;
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
  ++sceneGeneration_;
  emit loadingChanged();
  emit progressChanged();
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
  const int layerStep = layerStep_;
  const int workerCount = workerCount_;
  const QString inputPath = localPathFromInput(sourcePath_).trimmed();
  trace3d(
      logging::Level::kDebug,
      "viewer",
      "load_requested",
      {},
      {{"generation", text(generation)},
       {"layer_step", text(layerStep)},
       {"worker_count", text(workerCount)}});
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
      [guard, queue, generation, inputPath, layerStep, workerCount](std::stop_token stopToken) {
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

        photons::pwsz::PwszArchiveReader reader;
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
        if (!reader.open(archivePath, error)) {
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

        const auto& meta = reader.meta();
        const int totalLayers = static_cast<int>(reader.layerCount());
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
             {"width", text(reader.width())},
             {"height", text(reader.height())},
             {"pitch_x_mm", text(static_cast<double>(pitchX))},
             {"pitch_y_mm", text(static_cast<double>(pitchY))},
             {"pitch_z_mm", text(static_cast<double>(pitchZ))}});
        post([generation,
              totalLayers,
              pitchZ,
              machine = QString::fromStdString(meta.machineName)](QmlGlItem& item) mutable {
          item.applyDocumentMetadata(generation, totalLayers, pitchZ, std::move(machine));
        });

        if (totalLayers <= 0) {
          post([generation](QmlGlItem& item) {
            item.finishLoad(generation, QObject::tr("The PWSZ archive contains no layers."), false);
          });
          return;
        }

        LayerStackMesher mesher;
        MeshBuildOptions options;
        options.pitchXMm = pitchX;
        options.pitchYMm = pitchY;
        options.pitchZMm = pitchZ;
        options.chunkLayerCount = kChunkLayers;
        options.layerStride = static_cast<std::size_t>(std::max(1, layerStep));
        options.workerCount = static_cast<std::size_t>(std::clamp(
            workerCount,
            static_cast<int>(kMinimumMeshWorkerCount),
            static_cast<int>(kMaximumMeshWorkerCount)));
        options.cutSurfaceMode = CutSurfaceMode::Open;

        const auto meshStarted = std::chrono::steady_clock::now();
        std::size_t generatedChunks = 0;
        std::size_t generatedTriangles = 0;
        std::size_t generatedBytes = 0;
        trace3d(
            logging::Level::kDebug,
            "mesher",
            "build_started",
            {},
            {{"generation", text(generation)},
             {"source_layers", text(reader.layerCount())},
             {"layer_step", text(options.layerStride)},
             {"chunk_layers", text(options.chunkLayerCount)},
             {"requested_workers", text(options.workerCount)}});

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
          post([generation, value = total == 0
                                        ? 1.0
                                        : static_cast<qreal>(completed)
                                              / static_cast<qreal>(total)](QmlGlItem& item) {
            item.applyProgress(generation, value);
          });
        };
        callbacks.consumeChunk = [&](photons::MeshChunk&& chunk) {
          const auto bounds = chunk.bounds;
          const auto layers = chunk.layers;
          const auto triangles = chunk.triangleCount();
          const auto vertices = chunk.vertices.size();
          const auto bytes = UploadQueue::byteSize(chunk);
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
          generatedTriangles += triangles;
          generatedBytes += bytes;
          trace3d(
              logging::Level::kDebug,
              "mesher",
              "chunk_ready",
              {},
              {{"generation", text(generation)},
               {"first_layer", text(layers.first + 1)},
               {"last_layer", text(layers.last + 1)},
               {"vertices", text(vertices)},
               {"triangles", text(triangles)},
               {"bytes", text(bytes)},
               {"queue_wait_ms", text(queueWaitMs)},
               {"chunk_index", text(generatedChunks)}});
          post([generation, bounds, triangles](QmlGlItem& item) {
            item.applyChunkStats(generation, bounds, triangles);
          });
          return true;
        };

        const auto result = mesher.build(
            reader,
            photons::LayerRange{0, reader.layerCount() - 1},
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
             {"source_layers", text(reader.layerCount())},
             {"layer_step", text(options.layerStride)},
             {"requested_workers", text(options.workerCount)},
             {"effective_workers", text(result.effectiveWorkerCount)},
             {"chunks", text(generatedChunks)},
             {"triangles", text(generatedTriangles)},
             {"mesh_bytes", text(generatedBytes)}});
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
              message = QString::fromStdString(result.error)](QmlGlItem& item) mutable {
          item.finishLoad(generation, std::move(message), cancelled);
        });
      });
}

void QmlGlItem::applyDocumentMetadata(
    std::uint64_t generation,
    int totalLayers,
    qreal pitchZMm,
    QString machineName) {
  if (generation != sceneGeneration_) {
    return;
  }
  totalLayers_ = std::max(0, totalLayers);
  firstLayer_ = totalLayers_ > 0 ? 1 : 0;
  lastLayer_ = totalLayers_;
  layerHeightMm_ = pitchZMm > 0.0 ? pitchZMm : 0.05;
  machineName_ = std::move(machineName);
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

void QmlGlItem::finishLoad(
    std::uint64_t generation,
    QString errorString,
    bool cancelled) {
  if (generation != sceneGeneration_ || cancelled) {
    return;
  }
  loading_ = false;
  if (!errorString.isEmpty()) {
    errorString_ = std::move(errorString);
    emit errorStringChanged();
  } else {
    progress_ = 1.0;
    if (!cameraTouched_ && bounds_.valid()) {
      camera_.fit(bounds_);
    }
    emit progressChanged();
  }
  emit loadingChanged();
  scheduleRender();
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
