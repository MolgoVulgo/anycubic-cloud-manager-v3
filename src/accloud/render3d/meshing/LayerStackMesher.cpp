#include "render3d/meshing/LayerStackMesher.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace accloud::render3d {
namespace {

using photons::BinaryMask;
using photons::MeshChunk;
using photons::MeshVertex;

struct Point3 {
  float x;
  float y;
  float z;
};

enum class WallAxis {
  X,
  Y,
};

struct WallSpan {
  WallAxis axis = WallAxis::X;
  std::uint32_t fixed = 0;
  std::uint32_t first = 0;
  std::uint32_t last = 0;
  int orientation = 0;

  [[nodiscard]] auto key() const noexcept {
    return std::tuple{axis, fixed, first, last, orientation};
  }
  [[nodiscard]] bool operator<(const WallSpan& other) const noexcept {
    return key() < other.key();
  }
};

void addQuad(
    MeshChunk& chunk,
    const std::array<Point3, 4>& points,
    const Point3& normal) {
  const auto base = static_cast<std::uint32_t>(chunk.vertices.size());
  for (const auto& point : points) {
    chunk.vertices.push_back(MeshVertex{
        point.x,
        point.y,
        point.z,
        normal.x,
        normal.y,
        normal.z,
    });
    chunk.bounds.include(point.x, point.y, point.z);
  }
  chunk.indices.insert(
      chunk.indices.end(),
      {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
}

using PixelRun = std::pair<std::uint32_t, std::uint32_t>;

template <typename WordProvider>
std::vector<PixelRun> collectRuns(
    std::uint32_t width,
    std::size_t wordsPerRow,
    WordProvider wordProvider) {
  std::vector<PixelRun> runs;
  for (std::size_t wordIndex = 0; wordIndex < wordsPerRow; ++wordIndex) {
    std::uint64_t word = wordProvider(wordIndex);
    if (wordIndex + 1 == wordsPerRow && (width % 64u) != 0u) {
      word &= (std::uint64_t{1} << (width % 64u)) - 1u;
    }

    while (word != 0u) {
      const auto firstBit = static_cast<std::uint32_t>(std::countr_zero(word));
      const std::uint64_t shifted = word >> firstBit;
      const auto runLength = static_cast<std::uint32_t>(std::countr_one(shifted));
      const std::uint32_t first = static_cast<std::uint32_t>(wordIndex * 64u) + firstBit;
      const std::uint32_t last = std::min<std::uint32_t>(width, first + runLength);

      if (!runs.empty() && runs.back().second == first) {
        runs.back().second = last;
      } else {
        runs.emplace_back(first, last);
      }

      const std::uint32_t remainingBits = 64u - firstBit;
      if (runLength >= remainingBits) {
        word = 0u;
      } else {
        const std::uint64_t mask = ((std::uint64_t{1} << runLength) - 1u) << firstBit;
        word &= ~mask;
      }
    }
  }
  return runs;
}

std::vector<PixelRun> rowDifferenceRuns(
    const BinaryMask& material,
    const BinaryMask* neighbour,
    std::uint32_t y) {
  return collectRuns(
      material.width(),
      material.wordsPerRow(),
      [&](std::size_t wordIndex) {
        const std::uint64_t materialWord = material.rowWord(y, wordIndex);
        const std::uint64_t neighbourWord = neighbour == nullptr
                                                ? 0u
                                                : neighbour->rowWord(y, wordIndex);
        return materialWord & ~neighbourWord;
      });
}

void emitHorizontalSurface(
    MeshChunk& chunk,
    const BinaryMask& material,
    const BinaryMask* neighbour,
    float z,
    bool top,
    float pitchX,
    float pitchY) {
  using Run = PixelRun;
  std::map<Run, std::uint32_t> active;

  const auto emitRectangle = [&](const Run& run,
                                 std::uint32_t firstY,
                                 std::uint32_t endY) {
    const float x0 = run.first * pitchX;
    const float x1 = run.second * pitchX;
    const float yLow = (material.height() - endY) * pitchY;
    const float yHigh = (material.height() - firstY) * pitchY;
    if (top) {
      addQuad(chunk,
              {{{x0, yLow, z}, {x1, yLow, z}, {x1, yHigh, z}, {x0, yHigh, z}}},
              {0.0F, 0.0F, 1.0F});
    } else {
      addQuad(chunk,
              {{{x0, yLow, z}, {x0, yHigh, z}, {x1, yHigh, z}, {x1, yLow, z}}},
              {0.0F, 0.0F, -1.0F});
    }
  };

  for (std::uint32_t y = 0; y < material.height(); ++y) {
    const auto runs = rowDifferenceRuns(material, neighbour, y);
    std::set<Run> current(runs.begin(), runs.end());

    for (auto iterator = active.begin(); iterator != active.end();) {
      if (!current.contains(iterator->first)) {
        emitRectangle(iterator->first, iterator->second, y);
        iterator = active.erase(iterator);
      } else {
        ++iterator;
      }
    }
    for (const auto& run : runs) {
      active.try_emplace(run, y);
    }
  }

  for (const auto& [run, firstY] : active) {
    emitRectangle(run, firstY, material.height());
  }
}

std::set<WallSpan> collectXWalls(const BinaryMask& material) {
  struct ActiveWall {
    int orientation = 0;
    std::uint32_t firstY = 0;
  };

  std::set<WallSpan> walls;
  std::map<std::uint32_t, ActiveWall> active;
  for (std::uint32_t y = 0; y < material.height(); ++y) {
    std::map<std::uint32_t, int> rowWalls;
    const auto materialRuns = collectRuns(
        material.width(),
        material.wordsPerRow(),
        [&](std::size_t wordIndex) { return material.rowWord(y, wordIndex); });
    for (const auto& run : materialRuns) {
      rowWalls[run.first] = -1;
      rowWalls[run.second] = 1;
    }

    for (auto iterator = active.begin(); iterator != active.end();) {
      const auto found = rowWalls.find(iterator->first);
      if (found == rowWalls.end() || found->second != iterator->second.orientation) {
        walls.insert(WallSpan{
            WallAxis::X,
            iterator->first,
            iterator->second.firstY,
            y,
            iterator->second.orientation,
        });
        iterator = active.erase(iterator);
      } else {
        ++iterator;
      }
    }
    for (const auto& [x, orientation] : rowWalls) {
      active.try_emplace(x, ActiveWall{orientation, y});
    }
  }

  for (const auto& [x, wall] : active) {
    walls.insert(WallSpan{
        WallAxis::X,
        x,
        wall.firstY,
        material.height(),
        wall.orientation,
    });
  }
  return walls;
}

std::set<WallSpan> collectYWalls(const BinaryMask& material) {
  std::set<WallSpan> walls;
  for (std::uint32_t boundaryY = 0; boundaryY <= material.height(); ++boundaryY) {
    const bool hasAbove = boundaryY > 0;
    const bool hasBelow = boundaryY < material.height();

    const auto positiveRuns = collectRuns(
        material.width(),
        material.wordsPerRow(),
        [&](std::size_t wordIndex) {
          const std::uint64_t above = hasAbove
                                          ? material.rowWord(boundaryY - 1, wordIndex)
                                          : 0u;
          const std::uint64_t below = hasBelow
                                          ? material.rowWord(boundaryY, wordIndex)
                                          : 0u;
          return below & ~above;
        });
    for (const auto& run : positiveRuns) {
      walls.insert(WallSpan{WallAxis::Y, boundaryY, run.first, run.second, 1});
    }

    const auto negativeRuns = collectRuns(
        material.width(),
        material.wordsPerRow(),
        [&](std::size_t wordIndex) {
          const std::uint64_t above = hasAbove
                                          ? material.rowWord(boundaryY - 1, wordIndex)
                                          : 0u;
          const std::uint64_t below = hasBelow
                                          ? material.rowWord(boundaryY, wordIndex)
                                          : 0u;
          return above & ~below;
        });
    for (const auto& run : negativeRuns) {
      walls.insert(WallSpan{WallAxis::Y, boundaryY, run.first, run.second, -1});
    }
  }
  return walls;
}

void emitWall(
    MeshChunk& chunk,
    const WallSpan& wall,
    std::uint32_t rasterHeight,
    float z0,
    float z1,
    float pitchX,
    float pitchY) {
  if (wall.axis == WallAxis::X) {
    const float worldX = wall.fixed * pitchX;
    const float yLow = (rasterHeight - wall.last) * pitchY;
    const float yHigh = (rasterHeight - wall.first) * pitchY;
    if (wall.orientation > 0) {
      addQuad(chunk,
              {{{worldX, yLow, z0},
                {worldX, yHigh, z0},
                {worldX, yHigh, z1},
                {worldX, yLow, z1}}},
              {1.0F, 0.0F, 0.0F});
    } else {
      addQuad(chunk,
              {{{worldX, yLow, z0},
                {worldX, yLow, z1},
                {worldX, yHigh, z1},
                {worldX, yHigh, z0}}},
              {-1.0F, 0.0F, 0.0F});
    }
    return;
  }

  const float worldY = (rasterHeight - wall.fixed) * pitchY;
  const float x0 = wall.first * pitchX;
  const float x1 = wall.last * pitchX;
  if (wall.orientation > 0) {
    addQuad(chunk,
            {{{x0, worldY, z0},
              {x0, worldY, z1},
              {x1, worldY, z1},
              {x1, worldY, z0}}},
            {0.0F, 1.0F, 0.0F});
  } else {
    addQuad(chunk,
            {{{x0, worldY, z0},
              {x1, worldY, z0},
              {x1, worldY, z1},
              {x0, worldY, z1}}},
            {0.0F, -1.0F, 0.0F});
  }
}

using ActiveWalls = std::map<WallSpan, std::size_t>;

void updateActiveWalls(
    MeshChunk& chunk,
    ActiveWalls& active,
    const std::set<WallSpan>& current,
    std::size_t layer,
    std::uint32_t rasterHeight,
    float pitchX,
    float pitchY,
    float pitchZ) {
  for (auto iterator = active.begin(); iterator != active.end();) {
    if (!current.contains(iterator->first)) {
      emitWall(chunk,
               iterator->first,
               rasterHeight,
               static_cast<float>(iterator->second) * pitchZ,
               static_cast<float>(layer) * pitchZ,
               pitchX,
               pitchY);
      iterator = active.erase(iterator);
    } else {
      ++iterator;
    }
  }
  for (const auto& wall : current) {
    active.try_emplace(wall, layer);
  }
}

void flushActiveWalls(
    MeshChunk& chunk,
    ActiveWalls& active,
    std::size_t endLayerExclusive,
    std::uint32_t rasterHeight,
    float pitchX,
    float pitchY,
    float pitchZ) {
  for (const auto& [wall, firstLayer] : active) {
    emitWall(chunk,
             wall,
             rasterHeight,
             static_cast<float>(firstLayer) * pitchZ,
             static_cast<float>(endLayerExclusive) * pitchZ,
             pitchX,
             pitchY);
  }
  active.clear();
}


struct MeshTask {
  std::size_t firstSample = 0;
  std::size_t lastSample = 0;
  std::size_t firstLayer = 0;
  std::size_t endLayerExclusive = 0;

  [[nodiscard]] std::size_t sampleCount() const noexcept {
    return lastSample - firstSample + 1;
  }
};

struct TaskBuildResult {
  bool ok = false;
  bool cancelled = false;
  MeshChunk chunk;
  std::size_t decodedLayerCount = 0;
  std::string error;
};

std::vector<std::size_t> makeSampledLayers(
    photons::LayerRange range,
    std::size_t layerStride) {
  std::vector<std::size_t> sampledLayers;
  sampledLayers.reserve((range.count() + layerStride - 1) / layerStride + 1);
  for (std::size_t layer = range.first;;) {
    sampledLayers.push_back(layer);
    if (range.last - layer < layerStride) {
      break;
    }
    layer += layerStride;
  }
  if (sampledLayers.back() != range.last) {
    sampledLayers.push_back(range.last);
  }
  return sampledLayers;
}

std::vector<MeshTask> makeTasks(
    const std::vector<std::size_t>& sampledLayers,
    photons::LayerRange range,
    std::size_t chunkLayerCount) {
  std::vector<MeshTask> tasks;
  std::size_t firstSample = 0;
  std::size_t firstLayer = range.first;
  for (std::size_t sampleIndex = 0; sampleIndex < sampledLayers.size(); ++sampleIndex) {
    const bool lastSample = sampleIndex + 1 == sampledLayers.size();
    const std::size_t segmentEndExclusive = lastSample
                                                ? range.last + 1
                                                : sampledLayers[sampleIndex + 1];
    if (segmentEndExclusive - firstLayer < chunkLayerCount && !lastSample) {
      continue;
    }
    tasks.push_back(MeshTask{
        firstSample,
        sampleIndex,
        firstLayer,
        segmentEndExclusive,
    });
    firstSample = sampleIndex + 1;
    firstLayer = segmentEndExclusive;
  }
  return tasks;
}

TaskBuildResult buildTask(
    photons::LayerMaskSource& source,
    std::mutex* sourceLoadMutex,
    photons::LayerRange range,
    const std::vector<std::size_t>& sampledLayers,
    const MeshTask& task,
    const MeshBuildOptions& options,
    const std::function<bool()>& isCancelled,
    const std::function<void()>& sampleCompleted) {
  TaskBuildResult result;
  result.chunk.layers = {task.firstLayer, task.endLayerExclusive - 1};

  const float pitchX = static_cast<float>(options.pitchXMm);
  const float pitchY = static_cast<float>(options.pitchYMm);
  const float pitchZ = static_cast<float>(options.pitchZMm);
  std::string loadError;

  const auto load = [&](std::size_t layer) -> std::optional<BinaryMask> {
    if (isCancelled()) {
      return std::nullopt;
    }
    std::optional<BinaryMask> mask;
    if (sourceLoadMutex != nullptr) {
      std::scoped_lock lock(*sourceLoadMutex);
      mask = source.loadMask(layer, loadError);
    } else {
      mask = source.loadMask(layer, loadError);
    }
    if (mask && (mask->width() != source.width() || mask->height() != source.height())) {
      loadError = "layer mask dimensions differ from the source metadata";
      return std::nullopt;
    }
    if (mask) {
      ++result.decodedLayerCount;
    }
    return mask;
  };

  std::optional<BinaryMask> previousSample;
  if (task.firstSample > 0) {
    previousSample = load(sampledLayers[task.firstSample - 1]);
  } else if (range.first > 0 && options.cutSurfaceMode == CutSurfaceMode::Open) {
    previousSample = load(range.first - 1);
  }
  if ((task.firstSample > 0
       || (range.first > 0 && options.cutSurfaceMode == CutSurfaceMode::Open))
      && !previousSample) {
    result.cancelled = isCancelled();
    result.error = result.cancelled ? "mesh build cancelled" : loadError;
    return result;
  }

  std::optional<BinaryMask> current = load(sampledLayers[task.firstSample]);
  if (!current) {
    result.cancelled = isCancelled();
    result.error = result.cancelled ? "mesh build cancelled" : loadError;
    return result;
  }

  std::optional<BinaryMask> nextSample;
  if (task.firstSample + 1 < sampledLayers.size()) {
    nextSample = load(sampledLayers[task.firstSample + 1]);
  } else if (options.cutSurfaceMode == CutSurfaceMode::Open
             && range.last + 1 < source.layerCount()) {
    nextSample = load(range.last + 1);
  }
  if ((task.firstSample + 1 < sampledLayers.size()
       || (options.cutSurfaceMode == CutSurfaceMode::Open
           && range.last + 1 < source.layerCount()))
      && !nextSample) {
    result.cancelled = isCancelled();
    result.error = result.cancelled ? "mesh build cancelled" : loadError;
    return result;
  }

  ActiveWalls activeWalls;
  for (std::size_t sampleIndex = task.firstSample;
       sampleIndex <= task.lastSample;
       ++sampleIndex) {
    if (isCancelled()) {
      result.cancelled = true;
      result.error = "mesh build cancelled";
      return result;
    }

    const std::size_t layer = sampledLayers[sampleIndex];
    const bool firstGlobalSample = sampleIndex == 0;
    const bool lastGlobalSample = sampleIndex + 1 == sampledLayers.size();
    const std::size_t segmentEndExclusive = lastGlobalSample
                                                ? range.last + 1
                                                : sampledLayers[sampleIndex + 1];

    const BinaryMask* previousMask = previousSample ? &*previousSample : nullptr;
    const BinaryMask* nextMask = nextSample ? &*nextSample : nullptr;
    if (firstGlobalSample && options.cutSurfaceMode == CutSurfaceMode::Closed) {
      previousMask = nullptr;
    }
    if (lastGlobalSample && options.cutSurfaceMode == CutSurfaceMode::Closed) {
      nextMask = nullptr;
    }

    const float z0 = static_cast<float>(layer) * pitchZ;
    const float z1 = static_cast<float>(segmentEndExclusive) * pitchZ;
    emitHorizontalSurface(result.chunk, *current, previousMask, z0, false, pitchX, pitchY);
    emitHorizontalSurface(result.chunk, *current, nextMask, z1, true, pitchX, pitchY);

    auto walls = collectXWalls(*current);
    const auto yWalls = collectYWalls(*current);
    walls.insert(yWalls.begin(), yWalls.end());
    updateActiveWalls(
        result.chunk,
        activeWalls,
        walls,
        layer,
        source.height(),
        pitchX,
        pitchY,
        pitchZ);

    sampleCompleted();
    if (sampleIndex == task.lastSample) {
      break;
    }

    previousSample = std::move(current);
    current = std::move(nextSample);
    const std::size_t followingSample = sampleIndex + 2;
    if (followingSample < sampledLayers.size()) {
      nextSample = load(sampledLayers[followingSample]);
      if (!nextSample) {
        result.cancelled = isCancelled();
        result.error = result.cancelled ? "mesh build cancelled" : loadError;
        return result;
      }
    } else if (options.cutSurfaceMode == CutSurfaceMode::Open
               && range.last + 1 < source.layerCount()) {
      nextSample = load(range.last + 1);
      if (!nextSample) {
        result.cancelled = isCancelled();
        result.error = result.cancelled ? "mesh build cancelled" : loadError;
        return result;
      }
    } else {
      nextSample.reset();
    }
  }

  flushActiveWalls(
      result.chunk,
      activeWalls,
      task.endLayerExclusive,
      source.height(),
      pitchX,
      pitchY,
      pitchZ);
  result.ok = true;
  return result;
}

} // namespace

MeshBuildResult LayerStackMesher::build(
    photons::LayerMaskSource& source,
    photons::LayerRange range,
    const MeshBuildOptions& options,
    const MeshBuildCallbacks& callbacks) const {
  MeshBuildResult result;
  if (!range.validFor(source.layerCount())) {
    result.error = "mesh layer range is outside the source";
    return result;
  }
  if (source.width() == 0 || source.height() == 0) {
    result.error = "mesh source dimensions must be non-zero";
    return result;
  }
  if (!(options.pitchXMm > 0.0) || !(options.pitchYMm > 0.0)
      || !(options.pitchZMm > 0.0) || options.chunkLayerCount == 0
      || options.layerStride == 0) {
    result.error = "mesh pitch, chunk size and layer stride must be positive";
    return result;
  }
  if (options.workerCount < kMinimumMeshWorkerCount
      || options.workerCount > kMaximumMeshWorkerCount) {
    result.error = "mesh worker count must be between 1 and 16";
    return result;
  }

  const auto sampledLayers = makeSampledLayers(range, options.layerStride);
  const auto tasks = makeTasks(sampledLayers, range, options.chunkLayerCount);
  result.effectiveWorkerCount = std::min(options.workerCount, tasks.size());
  result.workerStats.resize(result.effectiveWorkerCount);
  for (std::size_t workerIndex = 0; workerIndex < result.workerStats.size(); ++workerIndex) {
    result.workerStats[workerIndex].workerIndex = workerIndex;
  }

  std::mutex sourceLoadMutex;
  std::mutex cancelCallbackMutex;
  std::mutex progressCallbackMutex;
  std::mutex consumeCallbackMutex;
  std::mutex resultMutex;
  std::mutex errorMutex;
  std::atomic<std::size_t> nextTask{0};
  std::atomic<std::size_t> completedSamples{0};
  std::atomic<bool> cancellationRequested{false};
  std::atomic<bool> failed{false};
  std::string firstError;

  const auto checkCancelled = [&]() {
    if (cancellationRequested.load(std::memory_order_relaxed)) {
      return true;
    }
    if (!callbacks.isCancelled) {
      return false;
    }
    bool cancelled = false;
    {
      std::scoped_lock lock(cancelCallbackMutex);
      cancelled = callbacks.isCancelled();
    }
    if (cancelled) {
      cancellationRequested.store(true, std::memory_order_relaxed);
    }
    return cancelled;
  };

  const auto reportSampleCompleted = [&]() {
    if (!callbacks.progress) {
      completedSamples.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    // Keep progress callbacks monotonic even when workers complete samples out
    // of order. The counter increment deliberately happens under the same lock
    // as the callback invocation.
    std::scoped_lock lock(progressCallbackMutex);
    const std::size_t completed = completedSamples.fetch_add(1, std::memory_order_relaxed) + 1;
    callbacks.progress(completed, sampledLayers.size());
  };

  const auto recordFailure = [&](std::string error, bool cancelled) {
    if (cancelled) {
      cancellationRequested.store(true, std::memory_order_relaxed);
      return;
    }
    failed.store(true, std::memory_order_relaxed);
    cancellationRequested.store(true, std::memory_order_relaxed);
    std::scoped_lock lock(errorMutex);
    if (firstError.empty()) {
      firstError = std::move(error);
    }
  };

  const auto consumeChunk = [&](MeshChunk&& chunk) {
    if (callbacks.consumeChunk) {
      std::scoped_lock lock(consumeCallbackMutex);
      if (!callbacks.consumeChunk(std::move(chunk))) {
        cancellationRequested.store(true, std::memory_order_relaxed);
        return false;
      }
      return true;
    }
    std::scoped_lock lock(resultMutex);
    result.chunks.push_back(std::move(chunk));
    return true;
  };

  const auto workerMain = [&](std::size_t workerIndex) {
    const auto started = std::chrono::steady_clock::now();
    auto& stats = result.workerStats[workerIndex];
    while (!checkCancelled()) {
      const std::size_t taskIndex = nextTask.fetch_add(1, std::memory_order_relaxed);
      if (taskIndex >= tasks.size()) {
        break;
      }
      auto taskResult = buildTask(
          source,
          source.supportsConcurrentMaskLoads() ? nullptr : &sourceLoadMutex,
          range,
          sampledLayers,
          tasks[taskIndex],
          options,
          checkCancelled,
          reportSampleCompleted);
      ++stats.taskCount;
      stats.decodedLayerCount += taskResult.decodedLayerCount;
      if (!taskResult.ok) {
        recordFailure(taskResult.error, taskResult.cancelled);
        break;
      }
      if (!consumeChunk(std::move(taskResult.chunk))) {
        break;
      }
      ++stats.chunkCount;
    }
    stats.durationMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count());
  };

  std::vector<std::thread> workers;
  workers.reserve(result.effectiveWorkerCount);
  for (std::size_t workerIndex = 0; workerIndex < result.effectiveWorkerCount; ++workerIndex) {
    workers.emplace_back(workerMain, workerIndex);
  }
  for (auto& worker : workers) {
    worker.join();
  }

  for (const auto& stats : result.workerStats) {
    result.decodedLayerCount += stats.decodedLayerCount;
  }
  if (!callbacks.consumeChunk) {
    std::sort(
        result.chunks.begin(),
        result.chunks.end(),
        [](const MeshChunk& left, const MeshChunk& right) {
          return left.layers.first < right.layers.first;
        });
  }

  if (failed.load(std::memory_order_relaxed)) {
    result.error = firstError.empty() ? "mesh worker failed" : firstError;
    return result;
  }
  if (cancellationRequested.load(std::memory_order_relaxed)) {
    result.cancelled = true;
    result.error = "mesh build cancelled";
    return result;
  }
  result.ok = true;
  return result;
}

} // namespace accloud::render3d
