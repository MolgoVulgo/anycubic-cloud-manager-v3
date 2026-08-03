#include "domain/photons/BinaryMask.h"
#include "domain/photons/LayerMaskSource.h"
#include "domain/photons/MeshChunk.h"
#include "render3d/gl/MeshlessVolume.h"
#include "render3d/gl/Renderer.h"
#include "render3d/gl/UploadQueue.h"
#include "render3d/meshing/LayerStackMesher.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

class VectorSource final : public accloud::photons::LayerMaskSource {
public:
  explicit VectorSource(std::vector<accloud::photons::BinaryMask> layers)
      : layers_(std::move(layers)) {}

  std::size_t layerCount() const noexcept override { return layers_.size(); }
  std::uint32_t width() const noexcept override { return layers_.front().width(); }
  std::uint32_t height() const noexcept override { return layers_.front().height(); }
  bool supportsConcurrentMaskLoads() const noexcept override { return true; }

  std::optional<accloud::photons::BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) override {
    if (layerNumber >= layers_.size()) {
      error = "layer outside test source";
      return std::nullopt;
    }
    return layers_[layerNumber];
  }

private:
  std::vector<accloud::photons::BinaryMask> layers_;
};

class ConcurrentProbeSource final : public accloud::photons::LayerMaskSource {
public:
  explicit ConcurrentProbeSource(std::vector<accloud::photons::BinaryMask> layers)
      : layers_(std::move(layers)) {}

  std::size_t layerCount() const noexcept override { return layers_.size(); }
  std::uint32_t width() const noexcept override { return layers_.front().width(); }
  std::uint32_t height() const noexcept override { return layers_.front().height(); }
  bool supportsConcurrentMaskLoads() const noexcept override { return true; }

  std::optional<accloud::photons::BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) override {
    if (layerNumber >= layers_.size()) {
      error = "layer outside concurrent probe source";
      return std::nullopt;
    }
    const int active = activeLoads_.fetch_add(1, std::memory_order_relaxed) + 1;
    int observed = maximumConcurrentLoads_.load(std::memory_order_relaxed);
    while (active > observed
           && !maximumConcurrentLoads_.compare_exchange_weak(
               observed, active, std::memory_order_relaxed)) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
    auto mask = layers_[layerNumber];
    activeLoads_.fetch_sub(1, std::memory_order_relaxed);
    return mask;
  }

  int maximumConcurrentLoads() const noexcept {
    return maximumConcurrentLoads_.load(std::memory_order_relaxed);
  }

private:
  std::vector<accloud::photons::BinaryMask> layers_;
  std::atomic<int> activeLoads_{0};
  std::atomic<int> maximumConcurrentLoads_{0};
};

accloud::photons::MeshChunk makeChunk(std::size_t first, std::size_t last) {
  accloud::photons::MeshChunk chunk;
  chunk.layers = {first, last};
  chunk.vertices = {
      {0.0F, 0.0F, static_cast<float>(first), 0.0F, 0.0F, 1.0F},
      {1.0F, 0.0F, static_cast<float>(first), 0.0F, 0.0F, 1.0F},
      {1.0F, 1.0F, static_cast<float>(last + 1), 0.0F, 0.0F, 1.0F},
  };
  chunk.indices = {0, 1, 2};
  for (const auto& vertex : chunk.vertices) {
    chunk.bounds.include(vertex.x, vertex.y, vertex.z);
  }
  return chunk;
}

std::size_t totalTriangles(const std::vector<accloud::photons::MeshChunk>& chunks) {
  std::size_t total = 0;
  for (const auto& chunk : chunks) {
    total += chunk.triangleCount();
  }
  return total;
}

} // namespace

int main() {
  bool ok = true;
  ok &= require(accloud::render3d::MeshBuildOptions{}.workerCount == 4,
                "mesh generation must default to four workers");

  accloud::render3d::UploadQueue queue(2, 1024 * 1024);
  ok &= require(queue.tryPush(makeChunk(0, 31)), "first upload chunk must fit");
  ok &= require(queue.tryPush(makeChunk(32, 63)), "second upload chunk must fit");
  ok &= require(!queue.tryPush(makeChunk(64, 95)), "queue chunk budget must be enforced");
  ok &= require(queue.pendingChunks() == 2 && queue.pendingBytes() > 0,
                "queue accounting must report pending uploads");
  const auto drained = queue.takeAll();
  ok &= require(drained.size() == 2 && queue.pendingChunks() == 0
                    && queue.pendingBytes() == 0,
                "draining the upload queue must reset its accounting");

  accloud::render3d::Renderer renderer;
  renderer.setDocument(1247, 0.05);
  renderer.appendChunk(makeChunk(0, 31));
  renderer.appendChunk(makeChunk(384, 415));
  renderer.appendChunk(makeChunk(416, 447));
  renderer.appendChunk(makeChunk(992, 1023));
  renderer.appendChunk(makeChunk(1024, 1055));
  std::string error;
  ok &= require(renderer.setVisibleLayersOneBased(415, 1021, error),
                "renderer must accept a one-based visible range: " + error);
  const auto plan = renderer.framePlan();
  ok &= require(plan.layers.first == 414 && plan.layers.last == 1020,
                "render plan must preserve exact zero-based clip layers");
  ok &= require(plan.minimumZ == 20.7F && plan.maximumZ == 51.05F,
                "render plan must expose exact Z clip planes");
  ok &= require(plan.chunkIndices.size() == 3,
                "render plan must select only chunks intersecting the layer range");
  ok &= require(renderer.volume().triangleCount() == 5,
                "renderer volume must account for uploaded triangles");

  std::vector<accloud::photons::BinaryMask> masks;
  for (int layer = 0; layer < 4; ++layer) {
    accloud::photons::BinaryMask mask(4, 4);
    mask.set(1, 1, true);
    mask.set(2, 1, true);
    mask.set(1, 2, true);
    mask.set(2, 2, true);
    masks.push_back(std::move(mask));
  }
  VectorSource source(std::move(masks));
  accloud::render3d::LayerStackMesher mesher;
  accloud::render3d::MeshBuildOptions options;
  options.pitchXMm = 0.05;
  options.pitchYMm = 0.05;
  options.pitchZMm = 0.05;
  options.chunkLayerCount = 2;

  std::vector<accloud::photons::MeshChunk> streamed;
  std::size_t lastCompleted = 0;
  accloud::render3d::MeshBuildCallbacks callbacks;
  callbacks.consumeChunk = [&](accloud::photons::MeshChunk&& chunk) {
    streamed.push_back(std::move(chunk));
    return true;
  };
  callbacks.progress = [&](std::size_t completed, std::size_t total) {
    lastCompleted = completed;
    ok &= require(total == 4, "streamed mesher progress total must be stable");
  };
  const auto streamedResult = mesher.build(source, {0, 3}, options, callbacks);
  ok &= require(streamedResult.ok && !streamedResult.cancelled,
                "streamed mesh build must succeed");
  ok &= require(streamedResult.chunks.empty() && streamed.size() == 2,
                "streamed chunks must be consumed instead of retained twice");
  ok &= require(lastCompleted == 4,
                "streamed mesh build must report completion of all layers");

  accloud::render3d::MeshBuildOptions sampledOptions = options;
  sampledOptions.layerStride = 2;
  sampledOptions.chunkLayerCount = 4;
  std::size_t sampledCompleted = 0;
  std::size_t sampledTotal = 0;
  accloud::render3d::MeshBuildCallbacks sampledCallbacks;
  sampledCallbacks.progress = [&](std::size_t completed, std::size_t total) {
    sampledCompleted = completed;
    sampledTotal = total;
  };
  const auto sampledResult = mesher.build(source, {0, 3}, sampledOptions, sampledCallbacks);
  ok &= require(sampledResult.ok && sampledResult.decodedLayerCount == 3,
                "stride-two preview must decode layers 0, 2 and the exact final layer 3");
  ok &= require(sampledTotal == 3 && sampledCompleted == 3,
                "stride-two progress must count sampled layers instead of source layers");
  ok &= require(sampledResult.chunks.size() == 1,
                "stride-two preview must preserve chunk spans in source-layer coordinates");
  if (!sampledResult.chunks.empty()) {
    const auto& sampledChunk = sampledResult.chunks.front();
    ok &= require(sampledChunk.layers.first == 0 && sampledChunk.layers.last == 3,
                  "sampled mesh chunk must cover the original selected layer range");
    ok &= require(std::abs(sampledChunk.bounds.maxZ - 0.2F) < 0.0001F,
                  "sampled mesh must preserve the original Z extent");
  }

  std::vector<accloud::photons::BinaryMask> parallelMasks;
  for (int layer = 0; layer < 16; ++layer) {
    accloud::photons::BinaryMask mask(4, 4);
    mask.set(1, 1, true);
    mask.set(2, 1, true);
    mask.set(1, 2, true);
    mask.set(2, 2, true);
    parallelMasks.push_back(std::move(mask));
  }
  VectorSource serialSource(parallelMasks);
  ConcurrentProbeSource parallelSource(std::move(parallelMasks));
  accloud::render3d::MeshBuildOptions serialOptions = options;
  serialOptions.chunkLayerCount = 4;
  serialOptions.workerCount = 1;
  const auto serialResult = mesher.build(serialSource, {0, 15}, serialOptions);

  accloud::render3d::MeshBuildOptions parallelOptions = serialOptions;
  parallelOptions.workerCount = 4;
  std::vector<std::size_t> parallelProgress;
  accloud::render3d::MeshBuildCallbacks parallelCallbacks;
  parallelCallbacks.progress = [&](std::size_t completed, std::size_t) {
    parallelProgress.push_back(completed);
  };
  const auto parallelResult = mesher.build(
      parallelSource, {0, 15}, parallelOptions, parallelCallbacks);
  ok &= require(serialResult.ok && parallelResult.ok,
                "serial and four-worker mesh builds must both succeed");
  ok &= require(parallelResult.effectiveWorkerCount == 4
                    && parallelResult.workerStats.size() == 4,
                "four requested workers must be used when four chunks are available");
  ok &= require(serialResult.chunks.size() == parallelResult.chunks.size()
                    && totalTriangles(serialResult.chunks) == totalTriangles(parallelResult.chunks),
                "parallel chunk meshing must preserve serial geometry");
  std::size_t parallelTaskCount = 0;
  for (const auto& stats : parallelResult.workerStats) {
    parallelTaskCount += stats.taskCount;
  }
  ok &= require(parallelTaskCount == 4,
                "parallel worker statistics must account for every mesh chunk task");
  ok &= require(parallelSource.maximumConcurrentLoads() >= 2,
                "concurrent-capable sources must be read by multiple mesh workers");
  bool monotonicProgress = parallelProgress.size() == 16;
  for (std::size_t index = 0; monotonicProgress && index < parallelProgress.size(); ++index) {
    monotonicProgress = parallelProgress[index] == index + 1;
  }
  ok &= require(monotonicProgress,
                "parallel progress callbacks must remain serialized and monotonic");

  accloud::render3d::MeshBuildOptions invalidWorkerCount = options;
  invalidWorkerCount.workerCount = 0;
  ok &= require(!mesher.build(source, {0, 3}, invalidWorkerCount).ok,
                "zero mesh workers must be rejected explicitly");
  invalidWorkerCount.workerCount = 17;
  ok &= require(!mesher.build(source, {0, 3}, invalidWorkerCount).ok,
                "more than sixteen mesh workers must be rejected explicitly");

  accloud::render3d::MeshBuildOptions invalidStride = options;
  invalidStride.layerStride = 0;
  const auto invalidStrideResult = mesher.build(source, {0, 3}, invalidStride);
  ok &= require(!invalidStrideResult.ok,
                "zero layer stride must be rejected explicitly");

  bool cancel = false;
  accloud::render3d::MeshBuildCallbacks cancelCallbacks;
  cancelCallbacks.consumeChunk = [&](accloud::photons::MeshChunk&&) {
    cancel = true;
    return true;
  };
  cancelCallbacks.isCancelled = [&] { return cancel; };
  const auto cancelledResult = mesher.build(source, {0, 3}, options, cancelCallbacks);
  ok &= require(!cancelledResult.ok && cancelledResult.cancelled,
                "mesher cancellation must be explicit and non-successful");

  return ok ? 0 : 1;
}
