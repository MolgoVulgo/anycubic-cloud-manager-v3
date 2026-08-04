#include "domain/photons/BinaryMask.h"
#include "domain/photons/LayerMaskSource.h"
#include "domain/photons/MeshChunk.h"
#include "render3d/core/LayerSectionCache.h"
#include "render3d/gl/MeshlessVolume.h"
#include "render3d/gl/Renderer.h"
#include "render3d/gl/UploadQueue.h"
#include "render3d/meshing/LayerStackMesher.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <iostream>
#include <optional>
#include <set>
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

class UniformHeightSource final : public accloud::photons::LayerMaskSource {
public:
  explicit UniformHeightSource(std::size_t layers) : layers_(layers) {}

  std::size_t layerCount() const noexcept override { return layers_; }
  std::uint32_t width() const noexcept override { return 2; }
  std::uint32_t height() const noexcept override { return 2; }
  bool supportsConcurrentMaskLoads() const noexcept override { return true; }

  std::optional<accloud::photons::BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) override {
    if (layerNumber >= layers_) {
      error = "layer outside uniform height source";
      return std::nullopt;
    }
    accloud::photons::BinaryMask mask(2, 2);
    mask.set(0, 0, true);
    mask.set(1, 0, true);
    mask.set(0, 1, true);
    mask.set(1, 1, true);
    return mask;
  }

private:
  std::size_t layers_ = 0;
};

accloud::photons::MeshChunk makeChunk(std::size_t first, std::size_t last) {
  accloud::photons::MeshChunk chunk;
  chunk.layers = {first, last};
  chunk.rasterWidth = 1;
  chunk.rasterHeight = 1;
  chunk.pitchXMm = 1.0F;
  chunk.pitchYMm = 1.0F;
  chunk.pitchZMm = 1.0F;
  chunk.surfaces.push_back(accloud::photons::packZSurface(
      accloud::photons::PackedSurfaceFace::PositiveZ,
      0,
      0,
      1,
      0,
      1));
  for (const auto& vertex : accloud::photons::surfaceQuadCorners(
           chunk, chunk.surfaces.front())) {
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

std::size_t semanticSurfaceCount(
    const std::vector<accloud::photons::MeshChunk>& chunks,
    accloud::photons::PackedSurfaceSemantic semantic) {
  std::size_t total = 0;
  for (const auto& chunk : chunks) {
    for (const auto& surface : chunk.surfaces) {
      if (accloud::photons::packedSurfaceSemantic(surface) == semantic) {
        ++total;
      }
    }
  }
  return total;
}

struct MeshCoverageSignature {
  double surfaceAreaMm2 = 0.0;
  accloud::photons::MeshBounds bounds;
  std::size_t duplicateTriangles = 0;
};

using PointKey = std::array<std::uint32_t, 3>;
using TriangleKey = std::array<PointKey, 3>;

PointKey pointKey(const accloud::photons::MeshVertex& vertex) {
  return {
      std::bit_cast<std::uint32_t>(vertex.x),
      std::bit_cast<std::uint32_t>(vertex.y),
      std::bit_cast<std::uint32_t>(vertex.z),
  };
}

MeshCoverageSignature meshCoverage(
    const std::vector<accloud::photons::MeshChunk>& chunks) {
  MeshCoverageSignature signature;
  std::set<TriangleKey> triangles;
  for (const auto& chunk : chunks) {
    for (const auto& surface : chunk.surfaces) {
      const auto expanded = accloud::photons::expandSurfaceQuad(chunk, surface);
      for (const auto& vertex : expanded) {
        signature.bounds.include(vertex.x, vertex.y, vertex.z);
      }
      for (std::size_t index = 0; index < expanded.size(); index += 3) {
        const auto& a = expanded[index];
        const auto& b = expanded[index + 1];
        const auto& c = expanded[index + 2];
        const double abX = static_cast<double>(b.x) - a.x;
        const double abY = static_cast<double>(b.y) - a.y;
        const double abZ = static_cast<double>(b.z) - a.z;
        const double acX = static_cast<double>(c.x) - a.x;
        const double acY = static_cast<double>(c.y) - a.y;
        const double acZ = static_cast<double>(c.z) - a.z;
        const double crossX = abY * acZ - abZ * acY;
        const double crossY = abZ * acX - abX * acZ;
        const double crossZ = abX * acY - abY * acX;
        signature.surfaceAreaMm2 += 0.5 * std::sqrt(
            crossX * crossX + crossY * crossY + crossZ * crossZ);

        TriangleKey key{pointKey(a), pointKey(b), pointKey(c)};
        std::sort(key.begin(), key.end());
        if (!triangles.insert(key).second) {
          ++signature.duplicateTriangles;
        }
      }
    }
  }
  return signature;
}

bool sameBounds(
    const accloud::photons::MeshBounds& left,
    const accloud::photons::MeshBounds& right,
    float epsilon = 0.0001F) {
  return std::abs(left.minX - right.minX) <= epsilon
         && std::abs(left.minY - right.minY) <= epsilon
         && std::abs(left.minZ - right.minZ) <= epsilon
         && std::abs(left.maxX - right.maxX) <= epsilon
         && std::abs(left.maxY - right.maxY) <= epsilon
         && std::abs(left.maxZ - right.maxZ) <= epsilon;
}

} // namespace

int main() {
  bool ok = true;
  ok &= require(accloud::render3d::MeshBuildOptions{}.workerCount == 4,
                "mesh generation must default to four workers");
  ok &= require(accloud::render3d::MeshBuildOptions{}.chunkLayerCount == 8,
                "mesh generation must default to eight source layers per chunk");

  ok &= require(sizeof(accloud::photons::PackedSurfaceQuad) == 8,
                "packed GPU surfaces must remain eight-byte instances");
  const auto packedProbe = accloud::photons::packXSurface(
      accloud::photons::PackedSurfaceFace::PositiveX,
      10,
      20,
      30,
      2,
      7,
      accloud::photons::PackedSurfaceSemantic::Support);
  accloud::photons::MeshChunk packedProbeChunk;
  packedProbeChunk.layers = {100, 107};
  packedProbeChunk.pitchXMm = 0.05F;
  packedProbeChunk.pitchYMm = 0.06F;
  packedProbeChunk.pitchZMm = 0.07F;
  const auto packedProbeCorners = accloud::photons::surfaceQuadCorners(
      packedProbeChunk, packedProbe);
  ok &= require(
      accloud::photons::packedSurfaceFace(packedProbe)
              == accloud::photons::PackedSurfaceFace::PositiveX
          && accloud::photons::packedSurfaceSemantic(packedProbe)
                 == accloud::photons::PackedSurfaceSemantic::Support
          && std::abs(packedProbeCorners[0].x - 0.5F) < 0.0001F
          && std::abs(packedProbeCorners[0].y - 1.2F) < 0.0001F
          && std::abs(packedProbeCorners[2].z - 7.49F) < 0.0001F
          && packedProbeCorners[0].nx == 1.0F,
      "packed surface expansion must preserve exact grid coordinates and normals");

  accloud::render3d::GpuMemoryBudget gpuBudget(100);
  ok &= require(gpuBudget.tryReserve(60) && !gpuBudget.tryReserve(50)
                    && gpuBudget.residentBytes() == 60,
                "GPU budget must reject allocations before the driver is called");
  gpuBudget.release(20);
  ok &= require(gpuBudget.residentBytes() == 40,
                "GPU budget release must update resident bytes");
  gpuBudget.reset();
  ok &= require(gpuBudget.residentBytes() == 0,
                "GPU budget reset must release the complete generation accounting");

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

  const auto compactProbeChunk = makeChunk(0, 7);
  ok &= require(
      accloud::render3d::UploadQueue::byteSize(compactProbeChunk) == 8
          && accloud::render3d::UploadQueue::legacyEquivalentByteSize(compactProbeChunk)
                 == 120,
      "one packed quad must replace 120 legacy vertex/index bytes with eight bytes");

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
  ok &= require(renderer.volume().triangleCount() == 10,
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

  accloud::render3d::MeshBuildOptions semanticOptions;
  semanticOptions.pitchXMm = 1.0;
  semanticOptions.pitchYMm = 1.0;
  semanticOptions.pitchZMm = 0.25;
  semanticOptions.chunkLayerCount = 8;
  semanticOptions.workerCount = 1;
  semanticOptions.classifySupports = true;
  semanticOptions.supportMaximumSpanMm = 1.5;
  semanticOptions.supportMaximumAreaMm2 = 2.0;
  semanticOptions.supportRaftMaximumHeightMm = 1.0;

  const auto makePartAndStem = [](bool fuseStem) {
    accloud::photons::BinaryMask mask(9, 5);
    for (std::uint32_t y = 1; y < 4; ++y) {
      for (std::uint32_t x = 1; x < 4; ++x) {
        mask.set(x, y, true);
      }
    }
    mask.set(7, 2, true);
    if (fuseStem) {
      for (std::uint32_t x = 4; x < 7; ++x) {
        mask.set(x, 2, true);
      }
    }
    return mask;
  };

  VectorSource partOnlySource({
      makePartAndStem(true),
      makePartAndStem(true),
      makePartAndStem(true),
  });
  const auto partOnlyResult = mesher.build(
      partOnlySource, {0, 2}, semanticOptions);
  ok &= require(
      partOnlyResult.ok
          && semanticSurfaceCount(
                 partOnlyResult.chunks,
                 accloud::photons::PackedSurfaceSemantic::Support)
                 == 0,
      "a single connected part must remain model-colored");

  accloud::photons::BinaryMask tinyPart(3, 3);
  tinyPart.set(1, 1, true);
  VectorSource tinyPartSource({tinyPart, tinyPart, tinyPart});
  const auto tinyPartResult = mesher.build(
      tinyPartSource, {0, 2}, semanticOptions);
  ok &= require(
      tinyPartResult.ok
          && semanticSurfaceCount(
                 tinyPartResult.chunks,
                 accloud::photons::PackedSurfaceSemantic::Support)
                 == 0,
      "a sole narrow component is ambiguous and must remain model-colored");

  VectorSource stemSource({
      makePartAndStem(false),
      makePartAndStem(false),
      makePartAndStem(false),
  });
  const auto stemResult = mesher.build(stemSource, {0, 2}, semanticOptions);
  ok &= require(
      stemResult.ok
          && semanticSurfaceCount(
                 stemResult.chunks,
                 accloud::photons::PackedSurfaceSemantic::Support)
                 > 0
          && semanticSurfaceCount(
                 stemResult.chunks,
                 accloud::photons::PackedSurfaceSemantic::Model)
                 > 0,
      "a persistent narrow island beside a larger part must be tagged as estimated support");

  VectorSource fusedStemSource({
      makePartAndStem(false),
      makePartAndStem(false),
      makePartAndStem(true),
  });
  const auto fusedStemResult = mesher.build(
      fusedStemSource, {0, 2}, semanticOptions);
  bool fusedTopIsModel = fusedStemResult.ok;
  for (const auto& chunk : fusedStemResult.chunks) {
    for (const auto& surface : chunk.surfaces) {
      if (accloud::photons::packedSurfaceFace(surface)
              == accloud::photons::PackedSurfaceFace::PositiveZ
          && accloud::photons::packedSurfaceField(surface, 54u, 6u) == 3u) {
        fusedTopIsModel &= accloud::photons::packedSurfaceSemantic(surface)
                           == accloud::photons::PackedSurfaceSemantic::Model;
      }
    }
  }
  ok &= require(
      fusedTopIsModel,
      "material made ambiguous by a support-to-model fusion must keep the model semantic");

  accloud::photons::BinaryMask raft(5, 5);
  for (std::uint32_t y = 0; y < 5; ++y) {
    for (std::uint32_t x = 0; x < 5; ++x) {
      raft.set(x, y, true);
    }
  }
  accloud::photons::BinaryMask raftStem(5, 5);
  raftStem.set(2, 2, true);
  raftStem.set(2, 3, true);
  VectorSource raftSource({raft, raftStem, raftStem});
  const auto raftResult = mesher.build(raftSource, {0, 2}, semanticOptions);
  ok &= require(
      raftResult.ok
          && semanticSurfaceCount(
                 raftResult.chunks,
                 accloud::photons::PackedSurfaceSemantic::Support)
                 > 0,
      "an early broad footprint that contracts into narrow stems must expose an estimated raft/support semantic");

  accloud::photons::BinaryMask solidSectionMask(6, 6);
  for (std::uint32_t y = 1; y < 5; ++y) {
    for (std::uint32_t x = 1; x < 5; ++x) {
      solidSectionMask.set(x, y, true);
    }
  }
  VectorSource solidSectionSource({solidSectionMask});
  accloud::render3d::MeshBuildOptions sectionOptions = options;
  sectionOptions.pitchXMm = 0.1;
  sectionOptions.pitchYMm = 0.2;
  sectionOptions.pitchZMm = 0.05;
  const auto solidSection = mesher.buildCutSurface(
      solidSectionSource,
      0,
      7,
      accloud::render3d::CutSurfaceBoundary::Upper,
      sectionOptions);
  ok &= require(solidSection.ok && solidSection.decodedLayerCount == 1,
                "solid visible-layer section generation must decode one exact mask");
  if (solidSection.ok) {
    const auto coverage = meshCoverage({solidSection.chunk});
    bool positiveZOnly = !solidSection.chunk.surfaces.empty();
    for (const auto& surface : solidSection.chunk.surfaces) {
      positiveZOnly &= accloud::photons::packedSurfaceFace(surface)
                       == accloud::photons::PackedSurfaceFace::PositiveZ;
    }
    ok &= require(positiveZOnly,
                  "upper visible-layer sections must face positive Z");
    ok &= require(solidSection.chunk.layers.first == 7
                      && solidSection.chunk.layers.last == 7
                      && std::abs(solidSection.chunk.bounds.minZ - 0.35F) < 0.0001F
                      && std::abs(solidSection.chunk.bounds.maxZ - 0.35F) < 0.0001F,
                  "cut surface plane must be independent from the decoded mask layer");
    ok &= require(std::abs(coverage.surfaceAreaMm2 - 0.32) < 0.0001,
                  "a solid 4x4 section must remain fully filled");

    accloud::render3d::LayerSectionTemplate cachedTemplate;
    std::string cacheError;
    ok &= require(
        accloud::render3d::makeLayerSectionTemplate(
            solidSection.chunk, cachedTemplate, cacheError),
        "horizontal cut surfaces must convert to an orientation-independent cache template");
    ok &= require(sizeof(accloud::render3d::LayerSectionRect) == 8
                      && cachedTemplate.compactByteSize()
                             == solidSection.chunk.compactByteSize(),
                  "the CPU layer-section cache must keep one eight-byte record per rectangle");
    const auto cachedLower = accloud::render3d::materializeLayerSection(
        cachedTemplate,
        11,
        accloud::render3d::CutSurfaceBoundary::Lower,
        solidSection.chunk.pitchXMm,
        solidSection.chunk.pitchYMm,
        solidSection.chunk.pitchZMm);
    ok &= require(
        cachedLower.surfaceQuadCount() == solidSection.chunk.surfaceQuadCount()
            && !cachedLower.surfaces.empty()
            && accloud::photons::packedSurfaceFace(cachedLower.surfaces.front())
                   == accloud::photons::PackedSurfaceFace::NegativeZ
            && std::abs(cachedLower.bounds.minZ - 0.55F) < 0.0001F,
        "a cached section must be reusable at a new plane and with the opposite orientation");

    accloud::render3d::LayerSectionCache sectionCache(
        cachedTemplate.compactByteSize() * 2, 2);
    auto firstCached = sectionCache.insert(1, cachedTemplate);
    auto secondCached = sectionCache.insert(2, cachedTemplate);
    ok &= require(firstCached && secondCached && sectionCache.find(1),
                  "the bounded layer-section cache must retain recently used entries");
    (void)sectionCache.insert(3, cachedTemplate);
    const auto cacheStats = sectionCache.stats();
    ok &= require(cacheStats.entries == 2
                      && cacheStats.residentBytes
                             == cachedTemplate.compactByteSize() * 2
                      && cacheStats.evictions == 1
                      && !sectionCache.find(2),
                  "the layer-section cache must evict the least-recently-used entry within its byte budget");
  }

  accloud::photons::BinaryMask hollowSectionMask = solidSectionMask;
  for (std::uint32_t y = 2; y < 4; ++y) {
    for (std::uint32_t x = 2; x < 4; ++x) {
      hollowSectionMask.set(x, y, false);
    }
  }
  VectorSource hollowSectionSource({hollowSectionMask});
  const auto hollowSection = mesher.buildCutSurface(
      hollowSectionSource,
      0,
      3,
      accloud::render3d::CutSurfaceBoundary::Lower,
      sectionOptions);
  ok &= require(hollowSection.ok,
                "hollow visible-layer section generation must succeed");
  if (hollowSection.ok) {
    const auto coverage = meshCoverage({hollowSection.chunk});
    bool negativeZOnly = !hollowSection.chunk.surfaces.empty();
    for (const auto& surface : hollowSection.chunk.surfaces) {
      negativeZOnly &= accloud::photons::packedSurfaceFace(surface)
                       == accloud::photons::PackedSurfaceFace::NegativeZ;
    }
    ok &= require(negativeZOnly,
                  "lower visible-layer sections must face negative Z");
    ok &= require(std::abs(coverage.surfaceAreaMm2 - 0.24) < 0.0001,
                  "a hollow section must preserve its exact central cavity");
    ok &= require(coverage.duplicateTriangles == 0,
                  "cut surface greedy rectangles must not overlap");
  }

  accloud::photons::BinaryMask islandSectionMask(6, 6);
  islandSectionMask.set(1, 1, true);
  islandSectionMask.set(4, 4, true);
  VectorSource islandSectionSource({islandSectionMask});
  const auto islandSection = mesher.buildCutSurface(
      islandSectionSource,
      0,
      2,
      accloud::render3d::CutSurfaceBoundary::Upper,
      sectionOptions);
  ok &= require(islandSection.ok && islandSection.chunk.surfaceQuadCount() == 2,
                "separate support islands must remain separate section surfaces");
  if (islandSection.ok) {
    const auto coverage = meshCoverage({islandSection.chunk});
    ok &= require(std::abs(coverage.surfaceAreaMm2 - 0.04) < 0.0001,
                  "section surfaces must not fill empty space between islands");
  }

  accloud::photons::MeshChunk supportSectionChunk;
  supportSectionChunk.layers = {4, 4};
  supportSectionChunk.rasterWidth = 3;
  supportSectionChunk.rasterHeight = 3;
  supportSectionChunk.pitchXMm = 0.1F;
  supportSectionChunk.pitchYMm = 0.2F;
  supportSectionChunk.pitchZMm = 0.05F;
  supportSectionChunk.surfaces.push_back(accloud::photons::packZSurface(
      accloud::photons::PackedSurfaceFace::PositiveZ,
      0,
      1,
      2,
      1,
      2,
      accloud::photons::PackedSurfaceSemantic::Support));
  accloud::render3d::LayerSectionTemplate supportTemplate;
  std::string supportCacheError;
  const bool supportTemplateOk = accloud::render3d::makeLayerSectionTemplate(
      supportSectionChunk,
      supportTemplate,
      supportCacheError);
  const auto supportRematerialized = supportTemplateOk
                                         ? accloud::render3d::materializeLayerSection(
                                               supportTemplate,
                                               6,
                                               accloud::render3d::CutSurfaceBoundary::Lower,
                                               supportSectionChunk.pitchXMm,
                                               supportSectionChunk.pitchYMm,
                                               supportSectionChunk.pitchZMm)
                                         : accloud::photons::MeshChunk{};
  ok &= require(
      supportTemplateOk
          && !supportRematerialized.surfaces.empty()
          && accloud::photons::packedSurfaceSemantic(
                 supportRematerialized.surfaces.front())
                 == accloud::photons::PackedSurfaceSemantic::Support,
      "the eight-byte section cache must preserve support semantics independently "
      "from classification");

  const auto invalidSection = mesher.buildCutSurface(
      islandSectionSource,
      1,
      2,
      accloud::render3d::CutSurfaceBoundary::Upper,
      sectionOptions);
  ok &= require(!invalidSection.ok,
                "cut surface mask layers outside the source must fail explicitly");

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

  std::vector<accloud::photons::BinaryMask> coverageMasks;
  for (int layer = 0; layer < 64; ++layer) {
    accloud::photons::BinaryMask mask(8, 8);
    for (std::uint32_t y = 1; y < 7; ++y) {
      for (std::uint32_t x = 1; x < 7; ++x) {
        mask.set(x, y, true);
      }
    }
    if (layer >= 8 && layer < 56) {
      mask.set(3, 3, false);
      mask.set(4, 3, false);
      mask.set(3, 4, false);
      mask.set(4, 4, false);
    }
    if (layer >= 16 && layer < 48) {
      mask.set(0, 0, true);
    }
    coverageMasks.push_back(std::move(mask));
  }
  std::optional<MeshCoverageSignature> referenceCoverage;
  for (const std::size_t chunkLayers : {std::size_t{8}, std::size_t{16}, std::size_t{32}}) {
    VectorSource coverageSource(coverageMasks);
    accloud::render3d::MeshBuildOptions coverageOptions = options;
    coverageOptions.chunkLayerCount = chunkLayers;
    coverageOptions.workerCount = 4;
    const auto coverageResult = mesher.build(coverageSource, {0, 63}, coverageOptions);
    ok &= require(coverageResult.ok,
                  "mesh coverage build must succeed for every benchmark chunk size");
    if (!coverageResult.ok) {
      continue;
    }
    const auto coverage = meshCoverage(coverageResult.chunks);
    ok &= require(coverage.duplicateTriangles == 0,
                  "chunk boundaries must not emit overlapping duplicate triangles");
    if (!referenceCoverage) {
      referenceCoverage = coverage;
      continue;
    }
    ok &= require(sameBounds(referenceCoverage->bounds, coverage.bounds),
                  "chunk sizes 8, 16 and 32 must preserve identical mesh bounds");
    ok &= require(std::abs(referenceCoverage->surfaceAreaMm2 - coverage.surfaceAreaMm2)
                          <= 0.0001,
                  "chunk sizes 8, 16 and 32 must preserve identical exposed surface area");
  }

  UniformHeightSource fullHeightSource(5000);
  accloud::render3d::MeshBuildOptions fullHeightOptions = options;
  fullHeightOptions.pitchXMm = 0.05;
  fullHeightOptions.pitchYMm = 0.05;
  fullHeightOptions.pitchZMm = 0.05;
  fullHeightOptions.chunkLayerCount = 8;
  fullHeightOptions.workerCount = 4;
  const auto fullHeightResult = mesher.build(
      fullHeightSource, {0, 4999}, fullHeightOptions);
  std::size_t fullHeightCompactBytes = 0;
  std::size_t fullHeightLegacyBytes = 0;
  for (const auto& chunk : fullHeightResult.chunks) {
    fullHeightCompactBytes += chunk.compactByteSize();
    fullHeightLegacyBytes += chunk.legacyEquivalentByteSize();
  }
  ok &= require(fullHeightResult.ok && !fullHeightResult.chunks.empty(),
                "a complete 250 mm synthetic piece must be meshed without truncation");
  if (fullHeightResult.ok && !fullHeightResult.chunks.empty()) {
    ok &= require(
        std::abs(fullHeightResult.chunks.back().bounds.maxZ - 250.0F) < 0.0001F,
        "the 250 mm synthetic piece must preserve its complete Z extent");
    ok &= require(
        fullHeightCompactBytes > 0
            && fullHeightLegacyBytes == fullHeightCompactBytes * 15,
        "full-height compact geometry must keep the exact fifteen-to-one byte reduction");
  }

  accloud::render3d::MeshBuildOptions invalidPackedChunk = options;
  invalidPackedChunk.chunkLayerCount = 64;
  ok &= require(!mesher.build(source, {0, 3}, invalidPackedChunk).ok,
                "chunk sizes outside the packed relative Z range must be rejected");

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
