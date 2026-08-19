#include "render3d/analysis/SupportAnalyzer.h"

#include "domain/photons/BinaryMask.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <numbers>
#include <optional>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if (defined(__x86_64__) || defined(__i386__)) \
    && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define ACCLOUD_SUPPORT_HAS_X86_AVX2_DISPATCH 1
#else
#define ACCLOUD_SUPPORT_HAS_X86_AVX2_DISPATCH 0
#endif

namespace accloud::render3d {
namespace {

using photons::BinaryMask;
using PixelRun = std::pair<std::uint32_t, std::uint32_t>;

// P5 keeps sparse runs as the canonical representation, but promotes
// sufficiently fragmented rows to a compact word bitset. Dense bitsets are
// deliberately row-local and bounded relative to the number of sparse
// intervals so a large mostly-empty resin layer cannot explode memory.
constexpr std::size_t kBitsetMinimumIntervals = 128u;
constexpr std::size_t kBitsetMaximumWordsPerInterval = 1u;

struct HybridBitRow {
  std::uint32_t firstWord = 0u;
  std::vector<std::uint64_t> words;

  [[nodiscard]] bool empty() const noexcept { return words.empty(); }
  void clear() { words.clear(); }
};

std::uint32_t lowBitsMask32(std::uint32_t bitCount) {
  if (bitCount == 0u) {
    return 0u;
  }
  if (bitCount >= 32u) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  return (std::uint32_t{1} << bitCount) - 1u;
}

void setDenseRange32(
    std::span<std::uint32_t> words,
    std::uint32_t wordsPerRow,
    std::uint32_t row,
    std::uint32_t firstX,
    std::uint32_t lastX) {
  if (firstX >= lastX || wordsPerRow == 0u) {
    return;
  }
  const auto firstWord = firstX / 32u;
  const auto lastWord = (lastX - 1u) / 32u;
  const auto rowBase = static_cast<std::size_t>(row) * wordsPerRow;
  for (auto word = firstWord; word <= lastWord; ++word) {
    if (word >= wordsPerRow || rowBase + word >= words.size()) {
      break;
    }
    const auto firstBit = word == firstWord ? firstX % 32u : 0u;
    const auto lastBit = word == lastWord ? ((lastX - 1u) % 32u) + 1u : 32u;
    const auto lower = firstBit == 0u
        ? std::numeric_limits<std::uint32_t>::max()
        : std::numeric_limits<std::uint32_t>::max() << firstBit;
    words[rowBase + word] |= lower & lowBitsMask32(lastBit);
  }
}

std::uint64_t lowBitsMask(std::uint32_t bitCount) {
  if (bitCount == 0u) {
    return 0u;
  }
  if (bitCount >= 64u) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return (std::uint64_t{1} << bitCount) - 1u;
}

std::uint64_t rangeBitsMask(std::uint32_t firstBit, std::uint32_t lastBit) {
  if (firstBit >= lastBit || firstBit >= 64u) {
    return 0u;
  }
  const auto lower = firstBit == 0u
      ? std::numeric_limits<std::uint64_t>::max()
      : std::numeric_limits<std::uint64_t>::max() << firstBit;
  return lower & lowBitsMask(std::min<std::uint32_t>(64u, lastBit));
}

void setBitRowRange(
    HybridBitRow& row,
    std::uint32_t firstX,
    std::uint32_t lastX) {
  if (firstX >= lastX || row.words.empty()) {
    return;
  }
  const auto firstWord = firstX / 64u;
  const auto lastWord = (lastX - 1u) / 64u;
  for (auto word = firstWord; word <= lastWord; ++word) {
    if (word < row.firstWord
        || static_cast<std::size_t>(word - row.firstWord) >= row.words.size()) {
      continue;
    }
    const auto firstBit = word == firstWord ? firstX % 64u : 0u;
    const auto lastBit = word == lastWord ? ((lastX - 1u) % 64u) + 1u : 64u;
    row.words[word - row.firstWord] |= rangeBitsMask(firstBit, lastBit);
  }
}

bool buildHybridBitRow(
    const std::vector<PixelRun>& intervals,
    HybridBitRow& output) {
  output.clear();
  if (intervals.size() < kBitsetMinimumIntervals) {
    return false;
  }
  const auto firstWord = intervals.front().first / 64u;
  const auto lastWordExclusive = (intervals.back().second + 63u) / 64u;
  if (lastWordExclusive <= firstWord) {
    return false;
  }
  const auto wordCount = static_cast<std::size_t>(lastWordExclusive - firstWord);
  if (wordCount > intervals.size() * kBitsetMaximumWordsPerInterval
      || wordCount * 4u > intervals.size()) {
    return false;
  }
  output.firstWord = firstWord;
  output.words.assign(wordCount, 0u);
  for (const auto& interval : intervals) {
    setBitRowRange(output, interval.first, interval.second);
  }
  return true;
}

std::size_t countHybridBitRowRange(
    const HybridBitRow& row,
    std::int64_t firstX,
    std::int64_t lastX) {
  if (row.empty() || firstX >= lastX || lastX <= 0) {
    return 0u;
  }
  const auto rowFirst = static_cast<std::int64_t>(row.firstWord) * 64;
  const auto rowLast = rowFirst + static_cast<std::int64_t>(row.words.size()) * 64;
  firstX = std::max(firstX, rowFirst);
  lastX = std::min(lastX, rowLast);
  if (firstX >= lastX) {
    return 0u;
  }

  const auto firstWord = static_cast<std::uint32_t>(firstX / 64);
  const auto lastWord = static_cast<std::uint32_t>((lastX - 1) / 64);
  std::size_t result = 0u;
  for (auto word = firstWord; word <= lastWord; ++word) {
    auto bits = row.words[word - row.firstWord];
    const auto firstBit = word == firstWord
        ? static_cast<std::uint32_t>(firstX % 64)
        : 0u;
    const auto lastBit = word == lastWord
        ? static_cast<std::uint32_t>(((lastX - 1) % 64) + 1)
        : 64u;
    bits &= rangeBitsMask(firstBit, lastBit);
    result += static_cast<std::size_t>(std::popcount(bits));
  }
  return result;
}

template <typename Emit>
void forEachHybridBitRun(
    const HybridBitRow& row,
    std::int64_t firstX,
    std::int64_t lastX,
    Emit emit) {
  if (row.empty() || firstX >= lastX || lastX <= 0) {
    return;
  }
  const auto rowFirst = static_cast<std::int64_t>(row.firstWord) * 64;
  const auto rowLast = rowFirst + static_cast<std::int64_t>(row.words.size()) * 64;
  firstX = std::max(firstX, rowFirst);
  lastX = std::min(lastX, rowLast);
  if (firstX >= lastX) {
    return;
  }

  const auto firstWord = static_cast<std::uint32_t>(firstX / 64);
  const auto lastWord = static_cast<std::uint32_t>((lastX - 1) / 64);
  std::optional<std::uint32_t> pendingFirst;
  std::uint32_t pendingLast = 0u;
  for (auto word = firstWord; word <= lastWord; ++word) {
    auto bits = row.words[word - row.firstWord];
    const auto firstBit = word == firstWord
        ? static_cast<std::uint32_t>(firstX % 64)
        : 0u;
    const auto lastBit = word == lastWord
        ? static_cast<std::uint32_t>(((lastX - 1) % 64) + 1)
        : 64u;
    bits &= rangeBitsMask(firstBit, lastBit);
    while (bits != 0u) {
      const auto startBit = static_cast<std::uint32_t>(std::countr_zero(bits));
      const auto shifted = bits >> startBit;
      const auto length = static_cast<std::uint32_t>(std::countr_one(shifted));
      const auto absoluteFirst = word * 64u + startBit;
      const auto absoluteLast = absoluteFirst + length;
      if (pendingFirst && pendingLast == absoluteFirst) {
        pendingLast = absoluteLast;
      } else {
        if (pendingFirst) {
          emit(*pendingFirst, pendingLast);
        }
        pendingFirst = absoluteFirst;
        pendingLast = absoluteLast;
      }
      const auto remaining = 64u - startBit;
      if (length >= remaining) {
        bits = 0u;
      } else {
        bits &= ~(((std::uint64_t{1} << length) - 1u) << startBit);
      }
    }
  }
  if (pendingFirst) {
    emit(*pendingFirst, pendingLast);
  }
}

void bitwiseAndWordsScalar(
    const std::uint64_t* left,
    const std::uint64_t* right,
    std::uint64_t* output,
    std::size_t count) {
  for (std::size_t index = 0u; index < count; ++index) {
    output[index] = left[index] & right[index];
  }
}

#if ACCLOUD_SUPPORT_HAS_X86_AVX2_DISPATCH
__attribute__((target("avx2")))
void bitwiseAndWordsAvx2(
    const std::uint64_t* left,
    const std::uint64_t* right,
    std::uint64_t* output,
    std::size_t count) {
  std::size_t index = 0u;
  for (; index + 4u <= count; index += 4u) {
    const auto a = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(left + index));
    const auto b = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(right + index));
    _mm256_storeu_si256(
        reinterpret_cast<__m256i*>(output + index), _mm256_and_si256(a, b));
  }
  bitwiseAndWordsScalar(left + index, right + index, output + index, count - index);
}

bool cpuSupportsAvx2() {
  static const bool supported = [] {
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
  }();
  return supported;
}
#endif

void bitwiseAndWords(
    const std::uint64_t* left,
    const std::uint64_t* right,
    std::uint64_t* output,
    std::size_t count) {
#if ACCLOUD_SUPPORT_HAS_X86_AVX2_DISPATCH
  if (cpuSupportsAvx2()) {
    bitwiseAndWordsAvx2(left, right, output, count);
    return;
  }
#endif
  bitwiseAndWordsScalar(left, right, output, count);
}

class DisjointSet {
public:
  void reserve(std::size_t capacity) {
    parent_.reserve(capacity);
    rank_.reserve(capacity);
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return parent_.size();
  }

  std::size_t add() {
    const std::size_t index = parent_.size();
    parent_.push_back(index);
    rank_.push_back(0u);
    return index;
  }

  std::size_t find(std::size_t value) {
    if (parent_[value] != value) {
      parent_[value] = find(parent_[value]);
    }
    return parent_[value];
  }

  void unite(std::size_t left, std::size_t right) {
    left = find(left);
    right = find(right);
    if (left == right) {
      return;
    }
    if (rank_[left] < rank_[right]) {
      std::swap(left, right);
    }
    parent_[right] = left;
    if (rank_[left] == rank_[right]) {
      ++rank_[left];
    }
  }

private:
  std::vector<std::size_t> parent_;
  std::vector<std::uint8_t> rank_;
};

void appendRunsFromWord(
    std::uint32_t width,
    std::size_t wordIndex,
    std::uint64_t word,
    std::vector<PixelRun>& runs) {
  if (wordIndex * 64u >= width) {
    return;
  }
  if ((wordIndex + 1u) * 64u > width && (width % 64u) != 0u) {
    word &= lowBitsMask(width % 64u);
  }
  while (word != 0u) {
    const auto firstBit = static_cast<std::uint32_t>(std::countr_zero(word));
    const auto shifted = word >> firstBit;
    const auto runLength = static_cast<std::uint32_t>(std::countr_one(shifted));
    const auto first = static_cast<std::uint32_t>(wordIndex * 64u) + firstBit;
    const auto last = std::min<std::uint32_t>(width, first + runLength);
    if (!runs.empty() && runs.back().second == first) {
      runs.back().second = last;
    } else {
      runs.emplace_back(first, last);
    }
    const auto remaining = 64u - firstBit;
    if (runLength >= remaining) {
      word = 0u;
    } else {
      word &= ~(((std::uint64_t{1} << runLength) - 1u) << firstBit);
    }
  }
}

void collectRunsScalar(
    std::uint32_t width,
    const std::uint64_t* words,
    std::size_t wordsPerRow,
    std::vector<PixelRun>& runs) {
  runs.clear();
  for (std::size_t wordIndex = 0u; wordIndex < wordsPerRow; ++wordIndex) {
    appendRunsFromWord(width, wordIndex, words[wordIndex], runs);
  }
}

#if ACCLOUD_SUPPORT_HAS_X86_AVX2_DISPATCH
__attribute__((target("avx2")))
void collectRunsAvx2(
    std::uint32_t width,
    const std::uint64_t* words,
    std::size_t wordsPerRow,
    std::vector<PixelRun>& runs) {
  runs.clear();
  const auto partialLastWord = (width % 64u) != 0u;
  const auto vectorWordCount = partialLastWord && wordsPerRow != 0u
      ? wordsPerRow - 1u
      : wordsPerRow;
  std::size_t wordIndex = 0u;
  for (; wordIndex + 4u <= vectorWordCount; wordIndex += 4u) {
    const auto block = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(words + wordIndex));
    if (_mm256_testz_si256(block, block) != 0) {
      continue;
    }
    appendRunsFromWord(width, wordIndex + 0u, words[wordIndex + 0u], runs);
    appendRunsFromWord(width, wordIndex + 1u, words[wordIndex + 1u], runs);
    appendRunsFromWord(width, wordIndex + 2u, words[wordIndex + 2u], runs);
    appendRunsFromWord(width, wordIndex + 3u, words[wordIndex + 3u], runs);
  }
  for (; wordIndex < wordsPerRow; ++wordIndex) {
    appendRunsFromWord(width, wordIndex, words[wordIndex], runs);
  }
}
#endif

void collectRuns(
    std::uint32_t width,
    const std::uint64_t* words,
    std::size_t wordsPerRow,
    std::vector<PixelRun>& runs) {
#if ACCLOUD_SUPPORT_HAS_X86_AVX2_DISPATCH
  if (cpuSupportsAvx2()) {
    collectRunsAvx2(width, words, wordsPerRow, runs);
    return;
  }
#endif
  collectRunsScalar(width, words, wordsPerRow, runs);
}

struct RawRun {
  std::uint32_t y = 0;
  std::uint32_t first = 0;
  std::uint32_t last = 0;
  std::size_t label = 0;
};

struct Component {
  std::size_t localId = 0;
  std::size_t area = 0;
  std::uint32_t minX = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxX = 0;
  std::uint32_t minY = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxY = 0;
  double centerX = 0.0;
  double centerY = 0.0;
  std::vector<SemanticRun> runs;
};

struct LayerDescription {
  std::size_t layer = 0;
  std::size_t totalArea = 0;
  std::size_t largestArea = 0;
  std::vector<Component> components;
};

struct GridQueryScratch {
  void begin(std::size_t stampCount) {
    if (stamps.size() < stampCount) {
      stamps.resize(stampCount, 0u);
    }
    result.clear();
    if (++generation == 0u) {
      std::fill(stamps.begin(), stamps.end(), 0u);
      generation = 1u;
    }
  }

  std::vector<std::uint32_t> stamps;
  std::uint32_t generation = 0u;
  std::vector<std::size_t> result;
};

class ComponentGridIndex {
public:
  ComponentGridIndex(
      const LayerDescription* layer,
      const std::vector<bool>& selected,
      double marginPixels)
      : componentCount_(layer == nullptr ? 0u : layer->components.size()) {
    build(layer, &selected, marginPixels);
  }

  // P6.5 graph construction indexes every component of the previous native
  // layer. Semantic filtering is deliberately deferred to reconciliation so
  // adjacent-layer geometry can be computed independently in parallel lots.
  ComponentGridIndex(
      const LayerDescription* layer,
      double marginPixels)
      : componentCount_(layer == nullptr ? 0u : layer->components.size()) {
    build(layer, nullptr, marginPixels);
  }

  [[nodiscard]] const std::vector<std::size_t>& query(
      const Component& component,
      GridQueryScratch& scratch) const {
    scratch.begin(componentCount_);
    if (componentCount_ == 0u) {
      return scratch.result;
    }
    forEachCell(
        component.minX, component.minY, component.maxX, component.maxY,
        [&](std::uint64_t key) {
          const auto iterator = cells_.find(key);
          if (iterator == cells_.end()) {
            return;
          }
          for (const auto index : iterator->second) {
            if (scratch.stamps[index] == scratch.generation) {
              continue;
            }
            scratch.stamps[index] = scratch.generation;
            scratch.result.push_back(index);
          }
        });
    return scratch.result;
  }

private:
  void build(
      const LayerDescription* layer,
      const std::vector<bool>* selected,
      double marginPixels) {
    if (layer == nullptr) {
      return;
    }
    const auto margin = static_cast<std::uint32_t>(
        std::max(0.0, std::ceil(marginPixels)));
    for (std::size_t index = 0; index < layer->components.size(); ++index) {
      if (selected != nullptr
          && (index >= selected->size() || !(*selected)[index])) {
        continue;
      }
      const auto& component = layer->components[index];
      const auto minX = component.minX > margin ? component.minX - margin : 0u;
      const auto minY = component.minY > margin ? component.minY - margin : 0u;
      const auto maxX = component.maxX + margin;
      const auto maxY = component.maxY + margin;
      add(index, minX, minY, maxX, maxY);
    }
  }

  static constexpr std::uint32_t kCellSize = 128u;

  static std::uint64_t cellKey(std::uint32_t x, std::uint32_t y) {
    return (static_cast<std::uint64_t>(x) << 32u) | y;
  }

  template <typename Visitor>
  static void forEachCell(
      std::uint32_t minX,
      std::uint32_t minY,
      std::uint32_t maxX,
      std::uint32_t maxY,
      Visitor visitor) {
    if (maxX <= minX || maxY <= minY) {
      return;
    }
    const auto firstCellX = minX / kCellSize;
    const auto firstCellY = minY / kCellSize;
    const auto lastCellX = (maxX - 1u) / kCellSize;
    const auto lastCellY = (maxY - 1u) / kCellSize;
    for (std::uint32_t cellY = firstCellY; cellY <= lastCellY; ++cellY) {
      for (std::uint32_t cellX = firstCellX; cellX <= lastCellX; ++cellX) {
        visitor(cellKey(cellX, cellY));
      }
    }
  }

  void add(
      std::size_t component,
      std::uint32_t minX,
      std::uint32_t minY,
      std::uint32_t maxX,
      std::uint32_t maxY) {
    forEachCell(minX, minY, maxX, maxY, [&](std::uint64_t key) {
      cells_[key].push_back(component);
    });
  }

  std::size_t componentCount_ = 0;
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> cells_;
  std::vector<std::uint32_t> stamps_;
  std::uint32_t generation_ = 0u;
};

LayerDescription describeLayer(const BinaryMask& mask, std::size_t layer) {
  LayerDescription description;
  description.layer = layer;

  std::vector<RawRun> runs;
  std::vector<PixelRun> rowRuns;
  std::vector<std::size_t> previousRow;
  std::vector<std::size_t> currentRow;
  DisjointSet sets;
  runs.reserve(mask.height());
  sets.reserve(mask.height());
  for (std::uint32_t y = 0; y < mask.height(); ++y) {
    const auto* rowWords = mask.words().data()
        + static_cast<std::size_t>(y) * mask.wordsPerRow();
    collectRuns(mask.width(), rowWords, mask.wordsPerRow(), rowRuns);
    currentRow.clear();
    if (currentRow.capacity() < rowRuns.size()) {
      currentRow.reserve(rowRuns.size());
    }
    std::size_t previousCursor = 0;
    for (const auto& [first, last] : rowRuns) {
      const auto label = sets.add();
      const auto runIndex = runs.size();
      runs.push_back(RawRun{y, first, last, label});
      currentRow.push_back(runIndex);

      while (previousCursor < previousRow.size()
             && runs[previousRow[previousCursor]].last <= first) {
        ++previousCursor;
      }
      for (std::size_t cursor = previousCursor; cursor < previousRow.size(); ++cursor) {
        const auto& upper = runs[previousRow[cursor]];
        if (upper.first >= last) {
          break;
        }
        sets.unite(label, upper.label);
      }
    }
    previousRow.swap(currentRow);
  }

  const auto invalidComponent = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> componentIndex(sets.size(), invalidComponent);
  std::vector<std::size_t> componentRunCounts;
  for (auto& run : runs) {
    const auto root = sets.find(run.label);
    auto index = componentIndex[root];
    if (index == invalidComponent) {
      index = description.components.size();
      componentIndex[root] = index;
      description.components.push_back(Component{});
      description.components.back().localId = index;
      componentRunCounts.push_back(0u);
    }
    run.label = index;
    ++componentRunCounts[index];
    auto& component = description.components[index];
    const auto length = static_cast<std::size_t>(run.last - run.first);
    component.area += length;
    component.minX = std::min(component.minX, run.first);
    component.maxX = std::max(component.maxX, run.last);
    component.minY = std::min(component.minY, run.y);
    component.maxY = std::max(component.maxY, run.y + 1u);
    component.centerX += (static_cast<double>(run.first + run.last - 1u) * 0.5)
                         * static_cast<double>(length);
    component.centerY += static_cast<double>(run.y) * static_cast<double>(length);
  }

  for (std::size_t index = 0u; index < description.components.size(); ++index) {
    description.components[index].runs.reserve(componentRunCounts[index]);
  }
  for (const auto& run : runs) {
    description.components[run.label].runs.push_back(SemanticRun{
        run.y, run.first, run.last, MaterialSemantic::Model});
  }

  for (auto& component : description.components) {
    if (component.area != 0) {
      component.centerX /= static_cast<double>(component.area);
      component.centerY /= static_cast<double>(component.area);
    }
    description.totalArea += component.area;
    description.largestArea = std::max(description.largestArea, component.area);
  }
  return description;
}

struct PreparedLayer {
  std::size_t layer = 0u;
  std::optional<BinaryMask> mask;
  LayerDescription description;
  std::string error;
  bool ok = false;
};

struct SupportPerformanceCounters {
  std::atomic<std::uint64_t> preparationLoadMicroseconds{0u};
  std::atomic<std::uint64_t> preparationDescribeMicroseconds{0u};
  std::atomic<std::uint64_t> forwardSemanticMicroseconds{0u};
  std::atomic<std::uint64_t> reverseSemanticMicroseconds{0u};
  std::atomic<std::uint64_t> forwardClassificationMicroseconds{0u};
  std::atomic<std::uint64_t> forwardCommitMicroseconds{0u};
  std::atomic<std::uint64_t> forwardLineageMicroseconds{0u};
  std::atomic<std::uint64_t> forwardLineageCommitMicroseconds{0u};
  std::atomic<std::uint64_t> reversePreparationMicroseconds{0u};
  std::atomic<std::uint64_t> reverseCommitMicroseconds{0u};
  std::atomic<std::uint64_t> semanticEvidenceMicroseconds{0u};
  std::atomic<std::size_t> semanticEvidenceEdgeCount{0u};
  std::size_t semanticEvidenceLotCount = 0u;
  std::size_t semanticEvidenceLayerPairCount = 0u;
  std::atomic<std::size_t> preparedLayerCount{0u};
  std::atomic<std::size_t> maximumPreparationInflight{0u};
  std::size_t preparationWindowCapacity = 0u;

  [[nodiscard]] SupportAnalysisPerformanceTelemetry snapshot() const noexcept {
    SupportAnalysisPerformanceTelemetry result;
    result.preparationWindowCapacity = preparationWindowCapacity;
    result.preparedLayerCount = preparedLayerCount.load(std::memory_order_relaxed);
    result.maximumPreparationInflight =
        maximumPreparationInflight.load(std::memory_order_relaxed);
    result.preparationLoadMicroseconds =
        preparationLoadMicroseconds.load(std::memory_order_relaxed);
    result.preparationDescribeMicroseconds =
        preparationDescribeMicroseconds.load(std::memory_order_relaxed);
    result.forwardSemanticMicroseconds =
        forwardSemanticMicroseconds.load(std::memory_order_relaxed);
    result.reverseSemanticMicroseconds =
        reverseSemanticMicroseconds.load(std::memory_order_relaxed);
    result.forwardClassificationMicroseconds =
        forwardClassificationMicroseconds.load(std::memory_order_relaxed);
    result.forwardCommitMicroseconds =
        forwardCommitMicroseconds.load(std::memory_order_relaxed);
    result.forwardLineageMicroseconds =
        forwardLineageMicroseconds.load(std::memory_order_relaxed);
    result.forwardLineageCommitMicroseconds =
        forwardLineageCommitMicroseconds.load(std::memory_order_relaxed);
    result.reversePreparationMicroseconds =
        reversePreparationMicroseconds.load(std::memory_order_relaxed);
    result.reverseCommitMicroseconds =
        reverseCommitMicroseconds.load(std::memory_order_relaxed);
    result.semanticEvidenceMicroseconds =
        semanticEvidenceMicroseconds.load(std::memory_order_relaxed);
    result.semanticEvidenceLotCount = semanticEvidenceLotCount;
    result.semanticEvidenceLayerPairCount = semanticEvidenceLayerPairCount;
    result.semanticEvidenceEdgeCount =
        semanticEvidenceEdgeCount.load(std::memory_order_relaxed);
    return result;
  }
};

class ScopedAtomicMicrosecondTimer {
public:
  explicit ScopedAtomicMicrosecondTimer(std::atomic<std::uint64_t>& target)
      : target_(target), started_(std::chrono::steady_clock::now()) {}

  ScopedAtomicMicrosecondTimer(const ScopedAtomicMicrosecondTimer&) = delete;
  ScopedAtomicMicrosecondTimer& operator=(const ScopedAtomicMicrosecondTimer&) = delete;

  ~ScopedAtomicMicrosecondTimer() {
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started_);
    target_.fetch_add(
        static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed.count())),
        std::memory_order_relaxed);
  }

private:
  std::atomic<std::uint64_t>& target_;
  std::chrono::steady_clock::time_point started_;
};

// P4 uses one shared worker pool for the entire support-analysis pipeline. Layer
// preparation is low-priority background work while per-component semantic work
// is foreground work. The analysis thread remains the deterministic coordinator
// and commits results in native component order. This avoids stacking an
// independent layer-preparation pool on top of a semantic executor while still
// allowing preparation to overlap the coordinator's ordered commit work.
class SupportWorkScheduler {
public:
  using Task = std::function<void(std::size_t)>;

  explicit SupportWorkScheduler(std::size_t workerCount)
      : workerCount_(std::max<std::size_t>(1u, workerCount)) {
    workers_.reserve(workerCount_);
    for (std::size_t slot = 0u; slot < workerCount_; ++slot) {
      workers_.emplace_back([this, slot] { workerMain(slot); });
    }
  }

  SupportWorkScheduler(const SupportWorkScheduler&) = delete;
  SupportWorkScheduler& operator=(const SupportWorkScheduler&) = delete;

  ~SupportWorkScheduler() {
    {
      std::scoped_lock lock(mutex_);
      stopping_ = true;
    }
    workAvailable_.notify_all();
    for (auto& worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  [[nodiscard]] std::size_t workerCount() const noexcept {
    return workerCount_;
  }

  [[nodiscard]] std::size_t backgroundWorkerCount() const noexcept {
    return workers_.size();
  }

  void submitBackground(Task task) {
    if (!task) {
      return;
    }
    {
      std::scoped_lock lock(mutex_);
      backgroundTasks_.push_back(std::move(task));
    }
    workAvailable_.notify_one();
  }

  template <typename Callback>
  void parallelFor(std::size_t itemCount, Callback&& callback) {
    if (itemCount == 0u) {
      return;
    }
    if (workers_.empty()) {
      for (std::size_t index = 0u; index < itemCount; ++index) {
        callback(index, 0u);
      }
      return;
    }

    struct BatchState {
      std::function<void(std::size_t, std::size_t)> callback;
      std::atomic<std::size_t> nextItem{0u};
      std::size_t remainingTickets = 0u;
      std::size_t itemCount = 0u;
      std::mutex completionMutex;
      std::condition_variable completion;
    };

    auto batch = std::make_shared<BatchState>();
    batch->callback = std::forward<Callback>(callback);
    batch->itemCount = itemCount;
    const auto ticketCount = std::min(workers_.size(), itemCount);
    batch->remainingTickets = ticketCount;

    auto executeBatch = [batch](std::size_t slot) {
      while (true) {
        const auto index = batch->nextItem.fetch_add(1u, std::memory_order_relaxed);
        if (index >= batch->itemCount) {
          return;
        }
        batch->callback(index, slot);
      }
    };

    {
      std::scoped_lock lock(mutex_);
      for (std::size_t ticket = 0u; ticket < ticketCount; ++ticket) {
        foregroundTasks_.push_back(
            [batch, executeBatch](std::size_t slot) {
              executeBatch(slot);
              bool complete = false;
              {
                std::scoped_lock lock(batch->completionMutex);
                if (batch->remainingTickets != 0u) {
                  --batch->remainingTickets;
                }
                complete = batch->remainingTickets == 0u;
              }
              if (complete) {
                batch->completion.notify_one();
              }
            });
      }
    }
    workAvailable_.notify_all();

    std::unique_lock lock(batch->completionMutex);
    batch->completion.wait(lock, [&] {
      return batch->remainingTickets == 0u;
    });
  }

private:
  void workerMain(std::size_t slot) {
    while (true) {
      Task task;
      {
        std::unique_lock lock(mutex_);
        workAvailable_.wait(lock, [&] {
          return stopping_ || !foregroundTasks_.empty() || !backgroundTasks_.empty();
        });
        if (stopping_) {
          return;
        }
        if (!foregroundTasks_.empty()) {
          task = std::move(foregroundTasks_.front());
          foregroundTasks_.pop_front();
        } else {
          task = std::move(backgroundTasks_.front());
          backgroundTasks_.pop_front();
        }
      }
      task(slot);
    }
  }

  std::size_t workerCount_ = 1u;
  std::mutex mutex_;
  std::condition_variable workAvailable_;
  std::deque<Task> foregroundTasks_;
  std::deque<Task> backgroundTasks_;
  std::vector<std::thread> workers_;
  bool stopping_ = false;
};

[[nodiscard]] std::size_t estimatedNativeMaskBytes(
    std::uint32_t width,
    std::uint32_t height) noexcept {
  const auto wordsPerRow = (static_cast<std::size_t>(width) + 63u) / 64u;
  if (height != 0u
      && wordsPerRow > std::numeric_limits<std::size_t>::max() / height) {
    return std::numeric_limits<std::size_t>::max();
  }
  const auto words = wordsPerRow * static_cast<std::size_t>(height);
  if (words > std::numeric_limits<std::size_t>::max() / sizeof(std::uint64_t)) {
    return std::numeric_limits<std::size_t>::max();
  }
  return words * sizeof(std::uint64_t);
}

[[nodiscard]] std::size_t adaptivePreparationWindow(
    const photons::LayerMaskSource& source,
    const SupportWorkScheduler& scheduler,
    std::size_t layerCount,
    std::size_t memoryBudgetBytes) noexcept {
  if (layerCount == 0u) {
    return 0u;
  }
  const auto maskBytes = estimatedNativeMaskBytes(source.width(), source.height());
  const auto memoryLimited = maskBytes == 0u
      ? scheduler.backgroundWorkerCount()
      : std::max<std::size_t>(1u, memoryBudgetBytes / maskBytes);
  return std::max<std::size_t>(
      1u,
      std::min({scheduler.backgroundWorkerCount(), layerCount, memoryLimited}));
}

// The semantic classifier has strict layer-to-layer dependencies, but loading
// and connected-component extraction for a layer are independent. P6.3 sizes
// this ordered background window from the configured worker count and an
// estimated native-mask memory budget instead of imposing a fixed four-layer
// cap. Foreground semantic work keeps priority in the shared scheduler, so the
// wider window fills only from otherwise idle workers. The forward consumer
// moves each compact connected-component description into the analysis cache so
// the reverse semantic traversal can reuse it without decoding the PWSZ again.
class OrderedLayerPreparer {
public:
  OrderedLayerPreparer(
      photons::LayerMaskSource& source,
      std::size_t firstLayer,
      std::size_t layerCount,
      bool descending,
      bool retainMask,
      SupportWorkScheduler& scheduler,
      std::size_t preparationMemoryBudgetBytes,
      SupportPerformanceCounters& performance)
      : source_(source),
        scheduler_(scheduler),
        firstLayer_(firstLayer),
        layerCount_(layerCount),
        descending_(descending),
        retainMask_(retainMask),
        concurrentLoads_(source.supportsConcurrentMaskLoads()),
        effectiveBackgroundWorkerCount_(adaptivePreparationWindow(
            source, scheduler, layerCount, preparationMemoryBudgetBytes)),
        performance_(performance),
        ready_(layerCount) {
    outstandingCapacity_ = effectiveBackgroundWorkerCount_;
    performance_.preparationWindowCapacity = outstandingCapacity_;
    scheduleAvailable();
  }

  OrderedLayerPreparer(const OrderedLayerPreparer&) = delete;
  OrderedLayerPreparer& operator=(const OrderedLayerPreparer&) = delete;

  void dropRetainedMasks() {
    retainMask_.store(false, std::memory_order_relaxed);
    std::scoped_lock lock(mutex_);
    for (std::size_t ordinal = nextConsume_; ordinal < ready_.size(); ++ordinal) {
      if (ready_[ordinal]) {
        ready_[ordinal]->mask.reset();
      }
    }
  }

  ~OrderedLayerPreparer() {
    if (effectiveBackgroundWorkerCount_ == 0u) {
      return;
    }
    std::unique_lock lock(mutex_);
    stopping_ = true;
    condition_.wait(lock, [&] { return scheduledTaskCount_ == 0u; });
  }

  [[nodiscard]] PreparedLayer next() {
    if (nextConsume_ >= layerCount_) {
      PreparedLayer result;
      result.error = "support analysis layer preparation exhausted";
      return result;
    }

    if (effectiveBackgroundWorkerCount_ == 0u) {
      return prepare(nextConsume_++);
    }

    std::unique_lock lock(mutex_);
    const std::size_t ordinal = nextConsume_;
    condition_.wait(lock, [&] {
      return ready_[ordinal].has_value();
    });
    PreparedLayer result = std::move(*ready_[ordinal]);
    ready_[ordinal].reset();
    ++nextConsume_;
    if (outstanding_ != 0u) {
      --outstanding_;
    }
    lock.unlock();
    scheduleAvailable();
    return result;
  }

private:
  [[nodiscard]] std::size_t layerForOrdinal(std::size_t ordinal) const {
    return descending_ ? firstLayer_ - ordinal : firstLayer_ + ordinal;
  }

  [[nodiscard]] PreparedLayer prepare(std::size_t ordinal) {
    PreparedLayer result;
    result.layer = layerForOrdinal(ordinal);

    std::optional<BinaryMask> mask;
    const auto loadStarted = std::chrono::steady_clock::now();
    if (concurrentLoads_) {
      mask = source_.loadMask(result.layer, result.error);
    } else {
      std::scoped_lock lock(sourceLoadMutex_);
      mask = source_.loadMask(result.layer, result.error);
    }
    const auto loadElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - loadStarted);
    performance_.preparationLoadMicroseconds.fetch_add(
        static_cast<std::uint64_t>(std::max<std::int64_t>(0, loadElapsed.count())),
        std::memory_order_relaxed);
    if (!mask) {
      if (result.error.empty()) {
        result.error = "support analysis could not load a layer";
      }
      return result;
    }

    const auto describeStarted = std::chrono::steady_clock::now();
    result.description = describeLayer(*mask, result.layer);
    const auto describeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - describeStarted);
    performance_.preparationDescribeMicroseconds.fetch_add(
        static_cast<std::uint64_t>(std::max<std::int64_t>(0, describeElapsed.count())),
        std::memory_order_relaxed);
    performance_.preparedLayerCount.fetch_add(1u, std::memory_order_relaxed);
    if (retainMask_.load(std::memory_order_relaxed)) {
      result.mask = std::move(mask);
    }
    result.ok = true;
    return result;
  }

  void scheduleAvailable() {
    if (effectiveBackgroundWorkerCount_ == 0u) {
      return;
    }

    std::vector<std::size_t> ordinals;
    {
      std::scoped_lock lock(mutex_);
      if (stopping_) {
        return;
      }
      while (nextAssign_ < layerCount_ && outstanding_ < outstandingCapacity_) {
        ordinals.push_back(nextAssign_++);
        ++outstanding_;
        ++scheduledTaskCount_;
        auto observed = performance_.maximumPreparationInflight.load(
            std::memory_order_relaxed);
        while (outstanding_ > observed
               && !performance_.maximumPreparationInflight.compare_exchange_weak(
                   observed, outstanding_, std::memory_order_relaxed)) {
        }
      }
    }

    for (const auto ordinal : ordinals) {
      scheduler_.submitBackground([this, ordinal](std::size_t) {
        {
          std::scoped_lock lock(mutex_);
          if (stopping_) {
            --scheduledTaskCount_;
            condition_.notify_all();
            return;
          }
        }

        auto prepared = prepare(ordinal);
        {
          std::scoped_lock lock(mutex_);
          if (!stopping_) {
            if (!retainMask_.load(std::memory_order_relaxed)) {
              prepared.mask.reset();
            }
            ready_[ordinal] = std::move(prepared);
          }
          --scheduledTaskCount_;
        }
        condition_.notify_all();
      });
    }
  }

  photons::LayerMaskSource& source_;
  SupportWorkScheduler& scheduler_;
  std::size_t firstLayer_ = 0u;
  std::size_t layerCount_ = 0u;
  bool descending_ = false;
  std::atomic<bool> retainMask_{false};
  bool concurrentLoads_ = false;
  std::size_t effectiveBackgroundWorkerCount_ = 0u;
  SupportPerformanceCounters& performance_;
  std::size_t outstandingCapacity_ = 0u;
  std::size_t nextAssign_ = 0u;
  std::size_t nextConsume_ = 0u;
  std::size_t outstanding_ = 0u;
  std::size_t scheduledTaskCount_ = 0u;
  bool stopping_ = false;
  std::mutex mutex_;
  std::mutex sourceLoadMutex_;
  std::condition_variable condition_;
  std::vector<std::optional<PreparedLayer>> ready_;
};

double equivalentDiameterMillimetres(
    const Component& component,
    const SupportAnalysisOptions& options) {
  const double area = static_cast<double>(component.area)
                      * options.pitchXMillimetres * options.pitchYMillimetres;
  return 2.0 * std::sqrt(area / std::numbers::pi);
}

std::size_t changedPixelCount(const BinaryMask& left, const BinaryMask& right) {
  if (left.width() != right.width() || left.height() != right.height()) {
    return std::numeric_limits<std::size_t>::max();
  }
  std::size_t changed = 0u;
  for (std::size_t index = 0; index < left.words().size(); ++index) {
    changed += static_cast<std::size_t>(
        std::popcount(left.words()[index] ^ right.words()[index]));
  }
  return changed;
}

std::size_t overlapPixels(const Component& left, const Component& right) {
  if (left.maxX <= right.minX || right.maxX <= left.minX
      || left.maxY <= right.minY || right.maxY <= left.minY) {
    return 0;
  }
  std::size_t result = 0;
  std::size_t leftIndex = 0;
  std::size_t rightIndex = 0;
  while (leftIndex < left.runs.size() && rightIndex < right.runs.size()) {
    const auto& a = left.runs[leftIndex];
    const auto& b = right.runs[rightIndex];
    if (a.y < b.y) {
      ++leftIndex;
      continue;
    }
    if (b.y < a.y) {
      ++rightIndex;
      continue;
    }
    const auto first = std::max(a.firstX, b.firstX);
    const auto last = std::min(a.lastX, b.lastX);
    if (first < last) {
      result += last - first;
    }
    if (a.lastX < b.lastX) {
      ++leftIndex;
    } else {
      ++rightIndex;
    }
  }
  return result;
}

double centreDistancePixels(
    const Component& left,
    const Component& right) {
  const double dx = left.centerX - right.centerX;
  const double dy = left.centerY - right.centerY;
  return std::hypot(dx, dy);
}

std::size_t translatedOverlapPixels(
    const Component& source,
    const Component& target,
    std::int64_t shiftX,
    std::int64_t shiftY) {
  std::size_t result = 0u;
  std::size_t sourceIndex = 0u;
  std::size_t targetIndex = 0u;
  while (sourceIndex < source.runs.size() && targetIndex < target.runs.size()) {
    const auto& sourceRun = source.runs[sourceIndex];
    const auto& targetRun = target.runs[targetIndex];
    const auto shiftedY = static_cast<std::int64_t>(sourceRun.y) + shiftY;
    const auto targetY = static_cast<std::int64_t>(targetRun.y);
    if (shiftedY < targetY) {
      ++sourceIndex;
      continue;
    }
    if (targetY < shiftedY) {
      ++targetIndex;
      continue;
    }

    const auto shiftedFirst = static_cast<std::int64_t>(sourceRun.firstX) + shiftX;
    const auto shiftedLast = static_cast<std::int64_t>(sourceRun.lastX) + shiftX;
    const auto first = std::max(
        shiftedFirst, static_cast<std::int64_t>(targetRun.firstX));
    const auto last = std::min(
        shiftedLast, static_cast<std::int64_t>(targetRun.lastX));
    if (first < last) {
      result += static_cast<std::size_t>(last - first);
    }
    if (shiftedLast < static_cast<std::int64_t>(targetRun.lastX)) {
      ++sourceIndex;
    } else {
      ++targetIndex;
    }
  }
  return result;
}

void canonicalizeSemanticRuns(
    std::vector<SemanticRun>& runs,
    MaterialSemantic semantic) {
  for (auto& run : runs) {
    run.semantic = semantic;
  }
  std::sort(runs.begin(), runs.end(), [](const SemanticRun& left,
                                         const SemanticRun& right) {
    if (left.y != right.y) {
      return left.y < right.y;
    }
    if (left.firstX != right.firstX) {
      return left.firstX < right.firstX;
    }
    return left.lastX < right.lastX;
  });
  std::size_t write = 0u;
  for (const auto& run : runs) {
    if (run.firstX >= run.lastX) {
      continue;
    }
    if (write != 0u
        && runs[write - 1u].y == run.y
        && run.firstX <= runs[write - 1u].lastX) {
      runs[write - 1u].lastX = std::max(runs[write - 1u].lastX, run.lastX);
    } else {
      runs[write++] = run;
    }
  }
  runs.resize(write);
}

std::size_t semanticRunPixelCount(const std::vector<SemanticRun>& runs) {
  std::size_t result = 0u;
  for (const auto& run : runs) {
    if (run.firstX < run.lastX) {
      result += static_cast<std::size_t>(run.lastX - run.firstX);
    }
  }
  return result;
}

class SparseRunMask {
public:
  explicit SparseRunMask(
      std::uint32_t height = 0u,
      bool enableBitsetAcceleration = true)
      : rows_(height),
        bitRows_(height),
        enableBitsetAcceleration_(enableBitsetAcceleration) {}

  void clear() {
    for (const auto y : touchedRows_) {
      rows_[y].clear();
      bitRows_[y].clear();
    }
    touchedRows_.clear();
    hasBitRows_ = false;
  }

  [[nodiscard]] bool empty() const noexcept {
    return touchedRows_.empty();
  }

  void swap(SparseRunMask& other) noexcept {
    rows_.swap(other.rows_);
    bitRows_.swap(other.bitRows_);
    touchedRows_.swap(other.touchedRows_);
    std::swap(enableBitsetAcceleration_, other.enableBitsetAcceleration_);
    std::swap(hasBitRows_, other.hasBitRows_);
  }

  void addRuns(const std::vector<SemanticRun>& runs) {
    for (const auto& run : runs) {
      addInterval(run.y, run.firstX, run.lastX);
    }
  }

  void addIntersection(
      const std::vector<SemanticRun>& source,
      const SparseRunMask& selected) {
    for (const auto& run : source) {
      if (run.y >= rows_.size() || run.firstX >= run.lastX) {
        continue;
      }
      const auto& selectedBits = selected.bitRows_[run.y];
      if (selected.hasBitRows_ && !selectedBits.empty()) {
        forEachHybridBitRun(
            selectedBits,
            static_cast<std::int64_t>(run.firstX),
            static_cast<std::int64_t>(run.lastX),
            [&](std::uint32_t first, std::uint32_t last) {
              addInterval(run.y, first, last);
            });
        continue;
      }
      const auto& selectedRow = selected.rows_[run.y];
      auto iterator = std::lower_bound(
          selectedRow.begin(), selectedRow.end(), run.firstX,
          [](const PixelRun& interval, std::uint32_t firstX) {
            return interval.second <= firstX;
          });
      for (; iterator != selectedRow.end() && iterator->first < run.lastX;
           ++iterator) {
        addInterval(
            run.y,
            std::max(run.firstX, iterator->first),
            std::min(run.lastX, iterator->second));
      }
    }
  }

  void addShiftedRuns(
      const std::vector<SemanticRun>& source,
      std::int64_t shiftX,
      std::int64_t shiftY) {
    for (const auto& run : source) {
      const auto shiftedY = static_cast<std::int64_t>(run.y) + shiftY;
      const auto shiftedFirst = static_cast<std::int64_t>(run.firstX) + shiftX;
      const auto shiftedLast = static_cast<std::int64_t>(run.lastX) + shiftX;
      if (shiftedY < 0
          || shiftedY >= static_cast<std::int64_t>(rows_.size())
          || shiftedLast <= 0
          || shiftedFirst >= shiftedLast) {
        continue;
      }
      addInterval(
          static_cast<std::uint32_t>(shiftedY),
          static_cast<std::uint32_t>(std::max<std::int64_t>(0, shiftedFirst)),
          static_cast<std::uint32_t>(shiftedLast));
    }
  }

  void appendTranslatedIntersectionRuns(
      const std::vector<SemanticRun>& source,
      std::int64_t shiftX,
      std::int64_t shiftY,
      MaterialSemantic semantic,
      std::vector<SemanticRun>& output) const {
    for (const auto& run : source) {
      const auto selectedY = static_cast<std::int64_t>(run.y) - shiftY;
      if (selectedY < 0
          || selectedY >= static_cast<std::int64_t>(rows_.size())
          || run.firstX >= run.lastX) {
        continue;
      }
      const auto y = static_cast<std::size_t>(selectedY);
      const auto& selectedBits = bitRows_[y];
      if (hasBitRows_ && !selectedBits.empty()) {
        const auto selectedFirst = static_cast<std::int64_t>(run.firstX) - shiftX;
        const auto selectedLast = static_cast<std::int64_t>(run.lastX) - shiftX;
        forEachHybridBitRun(
            selectedBits, selectedFirst, selectedLast,
            [&](std::uint32_t first, std::uint32_t last) {
              const auto shiftedFirst = static_cast<std::int64_t>(first) + shiftX;
              const auto shiftedLast = static_cast<std::int64_t>(last) + shiftX;
              if (shiftedFirst < shiftedLast
                  && shiftedFirst >= 0
                  && shiftedLast <= static_cast<std::int64_t>(
                      std::numeric_limits<std::uint32_t>::max())) {
                output.push_back(SemanticRun{
                    run.y,
                    static_cast<std::uint32_t>(shiftedFirst),
                    static_cast<std::uint32_t>(shiftedLast),
                    semantic});
              }
            });
        continue;
      }
      for (const auto& interval : rows_[y]) {
        const auto shiftedFirst = static_cast<std::int64_t>(interval.first) + shiftX;
        const auto shiftedLast = static_cast<std::int64_t>(interval.second) + shiftX;
        if (shiftedLast <= static_cast<std::int64_t>(run.firstX)) {
          continue;
        }
        if (shiftedFirst >= static_cast<std::int64_t>(run.lastX)) {
          break;
        }
        const auto first = std::max(
            static_cast<std::int64_t>(run.firstX), shiftedFirst);
        const auto last = std::min(
            static_cast<std::int64_t>(run.lastX), shiftedLast);
        if (first < last) {
          output.push_back(SemanticRun{
              run.y,
              static_cast<std::uint32_t>(first),
              static_cast<std::uint32_t>(last),
              semantic});
        }
      }
    }
  }

  [[nodiscard]] std::size_t countTranslatedSet(
      const std::vector<SemanticRun>& source,
      std::int64_t shiftX,
      std::int64_t shiftY) const {
    std::size_t result = 0u;
    for (const auto& run : source) {
      const auto selectedY = static_cast<std::int64_t>(run.y) - shiftY;
      if (selectedY < 0
          || selectedY >= static_cast<std::int64_t>(rows_.size())
          || run.firstX >= run.lastX) {
        continue;
      }
      const auto y = static_cast<std::size_t>(selectedY);
      const auto& selectedBits = bitRows_[y];
      if (hasBitRows_ && !selectedBits.empty()) {
        result += countHybridBitRowRange(
            selectedBits,
            static_cast<std::int64_t>(run.firstX) - shiftX,
            static_cast<std::int64_t>(run.lastX) - shiftX);
        continue;
      }
      for (const auto& interval : rows_[y]) {
        const auto shiftedFirst = static_cast<std::int64_t>(interval.first) + shiftX;
        const auto shiftedLast = static_cast<std::int64_t>(interval.second) + shiftX;
        if (shiftedLast <= static_cast<std::int64_t>(run.firstX)) {
          continue;
        }
        if (shiftedFirst >= static_cast<std::int64_t>(run.lastX)) {
          break;
        }
        const auto first = std::max(
            static_cast<std::int64_t>(run.firstX), shiftedFirst);
        const auto last = std::min(
            static_cast<std::int64_t>(run.lastX), shiftedLast);
        if (first < last) {
          result += static_cast<std::size_t>(last - first);
        }
      }
    }
    return result;
  }

  void assignDilated(
      const SparseRunMask& source,
      std::uint32_t radiusPixels) {
    clear();
    if (rows_.empty()) {
      return;
    }
    for (const auto y : source.touchedRows_) {
      const auto firstY = y > radiusPixels ? y - radiusPixels : 0u;
      const auto lastY = std::min<std::uint32_t>(
          static_cast<std::uint32_t>(rows_.size() - 1u), y + radiusPixels);
      for (std::uint32_t expandedY = firstY; expandedY <= lastY; ++expandedY) {
        for (const auto& interval : source.rows_[y]) {
          const auto firstX = interval.first > radiusPixels
              ? interval.first - radiusPixels
              : 0u;
          const auto lastX = interval.second + radiusPixels;
          addInterval(expandedY, firstX, lastX);
        }
      }
    }
    normalize();
  }

  void normalize() {
    for (const auto y : touchedRows_) {
      auto& row = rows_[y];
      std::sort(row.begin(), row.end());
      std::size_t write = 0u;
      for (const auto& interval : row) {
        if (write != 0u && interval.first <= row[write - 1u].second) {
          row[write - 1u].second = std::max(row[write - 1u].second, interval.second);
        } else {
          row[write++] = interval;
        }
      }
      row.resize(write);
      rebuildBitRow(y);
    }
    std::sort(touchedRows_.begin(), touchedRows_.end());
  }

  void assignFrom(const SparseRunMask& source) {
    clear();
    for (const auto y : source.touchedRows_) {
      rows_[y] = source.rows_[y];
      touchedRows_.push_back(y);
      if (enableBitsetAcceleration_) {
        if (source.hasBitRows_ && !source.bitRows_[y].empty()) {
          bitRows_[y] = source.bitRows_[y];
          hasBitRows_ = true;
        } else {
          rebuildBitRow(y);
        }
      }
    }
  }

  void assignUnion(
      const SparseRunMask& first,
      const SparseRunMask& second) {
    clear();
    addMask(first);
    addMask(second);
    normalize();
  }

  void assignIntersection(
      const SparseRunMask& left,
      const SparseRunMask& right) {
    clear();
    const auto* sparse = &left;
    const auto* other = &right;
    if (right.touchedRows_.size() < left.touchedRows_.size()) {
      sparse = &right;
      other = &left;
    }
    for (const auto y : sparse->touchedRows_) {
      if (y >= rows_.size()) {
        continue;
      }
      const auto& a = sparse->rows_[y];
      const auto& b = other->rows_[y];
      if (a.empty() || b.empty()) {
        continue;
      }

      if (enableBitsetAcceleration_
          && sparse->hasBitRows_
          && other->hasBitRows_
          && !sparse->bitRows_[y].empty()
          && !other->bitRows_[y].empty()) {
        const auto& aBits = sparse->bitRows_[y];
        const auto& bBits = other->bitRows_[y];
        const auto firstWord = std::max(aBits.firstWord, bBits.firstWord);
        const auto aLastWord = static_cast<std::uint64_t>(aBits.firstWord)
            + aBits.words.size();
        const auto bLastWord = static_cast<std::uint64_t>(bBits.firstWord)
            + bBits.words.size();
        const auto lastWord = std::min(aLastWord, bLastWord);
        if (static_cast<std::uint64_t>(firstWord) < lastWord) {
          auto& outputBits = bitRows_[y];
          outputBits.firstWord = firstWord;
          outputBits.words.assign(
              static_cast<std::size_t>(lastWord - firstWord), 0u);
          bitwiseAndWords(
              aBits.words.data() + (firstWord - aBits.firstWord),
              bBits.words.data() + (firstWord - bBits.firstWord),
              outputBits.words.data(),
              outputBits.words.size());
          auto& output = rows_[y];
          forEachHybridBitRun(
              outputBits,
              static_cast<std::int64_t>(firstWord) * 64,
              static_cast<std::int64_t>(lastWord) * 64,
              [&](std::uint32_t first, std::uint32_t last) {
                output.emplace_back(first, last);
              });
          if (!output.empty()) {
            touchedRows_.push_back(y);
            hasBitRows_ = true;
          } else {
            outputBits.clear();
          }
          continue;
        }
      }

      auto& output = rows_[y];
      std::size_t ai = 0u;
      std::size_t bi = 0u;
      while (ai < a.size() && bi < b.size()) {
        const auto first = std::max(a[ai].first, b[bi].first);
        const auto last = std::min(a[ai].second, b[bi].second);
        if (first < last) {
          output.emplace_back(first, last);
        }
        if (a[ai].second < b[bi].second) {
          ++ai;
        } else {
          ++bi;
        }
      }
      if (!output.empty()) {
        touchedRows_.push_back(y);
        rebuildBitRow(y);
      }
    }
  }

  [[nodiscard]] std::size_t countSet(
      const std::vector<SemanticRun>& runs) const {
    std::size_t result = 0u;
    for (const auto& run : runs) {
      if (run.y >= rows_.size() || run.firstX >= run.lastX) {
        continue;
      }
      const auto& bits = bitRows_[run.y];
      if (hasBitRows_ && !bits.empty()) {
        result += countHybridBitRowRange(
            bits,
            static_cast<std::int64_t>(run.firstX),
            static_cast<std::int64_t>(run.lastX));
        continue;
      }
      const auto& row = rows_[run.y];
      auto iterator = std::lower_bound(
          row.begin(), row.end(), run.firstX,
          [](const PixelRun& interval, std::uint32_t firstX) {
            return interval.second <= firstX;
          });
      for (; iterator != row.end() && iterator->first < run.lastX; ++iterator) {
        const auto first = std::max(run.firstX, iterator->first);
        const auto last = std::min(run.lastX, iterator->second);
        if (first < last) {
          result += static_cast<std::size_t>(last - first);
        }
      }
    }
    return result;
  }

  void rasterizeRegion(
      std::uint32_t minX,
      std::uint32_t minY,
      std::uint32_t width,
      std::uint32_t height,
      std::uint32_t wordsPerRow,
      std::span<std::uint32_t> output) const {
    if (width == 0u || height == 0u || wordsPerRow == 0u) {
      return;
    }
    const auto maxY = std::min<std::uint64_t>(
        rows_.size(), static_cast<std::uint64_t>(minY) + height);
    const auto maxX = static_cast<std::uint64_t>(minX) + width;
    for (std::uint64_t y = minY; y < maxY; ++y) {
      for (const auto& interval : rows_[static_cast<std::size_t>(y)]) {
        const auto first = std::max<std::uint64_t>(interval.first, minX);
        const auto last = std::min<std::uint64_t>(interval.second, maxX);
        if (first >= last) {
          continue;
        }
        setDenseRange32(
            output,
            wordsPerRow,
            static_cast<std::uint32_t>(y - minY),
            static_cast<std::uint32_t>(first - minX),
            static_cast<std::uint32_t>(last - minX));
      }
    }
  }

  void appendClearRuns(
      const std::vector<SemanticRun>& matterRuns,
      MaterialSemantic semantic,
      std::vector<SemanticRun>& output) const {
    for (const auto& run : matterRuns) {
      if (run.y >= rows_.size() || run.firstX >= run.lastX) {
        continue;
      }
      const auto& row = rows_[run.y];
      auto iterator = std::lower_bound(
          row.begin(), row.end(), run.firstX,
          [](const PixelRun& interval, std::uint32_t firstX) {
            return interval.second <= firstX;
          });
      auto cursor = run.firstX;
      for (; iterator != row.end() && iterator->first < run.lastX; ++iterator) {
        if (iterator->first > cursor) {
          output.push_back(SemanticRun{
              run.y, cursor, std::min(iterator->first, run.lastX), semantic});
        }
        cursor = std::max(cursor, iterator->second);
        if (cursor >= run.lastX) {
          break;
        }
      }
      if (cursor < run.lastX) {
        output.push_back(SemanticRun{run.y, cursor, run.lastX, semantic});
      }
    }
  }

private:
  void addInterval(
      std::uint32_t y,
      std::uint32_t firstX,
      std::uint32_t lastX) {
    if (y >= rows_.size() || firstX >= lastX) {
      return;
    }
    auto& row = rows_[y];
    if (row.empty()) {
      touchedRows_.push_back(y);
    }
    bitRows_[y].clear();
    row.emplace_back(firstX, lastX);
  }

  void addMask(const SparseRunMask& source) {
    for (const auto y : source.touchedRows_) {
      auto& row = rows_[y];
      if (row.empty()) {
        touchedRows_.push_back(y);
      }
      bitRows_[y].clear();
      row.insert(row.end(), source.rows_[y].begin(), source.rows_[y].end());
    }
  }

  void rebuildBitRow(std::uint32_t y) {
    bitRows_[y].clear();
    if (enableBitsetAcceleration_
        && rows_[y].size() >= kBitsetMinimumIntervals
        && buildHybridBitRow(rows_[y], bitRows_[y])) {
      hasBitRows_ = true;
    }
  }

  std::vector<std::vector<PixelRun>> rows_;
  std::vector<HybridBitRow> bitRows_;
  std::vector<std::uint32_t> touchedRows_;
  bool enableBitsetAcceleration_ = true;
  bool hasBitRows_ = false;
};

struct TranslatedOverlapScratch {
  std::vector<std::uint32_t> sourceWords;
  std::vector<std::uint32_t> referenceWords;
  std::vector<std::uint32_t> overlaps;
  std::vector<compute::SupportComputeRun> sourceRuns;
};

struct ResidentReferenceScratch {
  std::vector<std::uint32_t> words;
  compute::TranslatedRunOverlapBatch batch;
};

bool prepareResidentReferenceBatch(
    const SparseRunMask& reference,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t radius,
    std::uint64_t referenceKey,
    ResidentReferenceScratch& scratch) {
  if (width == 0u || height == 0u || radius > 31u || reference.empty()) {
    return false;
  }
  const auto wordsPerRow = (width + 31u) / 32u;
  const auto wordCount64 = static_cast<std::uint64_t>(wordsPerRow) * height;
  constexpr std::uint64_t kMaximumResidentWords = (128ull * 1024ull * 1024ull)
      / sizeof(std::uint32_t);
  if (wordCount64 == 0u
      || wordCount64 > kMaximumResidentWords
      || wordCount64 > std::numeric_limits<std::size_t>::max()) {
    return false;
  }
  scratch.words.assign(static_cast<std::size_t>(wordCount64), 0u);
  reference.rasterizeRegion(
      0u, 0u, width, height, wordsPerRow, scratch.words);
  scratch.batch.referenceKey = referenceKey;
  scratch.batch.width = width;
  scratch.batch.height = height;
  scratch.batch.wordsPerRow = wordsPerRow;
  scratch.batch.radius = radius;
  scratch.batch.referenceWords = scratch.words;
  scratch.batch.sourceRuns = {};
  return true;
}

bool prepareTranslatedRunBatch(
    const Component& current,
    const compute::TranslatedRunOverlapBatch& residentReference,
    TranslatedOverlapScratch& scratch,
    compute::TranslatedRunOverlapBatch& batch) {
  if (current.runs.empty() || current.area == 0u
      || residentReference.referenceWords.empty()
      || residentReference.referenceKey == 0u) {
    return false;
  }
  scratch.sourceRuns.clear();
  if (scratch.sourceRuns.capacity() < current.runs.size()) {
    scratch.sourceRuns.reserve(current.runs.size());
  }
  for (const auto& run : current.runs) {
    if (run.firstX >= run.lastX) {
      continue;
    }
    scratch.sourceRuns.push_back(compute::SupportComputeRun{
        run.y, run.firstX, run.lastX, 0u});
  }
  if (scratch.sourceRuns.empty()) {
    return false;
  }
  const auto diameter = static_cast<std::size_t>(residentReference.radius) * 2u + 1u;
  scratch.overlaps.assign(diameter * diameter, 0u);
  batch = residentReference;
  batch.sourceRuns = scratch.sourceRuns;
  return true;
}

bool prepareTranslatedOverlapBatch(
    const Component& current,
    const SparseRunMask& reference,
    std::uint32_t radius,
    TranslatedOverlapScratch& scratch,
    compute::TranslatedOverlapBatch& batch) {
  if (current.runs.empty() || current.area == 0u || radius > 31u) {
    return false;
  }

  const auto minX = current.minX > radius ? current.minX - radius : 0u;
  const auto minY = current.minY > radius ? current.minY - radius : 0u;
  const auto maxX64 = std::min<std::uint64_t>(
      std::numeric_limits<std::uint32_t>::max(),
      static_cast<std::uint64_t>(current.maxX) + radius);
  const auto maxY64 = std::min<std::uint64_t>(
      std::numeric_limits<std::uint32_t>::max(),
      static_cast<std::uint64_t>(current.maxY) + radius);
  if (maxX64 <= minX || maxY64 <= minY) {
    return false;
  }
  const auto width = static_cast<std::uint32_t>(maxX64 - minX);
  const auto height = static_cast<std::uint32_t>(maxY64 - minY);
  const auto wordsPerRow = (width + 31u) / 32u;
  const auto wordCount64 = static_cast<std::uint64_t>(wordsPerRow) * height;
  // Bound temporary host-visible payloads. Two 64 MiB input buffers are enough
  // for the largest normal resin frames while avoiding accidental unbounded
  // allocations on malformed metadata.
  constexpr std::uint64_t kMaximumDenseWords = (64ull * 1024ull * 1024ull)
      / sizeof(std::uint32_t);
  if (wordCount64 == 0u
      || wordCount64 > kMaximumDenseWords
      || wordCount64 > std::numeric_limits<std::size_t>::max()) {
    return false;
  }

  const auto wordCount = static_cast<std::size_t>(wordCount64);
  scratch.sourceWords.assign(wordCount, 0u);
  scratch.referenceWords.assign(wordCount, 0u);
  for (const auto& run : current.runs) {
    if (run.y < minY
        || run.y >= static_cast<std::uint64_t>(minY) + height
        || run.firstX >= run.lastX) {
      continue;
    }
    const auto first = std::max<std::uint64_t>(run.firstX, minX);
    const auto last = std::min<std::uint64_t>(
        run.lastX, static_cast<std::uint64_t>(minX) + width);
    if (first >= last) {
      continue;
    }
    setDenseRange32(
        scratch.sourceWords,
        wordsPerRow,
        run.y - minY,
        static_cast<std::uint32_t>(first - minX),
        static_cast<std::uint32_t>(last - minX));
  }
  reference.rasterizeRegion(
      minX, minY, width, height, wordsPerRow, scratch.referenceWords);

  const auto diameter = static_cast<std::size_t>(radius) * 2u + 1u;
  scratch.overlaps.assign(diameter * diameter, 0u);
  batch.wordsPerRow = wordsPerRow;
  batch.height = height;
  batch.radius = radius;
  batch.sourceWords = scratch.sourceWords;
  batch.referenceWords = scratch.referenceWords;
  return true;
}

struct NodeState {
  std::size_t nodeId = 0;
  std::size_t parent = std::numeric_limits<std::size_t>::max();
  std::size_t depth = 1;
  const Component* component = nullptr;
  std::size_t runCount = 0;
  bool accepted = false;
  bool classifiedAsModel = false;
  bool modelContact = false;
  bool supportContact = false;
  std::size_t pendingContactTip = std::numeric_limits<std::size_t>::max();
  std::size_t pendingContactStart = std::numeric_limits<std::size_t>::max();
  std::size_t pendingContactLength = 0;
  std::vector<std::size_t> pendingContactTips;
  std::size_t contactNode = std::numeric_limits<std::size_t>::max();
  std::size_t branchOrigin = std::numeric_limits<std::size_t>::max();
  std::size_t contactLayer = std::numeric_limits<std::size_t>::max();
  std::size_t contactModelPixelCount = 0;
  double contactModelExpansionRatio = 0.0;
  bool hasSemanticProjection = false;
  // Pass 1 support provenance is independent from the provisional one-pass
  // semantic verdict. Every viable parent that still descends from the raft is
  // retained so a later merge cannot erase an otherwise valid support branch.
  bool supportEvidenceActive = false;
  std::vector<std::size_t> supportEvidenceParents;
  // Conservative pass-1 support core aligned on this native layer. When no
  // reverse model evidence reaches the component, the whole support component
  // may still be accepted. In a conflict, only this inherited core is protected
  // so model matter cannot be swallowed by a merged support component.
  std::vector<SemanticRun> supportEvidenceRuns;
  std::vector<SemanticRun> projectedSupportRuns;
};

struct SupportMotionComparison {
  std::size_t alignedOverlapPixels = 0u;
  std::size_t addedPixels = 0u;
  std::size_t removedPixels = 0u;
  double predictedMotionX = 0.0;
  double predictedMotionY = 0.0;
  double motionResidual = 0.0;
  double alignedOverlapRatio = 0.0;
  double alignedIntersectionOverUnion = 0.0;
  bool hasMotionPrediction = false;
  bool explainedBySupportMotion = false;
};

struct ModelLineageMotion {
  std::size_t overlapPixels = 0u;
  std::int64_t shiftX = 0;
  std::int64_t shiftY = 0;
  bool continued = false;
};

struct TranslatedOverlapEvidence {
  std::span<const std::uint32_t> acceleratedOverlaps;
};

TranslatedOverlapEvidence prepareTranslatedOverlapEvidence(
    const Component& current,
    const SparseRunMask& previousStableModel,
    std::uint32_t maximumShiftPixels,
    compute::SupportComputeBackend* computeBackend,
    TranslatedOverlapScratch* computeScratch,
    const compute::TranslatedRunOverlapBatch* residentReference,
    std::size_t vulkanMinimumAreaPixels) {
  TranslatedOverlapEvidence evidence;
  if (computeBackend == nullptr
      || computeScratch == nullptr
      || previousStableModel.empty()
      || current.area < vulkanMinimumAreaPixels
      || maximumShiftPixels == 0u
      || maximumShiftPixels > 31u) {
    return evidence;
  }

  computeBackend->recordEligibleJob();
  const auto preparationStarted = std::chrono::steady_clock::now();
  std::string computeError;
  bool usedComputeBackend = false;
  if (residentReference != nullptr
      && residentReference->radius == maximumShiftPixels) {
    compute::TranslatedRunOverlapBatch batch;
    if (prepareTranslatedRunBatch(
            current, *residentReference, *computeScratch, batch)) {
      const auto preparationEnded = std::chrono::steady_clock::now();
      computeBackend->recordHostPreparation(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              preparationEnded - preparationStarted).count()));
      usedComputeBackend = computeBackend->translatedRunOverlaps(
          batch, computeScratch->overlaps, computeError);
    } else {
      const auto preparationEnded = std::chrono::steady_clock::now();
      computeBackend->recordHostPreparation(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              preparationEnded - preparationStarted).count()));
      computeBackend->recordCpuFallbackJob();
    }
  } else {
    compute::TranslatedOverlapBatch batch;
    if (prepareTranslatedOverlapBatch(
            current, previousStableModel, maximumShiftPixels,
            *computeScratch, batch)) {
      const auto preparationEnded = std::chrono::steady_clock::now();
      computeBackend->recordHostPreparation(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              preparationEnded - preparationStarted).count()));
      usedComputeBackend = computeBackend->translatedOverlaps(
          batch, computeScratch->overlaps, computeError);
    } else {
      const auto preparationEnded = std::chrono::steady_clock::now();
      computeBackend->recordHostPreparation(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              preparationEnded - preparationStarted).count()));
      computeBackend->recordCpuFallbackJob();
    }
  }

  if (usedComputeBackend) {
    evidence.acceleratedOverlaps = computeScratch->overlaps;
  }
  return evidence;
}

ModelLineageMotion selectBestLineageMotion(
    const Component& current,
    const SparseRunMask& previousStableModel,
    const SparseRunMask* previousPreviousStableModel,
    std::uint32_t maximumShiftPixels,
    const SparseRunMask* competingSupport,
    const TranslatedOverlapEvidence& overlapEvidence) {
  ModelLineageMotion best;
  if (previousStableModel.empty()) {
    return best;
  }

  struct CandidateShift {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::uint64_t squaredDistance = 0u;
  };
  std::vector<CandidateShift> candidates;
  const auto radius = static_cast<std::int64_t>(maximumShiftPixels);
  const auto diameter = static_cast<std::size_t>(maximumShiftPixels) * 2u + 1u;
  if (diameter != 0u
      && diameter <= std::numeric_limits<std::size_t>::max() / diameter) {
    candidates.reserve(diameter * diameter);
  }

  std::size_t candidateIndex = 0u;
  for (std::int64_t shiftY = -radius; shiftY <= radius; ++shiftY) {
    for (std::int64_t shiftX = -radius; shiftX <= radius;
         ++shiftX, ++candidateIndex) {
      const auto overlap = candidateIndex < overlapEvidence.acceleratedOverlaps.size()
          ? static_cast<std::size_t>(
                overlapEvidence.acceleratedOverlaps[candidateIndex])
          : previousStableModel.countTranslatedSet(current.runs, shiftX, shiftY);
      if (overlap < best.overlapPixels) {
        continue;
      }
      if (overlap > best.overlapPixels) {
        best.overlapPixels = overlap;
        candidates.clear();
      }
      candidates.push_back(CandidateShift{
          shiftX,
          shiftY,
          static_cast<std::uint64_t>(shiftX * shiftX + shiftY * shiftY)});
    }
  }
  if (best.overlapPixels == 0u || candidates.empty()) {
    return best;
  }

  std::size_t bestOutsideSupportOverlap = 0u;
  std::size_t bestHistoricalOverlap = 0u;
  std::uint64_t bestSquaredDistance = std::numeric_limits<std::uint64_t>::max();
  for (const auto& candidate : candidates) {
    std::vector<SemanticRun> selectedCurrentRuns;
    if (competingSupport != nullptr
        || (previousPreviousStableModel != nullptr
            && !previousPreviousStableModel->empty())) {
      previousStableModel.appendTranslatedIntersectionRuns(
          current.runs, candidate.x, candidate.y,
          MaterialSemantic::Model, selectedCurrentRuns);
    }
    std::size_t outsideSupportOverlap = best.overlapPixels;
    if (competingSupport != nullptr && !competingSupport->empty()) {
      const auto supportOverlap = competingSupport->countSet(selectedCurrentRuns);
      outsideSupportOverlap = supportOverlap >= best.overlapPixels
          ? 0u
          : best.overlapPixels - supportOverlap;
    }

    std::size_t historicalOverlap = 0u;
    if (previousPreviousStableModel != nullptr
        && !previousPreviousStableModel->empty()) {
      std::vector<SemanticRun> selectedPreviousRuns;
      selectedPreviousRuns.reserve(selectedCurrentRuns.size());
      for (const auto& run : selectedCurrentRuns) {
        const auto previousY = static_cast<std::int64_t>(run.y) - candidate.y;
        const auto previousFirst = static_cast<std::int64_t>(run.firstX) - candidate.x;
        const auto previousLast = static_cast<std::int64_t>(run.lastX) - candidate.x;
        if (previousY < 0 || previousFirst < 0 || previousFirst >= previousLast) {
          continue;
        }
        selectedPreviousRuns.push_back(SemanticRun{
            static_cast<std::uint32_t>(previousY),
            static_cast<std::uint32_t>(previousFirst),
            static_cast<std::uint32_t>(previousLast),
            MaterialSemantic::Model});
      }
      historicalOverlap = previousPreviousStableModel->countTranslatedSet(
          selectedPreviousRuns, candidate.x, candidate.y);
    }
    if (outsideSupportOverlap > bestOutsideSupportOverlap
        || (outsideSupportOverlap == bestOutsideSupportOverlap
            && (historicalOverlap > bestHistoricalOverlap
                || (historicalOverlap == bestHistoricalOverlap
                    && candidate.squaredDistance < bestSquaredDistance)))) {
      best.shiftX = candidate.x;
      best.shiftY = candidate.y;
      bestOutsideSupportOverlap = outsideSupportOverlap;
      bestHistoricalOverlap = historicalOverlap;
      bestSquaredDistance = candidate.squaredDistance;
    }
  }
  best.continued = true;
  return best;
}

ModelLineageMotion bestTranslatedLineageMotion(
    const Component& current,
    const SparseRunMask& previousStableModel,
    const SparseRunMask* previousPreviousStableModel,
    std::uint32_t maximumShiftPixels,
    const SparseRunMask* competingSupport = nullptr,
    compute::SupportComputeBackend* computeBackend = nullptr,
    TranslatedOverlapScratch* computeScratch = nullptr,
    const compute::TranslatedRunOverlapBatch* residentReference = nullptr,
    std::size_t vulkanMinimumAreaPixels = std::numeric_limits<std::size_t>::max()) {
  const auto overlapEvidence = prepareTranslatedOverlapEvidence(
      current, previousStableModel, maximumShiftPixels,
      computeBackend, computeScratch, residentReference,
      vulkanMinimumAreaPixels);
  return selectBestLineageMotion(
      current, previousStableModel, previousPreviousStableModel,
      maximumShiftPixels, competingSupport, overlapEvidence);
}

SupportMotionComparison compareSupportMotion(
    std::size_t parentNode,
    const Component& current,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
  SupportMotionComparison comparison;
  if (parentNode >= states.size()) {
    return comparison;
  }

  const auto& parentState = states[parentNode];
  const auto& parent = *parentState.component;
  const double actualMotionX = current.centerX - parent.centerX;
  const double actualMotionY = current.centerY - parent.centerY;
  const auto alignmentX = static_cast<std::int64_t>(std::llround(actualMotionX));
  const auto alignmentY = static_cast<std::int64_t>(std::llround(actualMotionY));
  comparison.alignedOverlapPixels = translatedOverlapPixels(
      parent, current, alignmentX, alignmentY);
  comparison.addedPixels = current.area > comparison.alignedOverlapPixels
      ? current.area - comparison.alignedOverlapPixels
      : 0u;
  comparison.removedPixels = parent.area > comparison.alignedOverlapPixels
      ? parent.area - comparison.alignedOverlapPixels
      : 0u;
  const auto smallerArea = std::min(parent.area, current.area);
  if (smallerArea != 0u) {
    comparison.alignedOverlapRatio =
        static_cast<double>(comparison.alignedOverlapPixels)
        / static_cast<double>(smallerArea);
  }
  const auto unionArea = parent.area + current.area
                         - comparison.alignedOverlapPixels;
  if (unionArea != 0u) {
    comparison.alignedIntersectionOverUnion =
        static_cast<double>(comparison.alignedOverlapPixels)
        / static_cast<double>(unionArea);
  }

  if (parentState.parent != std::numeric_limits<std::size_t>::max()) {
    const auto& grandParent = *states[parentState.parent].component;
    comparison.hasMotionPrediction = true;
    comparison.predictedMotionX = parent.centerX - grandParent.centerX;
    comparison.predictedMotionY = parent.centerY - grandParent.centerY;
    comparison.motionResidual = std::hypot(
        actualMotionX - comparison.predictedMotionX,
        actualMotionY - comparison.predictedMotionY);
  } else {
    comparison.motionResidual = std::hypot(actualMotionX, actualMotionY);
  }

  const bool shapePreserved = comparison.alignedOverlapRatio
                              >= options.minimumSupportShapeOverlapRatio;
  const bool trajectoryPreserved = !comparison.hasMotionPrediction
                                   || comparison.motionResidual
                                          <= options.maximumLayerMotionPixels;
  comparison.explainedBySupportMotion = shapePreserved && trajectoryPreserved;
  return comparison;
}

struct Match {
  std::size_t previousNode = 0;
  std::size_t overlap = 0;
  double centreDistance = 0.0;
  double materialDistance = std::numeric_limits<double>::infinity();
};

struct ComponentPairMetrics {
  std::size_t overlap = 0u;
  double centreDistance = 0.0;
  double materialDistance = std::numeric_limits<double>::infinity();
  bool nearEnough = false;
};

double minimumSeparatedMaterialDistancePixels(
    const Component& left,
    const Component& right,
    double maximumDistance) {
  if (left.runs.empty() || right.runs.empty()) {
    return std::numeric_limits<double>::infinity();
  }

  const auto margin = static_cast<std::uint32_t>(
      std::max(0.0, std::ceil(maximumDistance)));
  const auto horizontalGap = [](const SemanticRun& a, const SemanticRun& b) {
    if (a.lastX < b.firstX) {
      return b.firstX - a.lastX;
    }
    if (b.lastX < a.firstX) {
      return a.firstX - b.lastX;
    }
    return 0u;
  };

  const auto* smaller = &left;
  const auto* larger = &right;
  if (left.runs.size() > right.runs.size()) {
    std::swap(smaller, larger);
  }

  double best = std::numeric_limits<double>::infinity();
  for (const auto& run : smaller->runs) {
    const auto firstY = run.y > margin ? run.y - margin : 0u;
    const auto lastY = run.y + margin;
    auto iterator = std::lower_bound(
        larger->runs.begin(), larger->runs.end(), firstY,
        [](const SemanticRun& candidate, std::uint32_t y) {
          return candidate.y < y;
        });
    for (; iterator != larger->runs.end() && iterator->y <= lastY; ++iterator) {
      const double dx = static_cast<double>(horizontalGap(run, *iterator));
      const double dy = static_cast<double>(
          run.y > iterator->y ? run.y - iterator->y : iterator->y - run.y);
      const double distance = std::hypot(dx, dy);
      best = std::min(best, distance);
      if (best == 0.0) {
        return 0.0;
      }
    }
  }
  return best;
}

ComponentPairMetrics componentPairMetrics(
    const Component& left,
    const Component& right,
    double maximumDistance) {
  ComponentPairMetrics metrics;
  metrics.overlap = overlapPixels(left, right);
  metrics.materialDistance = metrics.overlap != 0u
      ? 0.0
      : minimumSeparatedMaterialDistancePixels(left, right, maximumDistance);
  metrics.nearEnough = metrics.materialDistance <= maximumDistance;
  if (metrics.nearEnough) {
    metrics.centreDistance = centreDistancePixels(left, right);
  }
  return metrics;
}

struct ComponentPairEvidence {
  std::uint32_t previousComponent = 0u;
  std::size_t overlap = 0u;
  double centreDistance = 0.0;
  double materialDistance = std::numeric_limits<double>::infinity();
};

struct AdjacentLayerEvidence {
  std::vector<std::size_t> offsets;
  std::vector<ComponentPairEvidence> pairs;

  [[nodiscard]] std::span<const ComponentPairEvidence> forCurrent(
      std::size_t currentComponent) const noexcept {
    if (currentComponent + 1u >= offsets.size()) {
      return {};
    }
    const auto first = offsets[currentComponent];
    const auto last = offsets[currentComponent + 1u];
    if (first > last || last > pairs.size()) {
      return {};
    }
    return std::span<const ComponentPairEvidence>(pairs).subspan(first, last - first);
  }
};

AdjacentLayerEvidence buildAdjacentLayerEvidence(
    const LayerDescription& previous,
    const LayerDescription& current,
    double maximumDistance) {
  AdjacentLayerEvidence evidence;
  evidence.offsets.resize(current.components.size() + 1u, 0u);
  if (previous.components.empty() || current.components.empty()) {
    return evidence;
  }

  ComponentGridIndex previousIndex(&previous, maximumDistance);
  GridQueryScratch queryScratch;
  std::vector<ComponentPairEvidence> currentPairs;
  for (std::size_t currentIndex = 0u;
       currentIndex < current.components.size(); ++currentIndex) {
    currentPairs.clear();
    const auto& component = current.components[currentIndex];
    const auto& candidates = previousIndex.query(component, queryScratch);
    currentPairs.reserve(candidates.size());
    for (const auto previousIndexValue : candidates) {
      if (previousIndexValue >= previous.components.size()) {
        continue;
      }
      const auto metrics = componentPairMetrics(
          previous.components[previousIndexValue], component, maximumDistance);
      if (!metrics.nearEnough) {
        continue;
      }
      currentPairs.push_back(ComponentPairEvidence{
          static_cast<std::uint32_t>(previousIndexValue),
          metrics.overlap,
          metrics.centreDistance,
          metrics.materialDistance,
      });
    }
    std::sort(
        currentPairs.begin(), currentPairs.end(),
        [](const auto& left, const auto& right) {
          return left.previousComponent < right.previousComponent;
        });
    evidence.pairs.insert(
        evidence.pairs.end(), currentPairs.begin(), currentPairs.end());
    evidence.offsets[currentIndex + 1u] = evidence.pairs.size();
  }
  return evidence;
}

bool hasModelRootTaper(
    std::size_t nodeId,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
  std::size_t cursor = nodeId;
  std::size_t root = nodeId;
  std::size_t maximumArea = states[nodeId].component->area;
  std::size_t observed = 0;
  while (cursor != std::numeric_limits<std::size_t>::max()) {
    root = cursor;
    maximumArea = std::max(maximumArea, states[cursor].component->area);
    cursor = states[cursor].parent;
    ++observed;
  }
  if (observed < options.minimumTrackLayers || maximumArea == 0) {
    return false;
  }
  const auto rootArea = states[root].component->area;
  return maximumArea > rootArea
         && static_cast<double>(rootArea)
                <= static_cast<double>(maximumArea) * options.modelRootTaperRatio;
}

struct TerminalTaperEvidence {
  bool tapered = false;
  bool immediate = false;
  bool reboundAfterTaper = false;
  std::size_t meaningfulDecreaseSteps = 0u;
  std::size_t maximumEarlierArea = 0u;
};

TerminalTaperEvidence terminalTaperEvidence(
    std::size_t nodeId,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
  TerminalTaperEvidence evidence;
  if (nodeId >= states.size()) {
    return evidence;
  }
  const auto finalArea = states[nodeId].component->area;
  if (finalArea == 0u) {
    return evidence;
  }

  std::vector<std::size_t> newestToOldest;
  newestToOldest.reserve(options.taperLookbackLayers);
  std::size_t cursor = nodeId;
  while (cursor != std::numeric_limits<std::size_t>::max()
         && newestToOldest.size() < options.taperLookbackLayers) {
    newestToOldest.push_back(states[cursor].component->area);
    cursor = states[cursor].parent;
  }
  if (newestToOldest.size() < 2u) {
    return evidence;
  }

  evidence.maximumEarlierArea = *std::max_element(
      newestToOldest.begin(), newestToOldest.end());
  evidence.immediate = static_cast<double>(newestToOldest[0])
      <= static_cast<double>(newestToOldest[1]) * options.terminalTaperRatio;

  bool taperObservedChronologically = false;
  for (std::size_t reverse = newestToOldest.size() - 1u; reverse > 0u; --reverse) {
    const auto older = newestToOldest[reverse];
    const auto newer = newestToOldest[reverse - 1u];
    if (older != 0u
        && static_cast<double>(newer)
               <= static_cast<double>(older) * options.terminalTaperStepRatio) {
      ++evidence.meaningfulDecreaseSteps;
      taperObservedChronologically = true;
      continue;
    }
    if (taperObservedChronologically && older != 0u
        && static_cast<double>(newer)
               >= static_cast<double>(older) / options.terminalTaperRatio) {
      evidence.reboundAfterTaper = true;
    }
  }

  const bool relativeReduction = evidence.maximumEarlierArea > finalArea
      && static_cast<double>(finalArea)
             <= static_cast<double>(evidence.maximumEarlierArea)
                    * options.terminalTaperRatio;
  evidence.tapered = relativeReduction
      && (evidence.immediate
          || evidence.reboundAfterTaper
          || evidence.meaningfulDecreaseSteps
                 >= options.minimumTerminalTaperSteps);
  return evidence;
}

bool hasTerminalTaper(
    std::size_t nodeId,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
  return terminalTaperEvidence(nodeId, states, options).tapered;
}

// Pass 1 does not need to decide that the upper component is model. It only
// needs to know whether a particular support provenance may continue through
// the edge. A genuine terminal region has a recent decreasing history and the
// following layer rebounds away from that minimum. This predicate is used only
// to cut one support provenance when an independent reverse model lineage later
// reaches the same region; it is not a standalone semantic classifier.
bool supportEvidenceStopsBefore(
    std::size_t lowerNode,
    std::size_t upperArea,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
  if (lowerNode >= states.size() || states[lowerNode].component->area == 0u) {
    return false;
  }
  const auto structuralParent = states[lowerNode].parent;
  if (structuralParent != std::numeric_limits<std::size_t>::max()
      && structuralParent < states.size()
      && states[structuralParent].component->area != 0u
      && static_cast<double>(states[lowerNode].component->area)
             > static_cast<double>(states[structuralParent].component->area)
                   / options.terminalTaperStepRatio) {
    // The lower node is already on the expanding side of a previous minimum;
    // it cannot itself be the terminal support tip for this edge.
    return false;
  }
  const auto evidence = terminalTaperEvidence(lowerNode, states, options);
  const bool recentReduction = evidence.tapered
      || evidence.immediate
      || evidence.meaningfulDecreaseSteps >= options.minimumTerminalTaperSteps;
  if (!recentReduction) {
    return false;
  }
  const double reboundRatio = static_cast<double>(upperArea)
      / static_cast<double>(states[lowerNode].component->area);
  const double minimumRebound = options.terminalTaperStepRatio > 0.0
      ? 1.0 / options.terminalTaperStepRatio
      : 1.0;
  return reboundRatio >= minimumRebound;
}

std::size_t recentMaximumArea(
    std::size_t nodeId,
    const std::vector<NodeState>& states,
    std::size_t lookbackLayers) {
  std::size_t maximumArea = 0;
  std::size_t observed = 0;
  while (nodeId != std::numeric_limits<std::size_t>::max()
         && observed < lookbackLayers) {
    maximumArea = std::max(maximumArea, states[nodeId].component->area);
    nodeId = states[nodeId].parent;
    ++observed;
  }
  return maximumArea;
}

std::vector<std::size_t> acceptPath(
    std::size_t nodeId,
    std::vector<NodeState>& states) {
  std::vector<std::size_t> acceptedNodes;
  while (nodeId != std::numeric_limits<std::size_t>::max()) {
    if (states[nodeId].classifiedAsModel || states[nodeId].accepted) {
      break;
    }
    states[nodeId].accepted = true;
    acceptedNodes.push_back(nodeId);
    nodeId = states[nodeId].parent;
  }
  return acceptedNodes;
}

double branchDriftPixelsPerLayer(
    std::size_t nodeId,
    const std::vector<NodeState>& states) {
  const auto& upper = states[nodeId];
  std::size_t lowerId = upper.branchOrigin;
  if (lowerId == std::numeric_limits<std::size_t>::max()) {
    lowerId = nodeId;
    std::size_t steps = 0;
    while (states[lowerId].parent != std::numeric_limits<std::size_t>::max()
           && steps < 8u) {
      lowerId = states[lowerId].parent;
      ++steps;
    }
  }
  const auto& lower = states[lowerId];
  const auto zLayers = upper.depth > lower.depth ? upper.depth - lower.depth : 0u;
  if (zLayers == 0) {
    return 0.0;
  }
  return centreDistancePixels(*lower.component, *upper.component)
         / static_cast<double>(zLayers);
}


} // namespace

SupportAnalysisResult SupportAnalyzer::analyze(
    photons::LayerMaskSource& source,
    const SupportAnalysisOptions& options,
    const SupportAnalysisCallbacks& callbacks) const {
  SupportAnalysisResult result;
  if (source.layerCount() == 0 || source.width() == 0 || source.height() == 0) {
    result.error = "support analysis source is empty";
    return result;
  }
  if (!(options.pitchXMillimetres > 0.0)
      || !(options.pitchYMillimetres > 0.0)
      || !(options.pitchZMillimetres > 0.0)
      || options.minimumTrackLayers == 0
      || options.modelContactConfirmationLayers < 2u
      || options.workerCount < kMinimumSupportAnalysisWorkerCount
      || options.workerCount > kMaximumSupportAnalysisWorkerCount
      || options.preparationMemoryBudgetBytes == 0u
      || !(options.raftMaximumChangedPixelRatio >= 0.0
           && options.raftMaximumChangedPixelRatio < 1.0)
      || !(options.maximumLayerMotionPixels >= 0.0)
      || !(options.minimumSupportShapeOverlapRatio > 0.0
           && options.minimumSupportShapeOverlapRatio <= 1.0)
      || !(options.braceMinimumDriftPixelsPerLayer >= 0.0)
      || !(options.braceMaximumDriftPixelsPerLayer
           >= options.braceMinimumDriftPixelsPerLayer)
      || !(options.minimumModelExpansionRatio > 1.0)
      || !(options.abruptModelExpansionRatio > 1.0)
      || !(options.abruptModelExpansionRatio
           >= options.minimumModelExpansionRatio)
      || !(options.terminalTaperRatio > 0.0 && options.terminalTaperRatio <= 1.0)
      || !(options.modelRootTaperRatio > 0.0 && options.modelRootTaperRatio <= 1.0)) {
    result.error = "support analysis options are invalid";
    return result;
  }

  result.summary.vulkanComputeCompiled = compute::vulkanSupportComputeCompiled();
  std::string computeDiagnostic;
  auto computeBackend = compute::createSupportComputeBackend(
      options.computePreference, computeDiagnostic);
  result.summary.vulkanComputeActive = computeBackend != nullptr;
  if (computeBackend) {
    result.summary.vulkanDeviceName = computeBackend->deviceName();
  }
  if (callbacks.computeStatus) {
    callbacks.computeStatus(
        result.summary.vulkanComputeCompiled,
        result.summary.vulkanComputeActive,
        computeBackend ? computeBackend->name() : "cpu",
        computeBackend ? computeBackend->deviceName() : "",
        computeDiagnostic);
  }
  const auto publishComputeTelemetry = [&] {
    if (computeBackend && callbacks.computeTelemetry) {
      callbacks.computeTelemetry(computeBackend->telemetry());
    }
  };
  result.layers.resize(source.layerCount());
  std::vector<NodeState> states;
  std::vector<std::size_t> nodeDecisionIndices;
  const auto invalidDecision = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> previousCandidateNodes;
  // P1 optimization: connected-component descriptions are produced once by
  // the forward preparation window and retained for the descending semantic
  // pass. Material masks remain bounded to the preparation window/raft check.
  std::vector<LayerDescription> layerDescriptions(source.layerCount());
  const LayerDescription* previousLayer = nullptr;
  std::vector<bool> previousModelComponents;
  std::optional<BinaryMask> firstRaftMask;
  std::size_t firstRaftArea = 0u;
  bool raftEnded = false;
  bool modelSeen = false;
  SparseRunMask previousSemanticModel(source.height(), options.enableBitsetAcceleration);
  SparseRunMask previousPreviousStableSemanticModel(source.height(), options.enableBitsetAcceleration);
  SparseRunMask previousStableSemanticModel(source.height(), options.enableBitsetAcceleration);
  SparseRunMask previousStableModelEnvelope(source.height(), options.enableBitsetAcceleration);
  SparseRunMask currentSemanticModel(source.height(), options.enableBitsetAcceleration);
  SparseRunMask confirmedSemanticModel(source.height(), options.enableBitsetAcceleration);
  SparseRunMask propagatedSemanticModel(source.height(), options.enableBitsetAcceleration);
  SparseRunMask currentStableSemanticModel(source.height(), options.enableBitsetAcceleration);
  SparseRunMask stableSemanticIntersection(source.height(), options.enableBitsetAcceleration);
  SparseRunMask stableSemanticUnion(source.height(), options.enableBitsetAcceleration);
  const auto maximumModelLineageShift = static_cast<std::uint32_t>(
      std::max(0.0, std::ceil(options.maximumLayerMotionPixels)));

  struct ForwardComponentPreparation {
    std::vector<Match> matches;
    SupportDecisionTrace decisionTrace;
    bool overlapsPreviousModel = false;
    bool nearPreviousModel = false;
    bool isModel = false;
    bool isSupportCandidate = false;
    bool rootedInModel = false;
    bool unparentedRaftSupport = false;
    bool taperedExpansion = false;
    SupportDecisionReason decisionReason = SupportDecisionReason::UnrelatedAfterModel;
    std::vector<std::size_t> supportEvidenceParents;
    std::vector<SemanticRun> supportEvidenceRuns;
    bool supportEvidenceActive = false;
  };
  struct ReverseComponentPreparation {
    std::size_t nodeId = std::numeric_limits<std::size_t>::max();
    bool hasSupportEvidence = false;
    bool protectSupportEvidence = false;
    ModelLineageMotion modelLineage;
    bool modelSeed = false;
    std::vector<SemanticRun> modelRuns;
    std::vector<SemanticRun> finalSupportRuns;
    std::size_t supportCorePixels = 0u;
    std::size_t modelPixels = 0u;
  };
  struct ForwardLineagePreparation {
    bool wholeComponentModel = false;
    bool confirmedWholeComponentModel = false;
    bool hasSemanticProjection = false;
    ModelLineageMotion modelLineage;
    std::vector<SemanticRun> modelRuns;
    std::vector<SemanticRun> projectedSupportRuns;
  };
  struct ForwardWorkerScratch {
    ForwardWorkerScratch(std::uint32_t height, bool enableBitsets)
        : parentSupport(height, enableBitsets) {}
    SparseRunMask parentSupport;
    TranslatedOverlapScratch computeOverlap;
  };
  struct ReverseWorkerScratch {
    ReverseWorkerScratch(std::uint32_t height, bool enableBitsets)
        : supportCore(height, enableBitsets),
          componentModel(height, enableBitsets),
          inheritedModel(height, enableBitsets) {}
    SparseRunMask supportCore;
    SparseRunMask componentModel;
    SparseRunMask inheritedModel;
    TranslatedOverlapScratch computeOverlap;
    std::vector<SemanticRun> inheritedRuns;
    std::vector<SemanticRun> inheritedOutsideSupport;
  };
  struct ForwardLineageWorkerScratch {
    ForwardLineageWorkerScratch(std::uint32_t height, bool enableBitsets)
        : predictedSupport(height, enableBitsets),
          parentSupport(height, enableBitsets),
          selectedModel(height, enableBitsets) {}
    SparseRunMask predictedSupport;
    SparseRunMask parentSupport;
    SparseRunMask selectedModel;
    TranslatedOverlapScratch computeOverlap;
    std::vector<SemanticRun> exactModelRuns;
    std::vector<SemanticRun> translatedModelRuns;
  };


  SupportPerformanceCounters performanceCounters;
  SupportWorkScheduler workScheduler(options.workerCount);
  // P6.5 exposes three monotonic analysis phases to the viewer: immutable
  // geometry/evidence preparation, forward reconciliation and reverse
  // reconciliation. This avoids an apparent progress stall while all native
  // LayerDescriptions are prepared before semantic commits begin.
  const std::size_t supportProgressPhaseWork = source.layerCount();
  const std::size_t supportProgressTotal = supportProgressPhaseWork * 3u;
  const auto publishPerformanceTelemetry = [&] {
    if (callbacks.performanceTelemetry) {
      callbacks.performanceTelemetry(performanceCounters.snapshot());
    }
  };
  std::vector<ForwardWorkerScratch> forwardScratch;
  forwardScratch.reserve(workScheduler.workerCount());
  std::vector<ReverseWorkerScratch> reverseScratch;
  reverseScratch.reserve(workScheduler.workerCount());
  std::vector<ForwardLineageWorkerScratch> forwardLineageScratch;
  forwardLineageScratch.reserve(workScheduler.workerCount());
  for (std::size_t worker = 0u; worker < workScheduler.workerCount(); ++worker) {
    forwardScratch.emplace_back(source.height(), options.enableBitsetAcceleration);
    reverseScratch.emplace_back(source.height(), options.enableBitsetAcceleration);
    forwardLineageScratch.emplace_back(source.height(), options.enableBitsetAcceleration);
  }
  ResidentReferenceScratch forwardResidentReference;
  ResidentReferenceScratch reverseResidentReference;
  std::uint64_t residentReferenceGeneration = 1u;

  OrderedLayerPreparer forwardPreparer(
      source,
      0u,
      source.layerCount(),
      false,
      true,
      workScheduler,
      options.preparationMemoryBudgetBytes,
      performanceCounters);
  std::vector<std::size_t> currentCandidateNodes;
  std::vector<std::size_t> currentComponentNodes;
  std::vector<std::size_t> previousComponentNodes;
  std::vector<bool> currentIsModel;
  std::vector<bool> previousHasStructuralChild;
  std::vector<ForwardComponentPreparation> currentPreparations;
  std::vector<ForwardLineagePreparation> currentLineagePreparations;

  // P6.5 separates immutable layer geometry from semantic reconciliation. All
  // native layers are decoded/described once before classification, while the
  // adaptive P6.3 window still bounds resident masks. Only the raft prefix
  // needs raw masks; once its first differing layer is known retained masks are
  // dropped immediately.
  for (std::size_t layer = 0u; layer < source.layerCount(); ++layer) {
    if (callbacks.isCancelled && callbacks.isCancelled()) {
      result.cancelled = true;
      result.error = "support analysis cancelled";
      return result;
    }
    auto prepared = forwardPreparer.next();
    if (!prepared.ok || (!raftEnded && !prepared.mask)) {
      result.error = prepared.error.empty()
          ? "support analysis could not load a layer"
          : std::move(prepared.error);
      return result;
    }
    auto mask = std::move(prepared.mask);
    layerDescriptions[layer] = std::move(prepared.description);
    auto& description = layerDescriptions[layer];
    result.summary.componentCount += description.components.size();

    if (layer == 0u) {
      if (description.totalArea == 0u) {
        result.error = "support analysis requires raft matter on the first layer";
        return result;
      }
      firstRaftMask = *mask;
      firstRaftArea = description.totalArea;
    }

    if (!raftEnded) {
      const auto maximumChangedPixels = static_cast<std::size_t>(std::ceil(
          static_cast<double>(firstRaftArea)
          * options.raftMaximumChangedPixelRatio));
      const bool sameAsFirstRaft = layer == 0u
          || (firstRaftMask
              && changedPixelCount(*mask, *firstRaftMask) <= maximumChangedPixels);
      if (sameAsFirstRaft) {
        result.summary.raftLastLayer = layer;
        result.layers[layer].layer = layer;
        result.layers[layer].phase = PrintPhase::Raft;
        for (const auto& component : description.components) {
          result.summary.raftRunCount += component.runs.size();
          if (options.captureDecisionTrace) {
            SupportDecisionTrace trace;
            trace.layer = layer;
            trace.componentId = component.localId;
            trace.currentAreaPixels = component.area;
            trace.minX = component.minX;
            trace.minY = component.minY;
            trace.maxX = component.maxX;
            trace.maxY = component.maxY;
            trace.accepted = true;
            trace.decision = MaterialSemantic::Raft;
            trace.reason = SupportDecisionReason::RaftPrefix;
            result.decisions.push_back(trace);
          }
        }
        if (callbacks.progress) {
          publishComputeTelemetry();
          publishPerformanceTelemetry();
          callbacks.progress(layer + 1u, supportProgressTotal);
        }
        continue;
      }
      raftEnded = true;
      forwardPreparer.dropRetainedMasks();
      firstRaftMask.reset();
    }
    if (callbacks.progress) {
      publishComputeTelemetry();
      publishPerformanceTelemetry();
      callbacks.progress(layer + 1u, supportProgressTotal);
    }
  }

  // Build one immutable geometric graph between adjacent native layers. Each
  // contiguous lot is independent, including its boundary pair, so all lots
  // can run simultaneously. The ordered forward/reverse passes below become a
  // reconciliation engine over these facts instead of recomputing pair metrics
  // while waiting on the previous semantic commit.
  std::vector<AdjacentLayerEvidence> adjacentEvidence(source.layerCount());
  const auto firstSemanticLayer = std::min(
      source.layerCount(), result.summary.raftLastLayer + 1u);
  const auto firstEvidenceLayer = std::min(
      source.layerCount(), firstSemanticLayer + 1u);
  const auto evidenceLayerPairCount = source.layerCount() > firstEvidenceLayer
      ? source.layerCount() - firstEvidenceLayer
      : 0u;
  const auto evidenceLotCount = evidenceLayerPairCount == 0u
      ? 0u
      : std::min(workScheduler.workerCount(), evidenceLayerPairCount);
  performanceCounters.semanticEvidenceLotCount = evidenceLotCount;
  performanceCounters.semanticEvidenceLayerPairCount = evidenceLayerPairCount;
  if (evidenceLotCount != 0u) {
    ScopedAtomicMicrosecondTimer evidenceTimer(
        performanceCounters.semanticEvidenceMicroseconds);
    workScheduler.parallelFor(
        evidenceLotCount,
        [&](std::size_t lot, std::size_t) {
          const auto beginOrdinal = evidenceLayerPairCount * lot / evidenceLotCount;
          const auto endOrdinal = evidenceLayerPairCount * (lot + 1u) / evidenceLotCount;
          std::size_t localEdgeCount = 0u;
          for (auto ordinal = beginOrdinal; ordinal < endOrdinal; ++ordinal) {
            const auto layer = firstEvidenceLayer + ordinal;
            adjacentEvidence[layer] = buildAdjacentLayerEvidence(
                layerDescriptions[layer - 1u],
                layerDescriptions[layer],
                options.maximumLayerMotionPixels);
            localEdgeCount += adjacentEvidence[layer].pairs.size();
          }
          performanceCounters.semanticEvidenceEdgeCount.fetch_add(
              localEdgeCount, std::memory_order_relaxed);
        });
  }
  if (callbacks.isCancelled && callbacks.isCancelled()) {
    result.cancelled = true;
    result.error = "support analysis cancelled";
    return result;
  }

  const auto invalidNode = std::numeric_limits<std::size_t>::max();
  if (firstSemanticLayer != 0u && firstSemanticLayer <= source.layerCount()) {
    previousLayer = &layerDescriptions[firstSemanticLayer - 1u];
    previousModelComponents.assign(previousLayer->components.size(), false);
    previousComponentNodes.assign(previousLayer->components.size(), invalidNode);
  }

  for (std::size_t layer = firstSemanticLayer;
       layer < source.layerCount(); ++layer) {
    if (callbacks.isCancelled && callbacks.isCancelled()) {
      result.cancelled = true;
      result.error = "support analysis cancelled";
      return result;
    }
    ScopedAtomicMicrosecondTimer forwardSemanticTimer(
        performanceCounters.forwardSemanticMicroseconds);
    auto& current = layerDescriptions[layer];
    result.layers[layer].layer = layer;
    const bool modelExistedBeforeLayer = modelSeen;
    const bool firstSupportLayer = layer == firstSemanticLayer;

    currentCandidateNodes.clear();
    if (currentCandidateNodes.capacity() < current.components.size()) {
      currentCandidateNodes.reserve(current.components.size());
    }
    currentComponentNodes.assign(current.components.size(), invalidNode);
    currentIsModel.assign(current.components.size(), false);
    std::unordered_map<std::size_t, std::size_t> previousNodeLocalIndex;
    previousNodeLocalIndex.reserve(previousCandidateNodes.size());
    previousHasStructuralChild.assign(previousCandidateNodes.size(), false);
    std::size_t maximumPreviousSupportArea = 0u;
    for (std::size_t local = 0u; local < previousCandidateNodes.size(); ++local) {
      const auto nodeId = previousCandidateNodes[local];
      previousNodeLocalIndex.emplace(nodeId, local);
      maximumPreviousSupportArea = std::max(
          maximumPreviousSupportArea, states[nodeId].component->area);
    }
    previousStableModelEnvelope.assignDilated(
        previousStableSemanticModel, maximumModelLineageShift);

    // Stabilisation: exact and envelope zero-shift semantic overlaps stay on
    // the canonical sparse CPU masks in both standard modes. Vulkan remains an
    // opportunistic accelerator only for translated lineage kernels.

    currentPreparations.resize(current.components.size());
    {
      ScopedAtomicMicrosecondTimer classificationTimer(
          performanceCounters.forwardClassificationMicroseconds);
      workScheduler.parallelFor(
        current.components.size(),
        [&](std::size_t index, std::size_t workerSlot) {
          auto& preparation = currentPreparations[index];
          preparation.matches.clear();
          preparation.decisionTrace = SupportDecisionTrace{};
          preparation.overlapsPreviousModel = false;
          preparation.nearPreviousModel = false;
          preparation.isModel = false;
          preparation.isSupportCandidate = false;
          preparation.rootedInModel = false;
          preparation.unparentedRaftSupport = false;
          preparation.taperedExpansion = false;
          preparation.decisionReason = SupportDecisionReason::UnrelatedAfterModel;
          preparation.supportEvidenceParents.clear();
          preparation.supportEvidenceRuns.clear();
          preparation.supportEvidenceActive = false;
          const auto& component = current.components[index];
          auto& workerScratch = forwardScratch[workerSlot];
          auto& matches = preparation.matches;
          matches.clear();
          const auto pairEvidence = adjacentEvidence[layer].forCurrent(index);
          if (matches.capacity() < pairEvidence.size()) {
            matches.reserve(pairEvidence.size());
          }
          for (const auto& pair : pairEvidence) {
            const auto previousComponent =
                static_cast<std::size_t>(pair.previousComponent);
            if (previousComponent >= previousComponentNodes.size()) {
              continue;
            }
            const auto previousNode = previousComponentNodes[previousComponent];
            if (previousNode == invalidNode || previousNode >= states.size()) {
              continue;
            }
            matches.push_back(Match{
                previousNode,
                pair.overlap,
                pair.centreDistance,
                pair.materialDistance,
            });
          }
          std::sort(matches.begin(), matches.end(), [](const Match& left, const Match& right) {
            if (left.overlap != right.overlap) {
              return left.overlap > right.overlap;
            }
            if (left.materialDistance != right.materialDistance) {
              return left.materialDistance < right.materialDistance;
            }
            if (left.centreDistance != right.centreDistance) {
              return left.centreDistance < right.centreDistance;
            }
            return left.previousNode < right.previousNode;
          });

          auto& decisionTrace = preparation.decisionTrace;
          decisionTrace.layer = layer;
          decisionTrace.componentId = component.localId;
          decisionTrace.currentAreaPixels = component.area;
          decisionTrace.minX = component.minX;
          decisionTrace.minY = component.minY;
          decisionTrace.maxX = component.maxX;
          decisionTrace.maxY = component.maxY;
          if (!matches.empty()) {
            decisionTrace.parentNodeId = matches.front().previousNode;
            decisionTrace.parentAreaPixels =
                states[matches.front().previousNode].component->area;
            decisionTrace.overlapPixels = matches.front().overlap;
            decisionTrace.centreDistancePixels = matches.front().centreDistance;
            decisionTrace.materialDistancePixels = matches.front().materialDistance;
            if (decisionTrace.parentAreaPixels != 0u) {
              decisionTrace.parentAreaRatio =
                  static_cast<double>(decisionTrace.currentAreaPixels)
                  / static_cast<double>(decisionTrace.parentAreaPixels);
              decisionTrace.primaryParentCoverageRatio =
                  static_cast<double>(decisionTrace.overlapPixels)
                  / static_cast<double>(decisionTrace.parentAreaPixels);
            }
          }

          const auto exactStableModelOverlap =
              previousStableSemanticModel.countSet(component.runs);
          const auto nearbyStableModelOverlap =
              previousStableModelEnvelope.countSet(component.runs);
          bool overlapsPreviousModel = exactStableModelOverlap != 0u;
          bool nearPreviousModel = nearbyStableModelOverlap != 0u;
          for (const auto& pair : pairEvidence) {
            const auto previousComponent =
                static_cast<std::size_t>(pair.previousComponent);
            if (previousComponent >= previousModelComponents.size()
                || !previousModelComponents[previousComponent]) {
              continue;
            }
            nearPreviousModel = true;
            overlapsPreviousModel = overlapsPreviousModel || pair.overlap != 0u;
          }

          std::size_t preservedSupportParentCount = 0u;
          std::size_t preservedSupportPixels = 0u;
          for (const auto& match : matches) {
            const auto parentArea = states[match.previousNode].component->area;
            if (parentArea == 0u || match.overlap == 0u) {
              continue;
            }
            const double parentCoverage = static_cast<double>(match.overlap)
                                          / static_cast<double>(parentArea);
            if (parentCoverage >= options.minimumSupportParentCoverageRatio) {
              ++preservedSupportParentCount;
              preservedSupportPixels += match.overlap;
            }
          }
          const double supportFusionCoverage = component.area == 0u
              ? 0.0
              : static_cast<double>(std::min(component.area, preservedSupportPixels))
                    / static_cast<double>(component.area);
          const bool supportFusionContinuation =
              preservedSupportParentCount >= 2u
              && supportFusionCoverage >= options.minimumSupportFusionCoverageRatio;

          SupportMotionComparison supportMotion;
          TerminalTaperEvidence parentTaperEvidence;
          bool terminalTaperOnParent = false;
          bool supportMotionContinuation = false;
          bool taperedExpansion = false;
          if (!matches.empty()) {
            const auto& match = matches.front();
            const auto& previousState = states[match.previousNode];
            if (previousState.component->area != 0u
                && component.area > previousState.component->area) {
              const double expansion = static_cast<double>(component.area)
                                       / static_cast<double>(previousState.component->area);
              const bool rootedOnlyInModel =
                  result.nodes[match.previousNode].rootedInModel
                  && !result.nodes[match.previousNode].rootedInRaft;
              const bool validModelRoot = !rootedOnlyInModel
                                          || hasModelRootTaper(
                                              match.previousNode, states, options);
              parentTaperEvidence = terminalTaperEvidence(
                  match.previousNode, states, options);
              terminalTaperOnParent =
                  previousState.depth >= options.minimumTrackLayers
                  && validModelRoot
                  && parentTaperEvidence.tapered;
              if (terminalTaperOnParent && !supportFusionContinuation) {
                supportMotion = compareSupportMotion(
                    match.previousNode, component, states, options);
                const bool displacedGrowth = expansion > 1.0
                    && centreDistancePixels(*previousState.component, component)
                           > options.maximumLayerMotionPixels;
                const bool unexplainedDisplacedGrowth = displacedGrowth
                    && !supportMotion.explainedBySupportMotion;
                const bool growthMayOpenContact =
                    expansion >= options.minimumModelExpansionRatio
                    || unexplainedDisplacedGrowth;
                taperedExpansion = growthMayOpenContact;
                supportMotionContinuation = displacedGrowth
                    && !growthMayOpenContact
                    && supportMotion.explainedBySupportMotion;
              }
            }
          }

          std::size_t supportReferenceArea = 0u;
          if (!matches.empty()) {
            supportReferenceArea = recentMaximumArea(
                matches.front().previousNode, states, options.taperLookbackLayers);
          }
          const bool plausibleSupportContinuation = !matches.empty()
              && (taperedExpansion
                  || (supportReferenceArea != 0u
                      && static_cast<double>(component.area)
                             <= static_cast<double>(supportReferenceArea)
                                    * options.abruptModelExpansionRatio));
          const bool modelDominantMerge = overlapsPreviousModel
              && !plausibleSupportContinuation;
          bool isModel = modelDominantMerge;
          SupportDecisionReason decisionReason = modelDominantMerge
              ? SupportDecisionReason::ModelDominantMerge
              : SupportDecisionReason::UnrelatedAfterModel;

          bool rootedInModel = false;
          bool isSupportCandidate = false;
          bool unparentedRaftSupport = false;
          if (!isModel) {
            if (!matches.empty() || firstSupportLayer) {
              isSupportCandidate = true;
              unparentedRaftSupport = firstSupportLayer && matches.empty();
              decisionReason = unparentedRaftSupport
                  ? SupportDecisionReason::FirstSupportLayer
                  : (supportFusionContinuation
                         ? SupportDecisionReason::SupportFusionContinuation
                         : (supportMotionContinuation
                                ? SupportDecisionReason::SupportMotionContinuation
                                : SupportDecisionReason::SupportContinuation));
            } else if (modelExistedBeforeLayer && nearPreviousModel) {
              isSupportCandidate = true;
              rootedInModel = true;
              decisionReason = SupportDecisionReason::ModelRootCandidate;
            } else if (!modelExistedBeforeLayer) {
              const bool relativeModelExpansion = maximumPreviousSupportArea != 0u
                  && static_cast<double>(component.area)
                         >= static_cast<double>(maximumPreviousSupportArea)
                                * options.abruptModelExpansionRatio;
              if (relativeModelExpansion) {
                isModel = true;
                decisionReason =
                    SupportDecisionReason::RelativeExpansionBeforeFirstModel;
              } else {
                isSupportCandidate = true;
                unparentedRaftSupport = true;
                decisionReason = SupportDecisionReason::SupportBornBeforeModel;
              }
            } else {
              isModel = true;
            }
          }

          decisionTrace.matchedSupportParentCount = matches.size();
          decisionTrace.preservedSupportParentCount = preservedSupportParentCount;
          decisionTrace.supportFusionCoverageRatio = supportFusionCoverage;
          decisionTrace.supportFusionContinuation = supportFusionContinuation;
          decisionTrace.terminalTaperDecreaseSteps =
              parentTaperEvidence.meaningfulDecreaseSteps;
          decisionTrace.immediateTerminalTaperOnParent = parentTaperEvidence.immediate;
          decisionTrace.terminalTaperReboundOnParent =
              parentTaperEvidence.reboundAfterTaper;
          if (options.captureDecisionTrace) {
            decisionTrace.matchedSupportParentNodeIds.reserve(matches.size());
            decisionTrace.matchedSupportParentOverlapPixels.reserve(matches.size());
            for (const auto& match : matches) {
              decisionTrace.matchedSupportParentNodeIds.push_back(match.previousNode);
              decisionTrace.matchedSupportParentOverlapPixels.push_back(match.overlap);
            }
          }
          decisionTrace.overlapsPreviousModel = overlapsPreviousModel;
          decisionTrace.nearPreviousModel = nearPreviousModel;
          decisionTrace.alignedOverlapPixels = supportMotion.alignedOverlapPixels;
          decisionTrace.addedPixelsAfterAlignment = supportMotion.addedPixels;
          decisionTrace.removedPixelsAfterAlignment = supportMotion.removedPixels;
          decisionTrace.predictedMotionXPixels = supportMotion.predictedMotionX;
          decisionTrace.predictedMotionYPixels = supportMotion.predictedMotionY;
          decisionTrace.motionResidualPixels = supportMotion.motionResidual;
          decisionTrace.alignedOverlapRatio = supportMotion.alignedOverlapRatio;
          decisionTrace.alignedIntersectionOverUnion =
              supportMotion.alignedIntersectionOverUnion;
          decisionTrace.terminalTaperOnParent = terminalTaperOnParent;
          decisionTrace.supportMotionContinuation = supportMotionContinuation;
          decisionTrace.recentSupportMaximumAreaPixels = supportReferenceArea;

          preparation.overlapsPreviousModel = overlapsPreviousModel;
          preparation.nearPreviousModel = nearPreviousModel;
          preparation.isModel = isModel;
          preparation.isSupportCandidate = isSupportCandidate;
          preparation.rootedInModel = rootedInModel;
          preparation.unparentedRaftSupport = unparentedRaftSupport;
          preparation.taperedExpansion = taperedExpansion;
          preparation.decisionReason = decisionReason;

          if (isModel || !isSupportCandidate) {
            return;
          }

          const bool supportEvidenceRoot = firstSupportLayer;
          if (supportEvidenceRoot) {
            preparation.supportEvidenceRuns = component.runs;
          }
          auto& parentSupportSemantic = workerScratch.parentSupport;
          for (const auto& match : matches) {
            if (match.previousNode >= states.size()
                || !states[match.previousNode].supportEvidenceActive) {
              continue;
            }
            const auto& parentEvidence = states[match.previousNode].supportEvidenceRuns;
            if (parentEvidence.empty()) {
              continue;
            }
            preparation.supportEvidenceParents.push_back(match.previousNode);
            parentSupportSemantic.clear();
            parentSupportSemantic.addRuns(parentEvidence);
            parentSupportSemantic.normalize();
            std::int64_t shiftX = 0;
            std::int64_t shiftY = 0;
            if (parentSupportSemantic.countSet(component.runs) == 0u) {
              const auto supportLineage = bestTranslatedLineageMotion(
                  component, parentSupportSemantic, nullptr,
                  maximumModelLineageShift, nullptr,
                  computeBackend.get(), &workerScratch.computeOverlap, nullptr,
                  options.vulkanMinimumComponentAreaPixels);
              if (supportLineage.continued) {
                shiftX = supportLineage.shiftX;
                shiftY = supportLineage.shiftY;
              }
            }
            parentSupportSemantic.appendTranslatedIntersectionRuns(
                component.runs, shiftX, shiftY,
                MaterialSemantic::Support, preparation.supportEvidenceRuns);
          }
          canonicalizeSemanticRuns(
              preparation.supportEvidenceRuns, MaterialSemantic::Support);
          preparation.supportEvidenceActive = supportEvidenceRoot
              || !preparation.supportEvidenceRuns.empty();
          if (preparation.supportEvidenceActive && !nearPreviousModel) {
            preparation.supportEvidenceRuns = component.runs;
            canonicalizeSemanticRuns(
                preparation.supportEvidenceRuns, MaterialSemantic::Support);
          }
        });
    }

    if (callbacks.isCancelled && callbacks.isCancelled()) {
      result.cancelled = true;
      result.error = "support analysis cancelled";
      return result;
    }

    std::vector<std::uint32_t> currentStructuralChildCount(
        previousCandidateNodes.size(), 0u);
    std::vector<double> previousBranchDrift(
        previousCandidateNodes.size(), std::numeric_limits<double>::quiet_NaN());
    {
      ScopedAtomicMicrosecondTimer commitTimer(
          performanceCounters.forwardCommitMicroseconds);
      for (std::size_t index = 0; index < current.components.size(); ++index) {
      auto& preparation = currentPreparations[index];
      auto& matches = preparation.matches;
      auto decisionTrace = std::move(preparation.decisionTrace);
      const bool isModel = preparation.isModel;
      const auto decisionReason = preparation.decisionReason;
      const bool isSupportCandidate = preparation.isSupportCandidate;
      bool rootedInModel = preparation.rootedInModel;
      const bool unparentedRaftSupport = preparation.unparentedRaftSupport;
      const bool nearPreviousModel = preparation.nearPreviousModel;
      const bool taperedExpansion = preparation.taperedExpansion;

      if (isModel) {
        currentIsModel[index] = true;
        if (options.captureDecisionTrace) {
          decisionTrace.decision = MaterialSemantic::Model;
          decisionTrace.reason = decisionReason;
          result.decisions.push_back(decisionTrace);
        }
        continue;
      }
      if (!isSupportCandidate) {
        if (options.captureDecisionTrace) {
          decisionTrace.decision = MaterialSemantic::Model;
          decisionTrace.reason = decisionReason;
          result.decisions.push_back(decisionTrace);
        }
        continue;
      }

      std::size_t parent = std::numeric_limits<std::size_t>::max();
      bool rootedInRaft = unparentedRaftSupport;
      if (!matches.empty()) {
        parent = matches.front().previousNode;
        rootedInRaft = result.nodes[parent].rootedInRaft;
        rootedInModel = result.nodes[parent].rootedInModel;
      }

      const std::size_t nodeId = states.size();
      NodeState state;
      state.nodeId = nodeId;
      state.parent = parent;
      state.depth = parent == std::numeric_limits<std::size_t>::max()
                        ? 1u
                        : states[parent].depth + 1u;
      state.branchOrigin = parent == std::numeric_limits<std::size_t>::max()
                               ? std::numeric_limits<std::size_t>::max()
                               : states[parent].branchOrigin;
      state.component = &current.components[index];
      state.runCount = current.components[index].runs.size();
      state.supportEvidenceParents = std::move(preparation.supportEvidenceParents);
      state.supportEvidenceRuns = std::move(preparation.supportEvidenceRuns);
      state.supportEvidenceActive = preparation.supportEvidenceActive;
      states.push_back(std::move(state));
      currentComponentNodes[index] = nodeId;

      SupportGraphNode node;
      node.id = nodeId;
      node.layer = layer;
      node.areaPixels = current.components[index].area;
      node.centerXMillimetres = current.components[index].centerX
                                * options.pitchXMillimetres;
      node.centerYMillimetres = current.components[index].centerY
                                * options.pitchYMillimetres;
      node.equivalentDiameterMillimetres = equivalentDiameterMillimetres(
          current.components[index], options);
      node.rootedInRaft = rootedInRaft;
      node.rootedInModel = rootedInModel;
      node.kind = parent == std::numeric_limits<std::size_t>::max()
                      ? (rootedInRaft ? SupportNodeKind::RaftRoot
                                     : SupportNodeKind::Pillar)
                      : SupportNodeKind::Pillar;
      result.nodes.push_back(node);
      std::size_t decisionIndex = invalidDecision;
      if (options.captureDecisionTrace) {
        decisionTrace.nodeId = nodeId;
        decisionTrace.parentNodeId = parent;
        decisionTrace.rootedInRaft = rootedInRaft;
        decisionTrace.rootedInModel = rootedInModel;
        decisionTrace.decision = MaterialSemantic::Support;
        decisionTrace.reason = decisionReason;
        decisionIndex = result.decisions.size();
        result.decisions.push_back(decisionTrace);
      }
      nodeDecisionIndices.push_back(decisionIndex);
      currentCandidateNodes.push_back(nodeId);
      ++result.summary.candidateNodeCount;

      if (parent != std::numeric_limits<std::size_t>::max()) {
        std::uint32_t siblingCount = 1u;
        const auto parentLocal = previousNodeLocalIndex.find(parent);
        if (parentLocal != previousNodeLocalIndex.end()) {
          previousHasStructuralChild[parentLocal->second] = true;
          siblingCount = ++currentStructuralChildCount[parentLocal->second];
        }
        const auto edgeKind = siblingCount > 1u
                                  ? SupportEdgeKind::Split
                                  : SupportEdgeKind::Continuation;
        result.edges.push_back(SupportGraphEdge{parent, nodeId, edgeKind});
        if (edgeKind == SupportEdgeKind::Split) {
          ++result.summary.splitEdgeCount;
          states[nodeId].branchOrigin = parent;
          result.nodes[nodeId].kind = SupportNodeKind::Branch;
        } else {
          ++result.summary.continuationEdgeCount;
        }

        // A current connected component can geometrically touch several prior
        // branches. Only one parent is retained. A secondary contact is kept
        // as a brace relation only when one of the two independent paths has
        // the validated layer-native lateral drift.
        const double currentDrift = matches.size() > 1u
            ? branchDriftPixelsPerLayer(nodeId, states)
            : 0.0;
        for (std::size_t matchIndex = 1; matchIndex < matches.size(); ++matchIndex) {
          const auto other = matches[matchIndex].previousNode;
          double otherDrift = 0.0;
          const auto otherLocal = previousNodeLocalIndex.find(other);
          if (otherLocal != previousNodeLocalIndex.end()) {
            auto& cachedDrift = previousBranchDrift[otherLocal->second];
            if (std::isnan(cachedDrift)) {
              cachedDrift = branchDriftPixelsPerLayer(other, states);
            }
            otherDrift = cachedDrift;
          } else {
            otherDrift = branchDriftPixelsPerLayer(other, states);
          }
          if (currentDrift >= options.braceMinimumDriftPixelsPerLayer
              && currentDrift <= options.braceMaximumDriftPixelsPerLayer) {
            states[nodeId].supportContact = true;
            states[nodeId].contactNode = other;
            result.edges.push_back(SupportGraphEdge{
                nodeId, other, SupportEdgeKind::Brace});
            ++result.summary.braceEdgeCount;
          } else if (otherDrift >= options.braceMinimumDriftPixelsPerLayer
                     && otherDrift <= options.braceMaximumDriftPixelsPerLayer) {
            states[other].supportContact = true;
            states[other].contactNode = nodeId;
            result.edges.push_back(SupportGraphEdge{
                other, nodeId, SupportEdgeKind::Brace});
            ++result.summary.braceEdgeCount;
          }
        }
      }

      // A local growth after a validated terminal taper opens a provisional
      // contact candidate. The decision is strictly local to this branch: the
      // fact that model matter exists elsewhere in the print is irrelevant.
      // The candidate stays attached to the support graph while it is checked
      // against the following native layers. It is confirmed only when the
      // candidate matter persists and grows cumulatively from its first layer.
      // A stationary or shrinking sequence is committed back to support.
      if (parent != std::numeric_limits<std::size_t>::max()) {
        auto& currentState = states[nodeId];
        const auto& parentState = states[parent];
        if (parentState.pendingContactTip != invalidNode) {
          const auto tipArea = states[parentState.pendingContactTip].component->area;
          const bool remainsAboveTip = tipArea != 0u
              && currentState.component->area >= tipArea;
          if (remainsAboveTip) {
            currentState.pendingContactTip = parentState.pendingContactTip;
            currentState.pendingContactStart = parentState.pendingContactStart;
            currentState.pendingContactLength = parentState.pendingContactLength + 1u;
            currentState.pendingContactTips = parentState.pendingContactTips;
          }
        } else if (taperedExpansion) {
          currentState.pendingContactTip = parent;
          currentState.pendingContactStart = nodeId;
          currentState.pendingContactLength = 1u;
          for (const auto& match : matches) {
            const auto& matchedState = states[match.previousNode];
            if (matchedState.component->area == 0u
                || currentState.component->area <= matchedState.component->area) {
              continue;
            }
            const bool rootedOnlyInModel =
                result.nodes[match.previousNode].rootedInModel
                && !result.nodes[match.previousNode].rootedInRaft;
            const bool validModelRoot = !rootedOnlyInModel
                || hasModelRootTaper(match.previousNode, states, options);
            if (matchedState.depth >= options.minimumTrackLayers
                && validModelRoot
                && hasTerminalTaper(match.previousNode, states, options)) {
              currentState.pendingContactTips.push_back(match.previousNode);
            }
          }
          if (currentState.pendingContactTips.empty()) {
            currentState.pendingContactTips.push_back(parent);
          }
        }

        if (options.captureDecisionTrace
            && decisionIndex != invalidDecision
            && currentState.pendingContactTip != invalidNode) {
          auto& trace = result.decisions[decisionIndex];
          trace.contactCandidate = true;
          trace.pendingContactLength = currentState.pendingContactLength;
          trace.reason = currentState.pendingContactLength == 1u
              ? SupportDecisionReason::ContactCandidateOpened
              : SupportDecisionReason::ContactCandidateContinued;
          const auto startArea =
              states[currentState.pendingContactStart].component->area;
          if (startArea != 0u) {
            trace.pendingStartAreaRatio =
                static_cast<double>(currentState.component->area)
                / static_cast<double>(startArea);
          }
        }

        const auto pendingTipArea = currentState.pendingContactTip == invalidNode
            ? 0u
            : states[currentState.pendingContactTip].component->area;
        const auto pendingStartArea = currentState.pendingContactStart == invalidNode
            ? 0u
            : states[currentState.pendingContactStart].component->area;
        const bool abruptLocalContact = pendingTipArea != 0u
            && static_cast<double>(pendingStartArea)
                   >= static_cast<double>(pendingTipArea)
                          * options.abruptModelExpansionRatio;
        const bool cumulativeGrowthConfirmed = pendingStartArea != 0u
            && static_cast<double>(currentState.component->area)
                   >= static_cast<double>(pendingStartArea)
                          * options.minimumModelExpansionRatio;
        const bool persistenceConfirmed = currentState.pendingContactLength
                                          >= options.modelContactConfirmationLayers;
        const bool localContactConfirmed = abruptLocalContact
                                           || cumulativeGrowthConfirmed;
        if (currentState.pendingContactTip != invalidNode
            && persistenceConfirmed
            && localContactConfirmed) {
          const auto startNode = currentState.pendingContactStart;
          std::size_t cursor = nodeId;
          while (cursor != invalidNode) {
            states[cursor].classifiedAsModel = true;
            states[cursor].accepted = false;
            if (options.captureDecisionTrace
                && cursor < nodeDecisionIndices.size()
                && nodeDecisionIndices[cursor] != invalidDecision) {
              auto& trace = result.decisions[nodeDecisionIndices[cursor]];
              trace.contactCandidate = true;
              trace.contactConfirmed = true;
              trace.decision = MaterialSemantic::Model;
              trace.reason = abruptLocalContact
                  ? SupportDecisionReason::ContactConfirmedAbrupt
                  : SupportDecisionReason::ContactConfirmedProgressive;
              trace.pendingContactLength = currentState.pendingContactLength;
              if (pendingStartArea != 0u) {
                trace.pendingStartAreaRatio =
                    static_cast<double>(currentState.component->area)
                    / static_cast<double>(pendingStartArea);
              }
            }
            if (cursor == startNode) {
              break;
            }
            cursor = states[cursor].parent;
          }

          for (const auto contactTip : currentState.pendingContactTips) {
            auto& tipState = states[contactTip];
            if (tipState.modelContact) {
              continue;
            }
            tipState.modelContact = true;
            tipState.contactLayer = result.nodes[startNode].layer;
            tipState.contactModelPixelCount = states[startNode].component->area;
            tipState.contactModelExpansionRatio = tipState.component->area == 0u
                ? 0.0
                : static_cast<double>(states[startNode].component->area)
                      / static_cast<double>(tipState.component->area);
            result.edges.push_back(SupportGraphEdge{
                contactTip, contactTip, SupportEdgeKind::ModelContact});
            ++result.summary.modelContactEdgeCount;
          }

          currentIsModel[index] = true;
          const auto removedParentLocal = previousNodeLocalIndex.find(parent);
          if (removedParentLocal != previousNodeLocalIndex.end()
              && currentStructuralChildCount[removedParentLocal->second] != 0u) {
            --currentStructuralChildCount[removedParentLocal->second];
          }
          if (!currentCandidateNodes.empty()
              && currentCandidateNodes.back() == nodeId) {
            currentCandidateNodes.pop_back();
          } else {
            currentCandidateNodes.erase(
                std::remove(currentCandidateNodes.begin(),
                            currentCandidateNodes.end(), nodeId),
                currentCandidateNodes.end());
          }
          const auto confirmedModelLayer = result.nodes[startNode].layer;
          if (!modelSeen || confirmedModelLayer < result.summary.firstModelLayer) {
            result.summary.firstModelLayer = confirmedModelLayer;
          }
          modelSeen = true;
        } else if (currentState.pendingContactTip != invalidNode
                   && persistenceConfirmed) {
          const bool stillGrowing = currentState.component->area
                                    > parentState.component->area;
          const bool confirmationWindowExhausted =
              currentState.pendingContactLength >= options.taperLookbackLayers;
          if (!stillGrowing || confirmationWindowExhausted) {
            // No cumulative model growth was established. Keep the complete
            // sequence as support and allow a later terminal growth to open a
            // new local candidate.
            if (options.captureDecisionTrace
                && decisionIndex != invalidDecision) {
              auto& trace = result.decisions[decisionIndex];
              trace.contactCandidate = false;
              trace.decision = MaterialSemantic::Support;
              trace.reason = SupportDecisionReason::RejectedSupportPath;
            }
            currentState.pendingContactTip = invalidNode;
            currentState.pendingContactStart = invalidNode;
            currentState.pendingContactLength = 0u;
            currentState.pendingContactTips.clear();
          }
        }
      }
      }
    }

    const bool layerContainsModel = std::any_of(
        currentIsModel.begin(), currentIsModel.end(), [](bool value) { return value; });
    if (!modelSeen && layerContainsModel) {
      modelSeen = true;
      result.summary.firstModelLayer = layer;
    }

    // A support branch with no structural continuation on this layer may end
    // against any component already classified as model. P6.4 inverts the
    // previous-node/current-component scan: record the first model component
    // matched by each previous node once, then commit in the original
    // previousCandidateNodes order. This preserves edge ordering while removing
    // the quadratic layer scan on heavily fragmented prints.
    const auto invalidContactComponent = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> firstModelContactComponent(
        previousCandidateNodes.size(), invalidContactComponent);
    for (std::size_t index = 0u; index < current.components.size(); ++index) {
      if (!currentIsModel[index]) {
        continue;
      }
      for (const auto& match : currentPreparations[index].matches) {
        const auto previousLocal = previousNodeLocalIndex.find(match.previousNode);
        if (previousLocal != previousNodeLocalIndex.end()
            && firstModelContactComponent[previousLocal->second]
                   == invalidContactComponent) {
          firstModelContactComponent[previousLocal->second] = index;
        }
      }
    }
    for (std::size_t previousLocal = 0u;
         previousLocal < previousCandidateNodes.size(); ++previousLocal) {
      const auto previousNode = previousCandidateNodes[previousLocal];
      if (previousHasStructuralChild[previousLocal]) {
        continue;
      }
      const auto index = firstModelContactComponent[previousLocal];
      if (index == invalidContactComponent) {
        continue;
      }
      const auto& previousComponent = *states[previousNode].component;
      states[previousNode].modelContact = true;
      states[previousNode].contactLayer = layer;
      states[previousNode].contactModelPixelCount = current.components[index].area;
      states[previousNode].contactModelExpansionRatio = previousComponent.area == 0u
          ? 0.0
          : static_cast<double>(current.components[index].area)
                / static_cast<double>(previousComponent.area);
      result.edges.push_back(SupportGraphEdge{
          previousNode, previousNode, SupportEdgeKind::ModelContact});
      ++result.summary.modelContactEdgeCount;
    }

    // Preserve semantic continuity independently from the whole-component
    // graph decision. P4 moves the expensive lineage search into the shared
    // scheduler as a second read-only compute stage for the current layer. All
    // mutations of graph state, diagnostics and aggregate sparse masks still
    // happen below in original component order.
    currentSemanticModel.clear();
    confirmedSemanticModel.clear();
    propagatedSemanticModel.clear();
    currentLineagePreparations.resize(current.components.size());

    // P6.2: the stable model mask is shared by every expensive lineage query on
    // this layer. Rasterise it once in the native full-layer domain and let the
    // Vulkan backend keep it resident while component sources are submitted as
    // compact runs. Support-parent queries still use their component-local
    // references and therefore retain the dense fallback path below.
    const compute::TranslatedRunOverlapBatch* forwardResidentBatch = nullptr;
    const bool forwardHasGpuCandidate = computeBackend != nullptr
        && maximumModelLineageShift != 0u
        && std::any_of(
            current.components.begin(), current.components.end(),
            [&](const Component& component) {
              return component.area >= options.vulkanMinimumComponentAreaPixels;
            });
    if (forwardHasGpuCandidate && !previousStableSemanticModel.empty()) {
      if (!forwardResidentReference.words.empty()
          && forwardResidentReference.batch.referenceKey != 0u
          && forwardResidentReference.batch.width == source.width()
          && forwardResidentReference.batch.height == source.height()) {
        forwardResidentReference.batch.radius = maximumModelLineageShift;
        forwardResidentBatch = &forwardResidentReference.batch;
      } else {
        const auto preparationStarted = std::chrono::steady_clock::now();
        if (prepareResidentReferenceBatch(
                previousStableSemanticModel, source.width(), source.height(),
                maximumModelLineageShift, residentReferenceGeneration++,
                forwardResidentReference)) {
          const auto preparationEnded = std::chrono::steady_clock::now();
          computeBackend->recordHostPreparation(static_cast<std::uint64_t>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  preparationEnded - preparationStarted).count()));
          forwardResidentBatch = &forwardResidentReference.batch;
        }
      }
    }

    {
      ScopedAtomicMicrosecondTimer lineageTimer(
          performanceCounters.forwardLineageMicroseconds);
      workScheduler.parallelFor(
        current.components.size(),
        [&](std::size_t index, std::size_t workerSlot) {
          const auto& component = current.components[index];
          const auto nodeId = currentComponentNodes[index];
          auto& preparation = currentLineagePreparations[index];
          preparation.wholeComponentModel = false;
          preparation.confirmedWholeComponentModel = false;
          preparation.hasSemanticProjection = false;
          preparation.modelLineage = ModelLineageMotion{};
          preparation.modelRuns.clear();
          preparation.projectedSupportRuns.clear();

          if (currentIsModel[index]) {
            preparation.wholeComponentModel = true;
            preparation.confirmedWholeComponentModel =
                nodeId != invalidNode && states[nodeId].classifiedAsModel;
            return;
          }
          if (nodeId == invalidNode) {
            preparation.wholeComponentModel = modelSeen;
            return;
          }

          const auto lineageEnvelopeOverlap =
              previousStableModelEnvelope.countSet(component.runs);
          if (lineageEnvelopeOverlap == 0u) {
            return;
          }
          auto& scratch = forwardLineageScratch[workerSlot];
          preparation.modelLineage = bestTranslatedLineageMotion(
              component, previousStableSemanticModel,
              &previousPreviousStableSemanticModel,
              maximumModelLineageShift, nullptr,
              computeBackend.get(), &scratch.computeOverlap,
              forwardResidentBatch,
              options.vulkanMinimumComponentAreaPixels);
          if (!preparation.modelLineage.continued) {
            return;
          }

          scratch.exactModelRuns.clear();
          scratch.translatedModelRuns.clear();
          previousStableSemanticModel.appendTranslatedIntersectionRuns(
              component.runs,
              0,
              0,
              MaterialSemantic::Model,
              scratch.exactModelRuns);
          previousStableSemanticModel.appendTranslatedIntersectionRuns(
              component.runs,
              preparation.modelLineage.shiftX,
              preparation.modelLineage.shiftY,
              MaterialSemantic::Model,
              scratch.translatedModelRuns);

          scratch.predictedSupport.clear();
          const auto& semanticMatches = currentPreparations[index].matches;
          for (const auto& match : semanticMatches) {
            const auto& parentState = states[match.previousNode];
            const auto& supportRuns = parentState.hasSemanticProjection
                ? parentState.projectedSupportRuns
                : parentState.component->runs;
            scratch.parentSupport.clear();
            scratch.parentSupport.addRuns(supportRuns);
            scratch.parentSupport.normalize();
            const auto supportLineage = bestTranslatedLineageMotion(
                component, scratch.parentSupport, nullptr,
                maximumModelLineageShift, nullptr,
                computeBackend.get(), &scratch.computeOverlap, nullptr,
                options.vulkanMinimumComponentAreaPixels);
            const auto supportShiftX = supportLineage.continued
                ? supportLineage.shiftX
                : 0;
            const auto supportShiftY = supportLineage.continued
                ? supportLineage.shiftY
                : 0;
            scratch.predictedSupport.addShiftedRuns(
                supportRuns, supportShiftX, supportShiftY);
          }
          scratch.predictedSupport.normalize();
          scratch.predictedSupport.appendClearRuns(
              scratch.translatedModelRuns,
              MaterialSemantic::Model,
              preparation.modelRuns);
          preparation.modelRuns.insert(
              preparation.modelRuns.end(),
              scratch.exactModelRuns.begin(),
              scratch.exactModelRuns.end());
          canonicalizeSemanticRuns(
              preparation.modelRuns, MaterialSemantic::Model);
          if (preparation.modelRuns.empty()) {
            return;
          }

          scratch.selectedModel.clear();
          scratch.selectedModel.addRuns(preparation.modelRuns);
          scratch.selectedModel.normalize();
          scratch.selectedModel.appendClearRuns(
              component.runs,
              MaterialSemantic::Support,
              preparation.projectedSupportRuns);
          canonicalizeSemanticRuns(
              preparation.projectedSupportRuns, MaterialSemantic::Support);
          preparation.hasSemanticProjection = true;
        });
    }

    {
      ScopedAtomicMicrosecondTimer lineageCommitTimer(
          performanceCounters.forwardLineageCommitMicroseconds);
      for (std::size_t index = 0u; index < current.components.size(); ++index) {
      const auto& component = current.components[index];
      const auto nodeId = currentComponentNodes[index];
      auto& preparation = currentLineagePreparations[index];
      if (preparation.wholeComponentModel) {
        currentSemanticModel.addRuns(component.runs);
        if (preparation.confirmedWholeComponentModel) {
          confirmedSemanticModel.addRuns(component.runs);
        }
        continue;
      }
      if (!preparation.hasSemanticProjection) {
        continue;
      }

      currentSemanticModel.addRuns(preparation.modelRuns);
      propagatedSemanticModel.addRuns(preparation.modelRuns);
      auto& state = states[nodeId];
      state.hasSemanticProjection = true;
      if (options.captureDecisionTrace
          && nodeId < nodeDecisionIndices.size()
          && nodeDecisionIndices[nodeId] != invalidDecision) {
        auto& trace = result.decisions[nodeDecisionIndices[nodeId]];
        trace.mixedSemanticProjection = true;
        trace.modelLineageContinued = true;
        trace.modelLineageOverlapPixels = semanticRunPixelCount(
            preparation.modelRuns);
        trace.modelLineageShiftXPixels = static_cast<double>(
            preparation.modelLineage.shiftX);
        trace.modelLineageShiftYPixels = static_cast<double>(
            preparation.modelLineage.shiftY);
        trace.reason = SupportDecisionReason::MixedSemanticProjection;
      }
      state.projectedSupportRuns = std::move(preparation.projectedSupportRuns);
      }
    }
    currentSemanticModel.normalize();
    confirmedSemanticModel.normalize();
    propagatedSemanticModel.normalize();

    if (result.summary.firstModelLayer == layer) {
      currentStableSemanticModel.assignFrom(currentSemanticModel);
    } else {
      stableSemanticIntersection.assignIntersection(
          currentSemanticModel, previousSemanticModel);
      stableSemanticUnion.assignUnion(
          stableSemanticIntersection, confirmedSemanticModel);
      currentStableSemanticModel.assignUnion(
          stableSemanticUnion, propagatedSemanticModel);
    }
    previousSemanticModel.swap(currentSemanticModel);
    previousPreviousStableSemanticModel.assignFrom(previousStableSemanticModel);
    previousStableSemanticModel.swap(currentStableSemanticModel);

    for (std::size_t index = 0u; index < currentComponentNodes.size(); ++index) {
      if (index < currentIsModel.size() && currentIsModel[index]) {
        currentComponentNodes[index] = invalidNode;
      }
    }
    previousComponentNodes.swap(currentComponentNodes);
    previousCandidateNodes.swap(currentCandidateNodes);
    previousLayer = &current;
    previousModelComponents.swap(currentIsModel);
    if (callbacks.progress) {
      publishComputeTelemetry();
      publishPerformanceTelemetry();
      callbacks.progress(
          supportProgressPhaseWork + layer + 1u, supportProgressTotal);
    }
  }


  // All graph nodes before the first observed model layer are support matter by
  // construction: they belong to the continuous topology that starts on the
  // first layer after the raft.
  for (auto& state : states) {
    if (state.classifiedAsModel) {
      continue;
    }
    const bool supportOnlyPrint = result.summary.firstModelLayer == 0u;
    if ((supportOnlyPrint && result.nodes[state.nodeId].rootedInRaft)
        || (!supportOnlyPrint
            && result.nodes[state.nodeId].layer < result.summary.firstModelLayer)) {
      state.accepted = true;
    }
  }

  // Continue every already established raft-rooted branch through the mixed
  // phase while it remains a separate component. A model contact has no support
  // child, so the semantic path stops on its terminal free layer.
  for (auto& state : states) {
    if (state.classifiedAsModel
        || state.parent == std::numeric_limits<std::size_t>::max()) {
      continue;
    }
    if (!states[state.parent].classifiedAsModel
        && states[state.parent].accepted
        && result.nodes[state.nodeId].rootedInRaft
        && !result.nodes[state.nodeId].rootedInModel) {
      state.accepted = true;
    }
  }

  // Validate heads. A support branch must be rooted in the raft or in a
  // previously established part and must narrow before touching the part.
  for (auto& state : states) {
    if (state.classifiedAsModel
        || !state.modelContact || state.depth < options.minimumTrackLayers) {
      continue;
    }
    const bool rootedInRaft = result.nodes[state.nodeId].rootedInRaft;
    const bool rootedInModel = result.nodes[state.nodeId].rootedInModel;
    const bool validModelRoot = !rootedInModel
                                || rootedInRaft
                                || hasModelRootTaper(state.nodeId, states, options);
    if ((!rootedInRaft && !rootedInModel)
        || !validModelRoot
        || !hasTerminalTaper(state.nodeId, states, options)) {
      continue;
    }
    state.accepted = true;
    result.nodes[state.nodeId].terminalTaper = true;
    result.nodes[state.nodeId].kind = SupportNodeKind::Head;
    acceptPath(state.nodeId, states);
  }

  // Add diagonal braces only when they terminate on an already validated
  // support branch. Propagation uses reverse contact adjacency instead of
  // repeatedly rescanning the complete graph.
  std::vector<std::vector<std::size_t>> braceDependents(states.size());
  for (const auto& state : states) {
    if (state.supportContact
        && state.contactNode != std::numeric_limits<std::size_t>::max()) {
      braceDependents[state.contactNode].push_back(state.nodeId);
    }
  }
  std::queue<std::size_t> acceptedQueue;
  std::vector<bool> queued(states.size(), false);
  for (const auto& state : states) {
    if (state.accepted) {
      acceptedQueue.push(state.nodeId);
      queued[state.nodeId] = true;
    }
  }
  while (!acceptedQueue.empty()) {
    const auto acceptedNode = acceptedQueue.front();
    acceptedQueue.pop();
    for (const auto dependentId : braceDependents[acceptedNode]) {
      auto& dependent = states[dependentId];
      if (dependent.classifiedAsModel || dependent.accepted) {
        continue;
      }
      const double drift = branchDriftPixelsPerLayer(dependent.nodeId, states);
      if (drift < options.braceMinimumDriftPixelsPerLayer
          || drift > options.braceMaximumDriftPixelsPerLayer) {
        continue;
      }
      const auto newlyAccepted = acceptPath(dependent.nodeId, states);
      result.nodes[dependent.nodeId].kind = SupportNodeKind::Brace;
      for (const auto nodeId : newlyAccepted) {
        if (!queued[nodeId]) {
          acceptedQueue.push(nodeId);
          queued[nodeId] = true;
        }
      }
    }
  }

  result.summary.continuationEdgeCount = 0;
  result.summary.splitEdgeCount = 0;
  result.summary.braceEdgeCount = 0;
  result.summary.modelContactEdgeCount = 0;
  for (const auto& edge : result.edges) {
    const bool lowerAccepted = edge.lowerNode < states.size()
                               && states[edge.lowerNode].accepted;
    const bool upperAccepted = edge.upperNode < states.size()
                               && states[edge.upperNode].accepted;
    if (!lowerAccepted || (edge.kind != SupportEdgeKind::ModelContact && !upperAccepted)) {
      continue;
    }
    switch (edge.kind) {
    case SupportEdgeKind::Continuation:
      ++result.summary.continuationEdgeCount;
      break;
    case SupportEdgeKind::Split:
      ++result.summary.splitEdgeCount;
      break;
    case SupportEdgeKind::Brace:
      ++result.summary.braceEdgeCount;
      break;
    case SupportEdgeKind::ModelContact:
      ++result.summary.modelContactEdgeCount;
      break;
    }
  }

  for (auto& state : states) {
    if (state.classifiedAsModel || !state.accepted) {
      result.nodes[state.nodeId].kind = SupportNodeKind::Rejected;
      if (options.captureDecisionTrace
          && state.nodeId < nodeDecisionIndices.size()
          && nodeDecisionIndices[state.nodeId] != invalidDecision) {
        auto& trace = result.decisions[nodeDecisionIndices[state.nodeId]];
        trace.accepted = false;
        if (state.classifiedAsModel) {
          trace.decision = MaterialSemantic::Model;
        } else {
          trace.reason = SupportDecisionReason::RejectedSupportPath;
        }
      }
      continue;
    }
    if (options.captureDecisionTrace
        && state.nodeId < nodeDecisionIndices.size()
        && nodeDecisionIndices[state.nodeId] != invalidDecision) {
      auto& trace = result.decisions[nodeDecisionIndices[state.nodeId]];
      trace.accepted = true;
      trace.decision = MaterialSemantic::Support;
    }
    ++result.summary.acceptedNodeCount;
    auto& layer = result.layers[result.nodes[state.nodeId].layer];
    if (state.hasSemanticProjection) {
      layer.projectedSupportRuns.insert(
          layer.projectedSupportRuns.end(),
          state.projectedSupportRuns.begin(),
          state.projectedSupportRuns.end());
      if (!state.projectedSupportRuns.empty()) {
        result.summary.lastSupportLayer = std::max(
            result.summary.lastSupportLayer, result.nodes[state.nodeId].layer);
      }
    } else {
      layer.supportComponentIds.push_back(
          static_cast<std::uint32_t>(state.component->localId));
      result.summary.freeSupportRunCount += state.runCount;
      result.summary.lastSupportLayer = std::max(
          result.summary.lastSupportLayer, result.nodes[state.nodeId].layer);
    }

    if (state.modelContact && state.contactLayer < result.layers.size()) {
      result.forcedSampleLayers.push_back(result.nodes[state.nodeId].layer);
      result.forcedSampleLayers.push_back(state.contactLayer);
    }

    if (state.modelContact
        && result.nodes[state.nodeId].terminalTaper
        && state.contactLayer < result.layers.size()) {
      ++result.summary.terminalSupportStopCount;
      result.summary.maximumModelExpansionRatio = std::max(
          result.summary.maximumModelExpansionRatio,
          state.contactModelExpansionRatio);
      if (state.contactModelExpansionRatio > 1.0) {
        ++result.summary.expandingModelContactCount;
      }
      const auto terminalPixels = states[state.nodeId].component->area;
      if (state.contactModelPixelCount > terminalPixels) {
        result.summary.rejectedGrowthPixelCount +=
            state.contactModelPixelCount - terminalPixels;
      }
      // The contact component is model matter in full. No support pixel is
      // projected into it.
    } else if (state.modelContact) {
      if (!result.nodes[state.nodeId].terminalTaper) {
        ++result.summary.untaperedModelContactCount;
      }
    }
  }

  // Pass 2: propagate model provenance from the top of the print downward.
  //
  // Pass 1 above intentionally remains biased toward raft-rooted support
  // continuity. Its supportEvidenceRuns are a conservative inherited core; a
  // whole component is not declared model merely because it became ambiguous.
  // The reverse pass supplies the independent evidence that was missing from
  // the one-way classifier. Only the reconciliation below produces final
  // support/model runs.
  const std::size_t forwardFirstModelLayer = result.summary.firstModelLayer;
  const bool forwardModelObserved = forwardFirstModelLayer != 0u;
  const bool reversePassExecuted = result.summary.raftLastLayer + 1u
                                   < source.layerCount();
  if (reversePassExecuted) {
    std::vector<std::unordered_map<std::size_t, std::size_t>> nodeByLayer(
        source.layerCount());
    for (const auto& state : states) {
      const auto nodeLayer = result.nodes[state.nodeId].layer;
      nodeByLayer[nodeLayer].emplace(state.component->localId, state.nodeId);
    }

    std::vector<std::unordered_map<std::size_t, std::size_t>> decisionByLayer;
    if (options.captureDecisionTrace) {
      decisionByLayer.resize(source.layerCount());
      for (std::size_t index = 0; index < result.decisions.size(); ++index) {
        const auto& trace = result.decisions[index];
        if (trace.layer < decisionByLayer.size()) {
          decisionByLayer[trace.layer].emplace(trace.componentId, index);
        }
      }
    }

    for (std::size_t layer = result.summary.raftLastLayer + 1u;
         layer < result.layers.size(); ++layer) {
      result.layers[layer].supportComponentIds.clear();
      result.layers[layer].projectedSupportRuns.clear();
    }
    result.summary.firstModelLayer = 0u;
    result.summary.lastSupportLayer = 0u;
    result.summary.freeSupportRunCount = 0u;
    result.summary.projectedSupportRunCount = 0u;
    result.summary.supportRunCount = 0u;
    result.summary.reverseModelSeedCount = 0u;
    result.summary.reverseModelContinuationCount = 0u;
    result.summary.bidirectionalMixedComponentCount = 0u;

    SparseRunMask upperModel(source.height(), options.enableBitsetAcceleration);
    SparseRunMask upperUpperModel(source.height(), options.enableBitsetAcceleration);
    SparseRunMask upperModelEnvelope(source.height(), options.enableBitsetAcceleration);
    SparseRunMask currentModel(source.height(), options.enableBitsetAcceleration);
    std::vector<std::size_t> reverseLayerMaterialRunCount(source.layerCount(), 0u);
    std::vector<std::size_t> reverseLayerWholeSupportRunCount(source.layerCount(), 0u);
    std::vector<ReverseComponentPreparation> reversePreparations;
    std::size_t reverseCompleted = 0u;

    for (std::size_t reverse = source.layerCount();
         reverse-- > result.summary.raftLastLayer + 1u;) {
      ScopedAtomicMicrosecondTimer reverseSemanticTimer(
          performanceCounters.reverseSemanticMicroseconds);
      const std::size_t layer = reverse;
      if (callbacks.isCancelled && callbacks.isCancelled()) {
        result.cancelled = true;
        result.error = "support analysis cancelled";
        return result;
      }

      const auto& current = layerDescriptions[layer];
      for (const auto& component : current.components) {
        reverseLayerMaterialRunCount[layer] += component.runs.size();
      }
      currentModel.clear();
      upperModelEnvelope.assignDilated(upperModel, maximumModelLineageShift);

      std::size_t layerSupportPixels = 0u;
      std::size_t layerModelPixels = 0u;
      reversePreparations.resize(current.components.size());

      // Stabilisation: reverse reconciliation uses the same canonical sparse
      // zero-shift overlap path as forward reconciliation. This removes the
      // accidental P6.4 layer batch that previously remained active in Auto.

      const compute::TranslatedRunOverlapBatch* reverseResidentBatch = nullptr;
      const bool reverseHasGpuCandidate = computeBackend != nullptr
          && maximumModelLineageShift != 0u
          && std::any_of(
              current.components.begin(), current.components.end(),
              [&](const Component& component) {
                return component.area >= options.vulkanMinimumComponentAreaPixels;
              });
      if (reverseHasGpuCandidate && !upperModel.empty()) {
        const auto preparationStarted = std::chrono::steady_clock::now();
        if (prepareResidentReferenceBatch(
                upperModel, source.width(), source.height(),
                maximumModelLineageShift, residentReferenceGeneration++,
                reverseResidentReference)) {
          const auto preparationEnded = std::chrono::steady_clock::now();
          computeBackend->recordHostPreparation(static_cast<std::uint64_t>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  preparationEnded - preparationStarted).count()));
          reverseResidentBatch = &reverseResidentReference.batch;
        }
      }

      {
        ScopedAtomicMicrosecondTimer reversePreparationTimer(
            performanceCounters.reversePreparationMicroseconds);
        workScheduler.parallelFor(
          current.components.size(),
          [&](std::size_t index, std::size_t workerSlot) {
            const auto& component = current.components[index];
            auto& preparation = reversePreparations[index];
            preparation.nodeId = std::numeric_limits<std::size_t>::max();
            preparation.hasSupportEvidence = false;
            preparation.protectSupportEvidence = false;
            preparation.modelLineage = ModelLineageMotion{};
            preparation.modelSeed = false;
            preparation.modelRuns.clear();
            preparation.finalSupportRuns.clear();
            preparation.supportCorePixels = 0u;
            preparation.modelPixels = 0u;
            auto& scratch = reverseScratch[workerSlot];
            auto& supportCore = scratch.supportCore;
            auto& componentModel = scratch.componentModel;
            auto& inheritedModel = scratch.inheritedModel;
            auto& inheritedRuns = scratch.inheritedRuns;
            auto& inheritedOutsideSupport = scratch.inheritedOutsideSupport;
            inheritedRuns.clear();
            inheritedOutsideSupport.clear();

            const auto nodeIterator = nodeByLayer[layer].find(component.localId);
            const auto reverseInvalidNode = std::numeric_limits<std::size_t>::max();
            const std::size_t nodeId = nodeIterator == nodeByLayer[layer].end()
                ? reverseInvalidNode
                : nodeIterator->second;
            const bool hasSupportEvidence = nodeId != reverseInvalidNode
                && nodeId < states.size()
                && states[nodeId].supportEvidenceActive
                && !states[nodeId].supportEvidenceRuns.empty();
            const bool protectSupportEvidence = hasSupportEvidence
                && !states[nodeId].classifiedAsModel;

            preparation.nodeId = nodeId;
            preparation.hasSupportEvidence = hasSupportEvidence;
            preparation.protectSupportEvidence = protectSupportEvidence;

            supportCore.clear();
            if (protectSupportEvidence) {
              supportCore.addRuns(states[nodeId].supportEvidenceRuns);
              supportCore.normalize();
            }
            ModelLineageMotion modelLineage;
            const auto upperEnvelopeOverlap =
                upperModelEnvelope.countSet(component.runs);
            if (!upperModel.empty() && upperEnvelopeOverlap != 0u) {
              modelLineage = bestTranslatedLineageMotion(
                  component, upperModel,
                  upperUpperModel.empty() ? nullptr : &upperUpperModel,
                  maximumModelLineageShift,
                  protectSupportEvidence ? &supportCore : nullptr,
                  computeBackend.get(), &scratch.computeOverlap,
                  reverseResidentBatch,
                  options.vulkanMinimumComponentAreaPixels);
            }
            preparation.modelLineage = modelLineage;

            const bool topLayerSeed = layer + 1u == source.layerCount();
            const std::size_t independentModelSeedFloor = forwardModelObserved
                ? forwardFirstModelLayer
                : result.summary.raftLastLayer + 1u;
            const bool boundedIndependentModelSeed = !topLayerSeed
                && layer >= independentModelSeedFloor
                && !modelLineage.continued
                && !hasSupportEvidence;
            const bool modelSeed = topLayerSeed || boundedIndependentModelSeed;
            preparation.modelSeed = modelSeed;

            componentModel.clear();
            supportCore.clear();
            inheritedModel.clear();
            auto& modelRuns = preparation.modelRuns;
            auto& finalSupportRuns = preparation.finalSupportRuns;
            std::size_t supportCorePixels = 0u;

            if (modelSeed) {
              modelRuns = component.runs;
              canonicalizeSemanticRuns(modelRuns, MaterialSemantic::Model);
            } else if (modelLineage.continued) {
              upperModel.appendTranslatedIntersectionRuns(
                  component.runs,
                  modelLineage.shiftX,
                  modelLineage.shiftY,
                  MaterialSemantic::Model,
                  inheritedRuns);
              canonicalizeSemanticRuns(inheritedRuns, MaterialSemantic::Model);
              inheritedModel.addRuns(inheritedRuns);
              inheritedModel.normalize();

              if (protectSupportEvidence) {
                const auto& supportState = states[nodeId];
                bool continuingSupportParent = supportState.supportEvidenceParents.empty();
                for (const auto parentNode : supportState.supportEvidenceParents) {
                  if (parentNode >= states.size()
                      || !states[parentNode].supportEvidenceActive) {
                    continue;
                  }
                  if (!supportEvidenceStopsBefore(
                          parentNode, component.area, states, options)) {
                    continuingSupportParent = true;
                    break;
                  }
                }
                if (continuingSupportParent) {
                  supportCore.addRuns(supportState.supportEvidenceRuns);
                }
                supportCore.normalize();
                supportCorePixels = supportCore.countSet(component.runs);

                supportCore.appendClearRuns(
                    inheritedRuns,
                    MaterialSemantic::Model,
                    inheritedOutsideSupport);
                canonicalizeSemanticRuns(
                    inheritedOutsideSupport, MaterialSemantic::Model);
                if (!inheritedOutsideSupport.empty()) {
                  supportCore.appendClearRuns(
                      component.runs,
                      MaterialSemantic::Model,
                      modelRuns);
                  canonicalizeSemanticRuns(modelRuns, MaterialSemantic::Model);
                }
              } else {
                modelRuns = component.runs;
                canonicalizeSemanticRuns(modelRuns, MaterialSemantic::Model);
              }
            }

            componentModel.addRuns(modelRuns);
            componentModel.normalize();
            const auto modelPixels = semanticRunPixelCount(modelRuns);
            if (protectSupportEvidence && modelPixels != 0u) {
              componentModel.appendClearRuns(
                  states[nodeId].supportEvidenceRuns,
                  MaterialSemantic::Support,
                  finalSupportRuns);
              canonicalizeSemanticRuns(
                  finalSupportRuns, MaterialSemantic::Support);
            }
            preparation.supportCorePixels = supportCorePixels;
            preparation.modelPixels = modelPixels;
          });
      }

      if (callbacks.isCancelled && callbacks.isCancelled()) {
        result.cancelled = true;
        result.error = "support analysis cancelled";
        return result;
      }

      {
        ScopedAtomicMicrosecondTimer reverseCommitTimer(
            performanceCounters.reverseCommitMicroseconds);
        for (std::size_t index = 0u; index < current.components.size(); ++index) {
        const auto& component = current.components[index];
        auto& preparation = reversePreparations[index];
        const auto nodeId = preparation.nodeId;
        const bool protectSupportEvidence = preparation.protectSupportEvidence;
        const auto& modelLineage = preparation.modelLineage;
        const bool modelSeed = preparation.modelSeed;
        const auto modelPixels = preparation.modelPixels;
        const auto supportCorePixels = preparation.supportCorePixels;
        auto& modelRuns = preparation.modelRuns;
        auto& finalSupportRuns = preparation.finalSupportRuns;

        currentModel.addRuns(modelRuns);
        layerModelPixels += modelPixels;

        if (protectSupportEvidence) {
          if (modelPixels == 0u) {
            result.layers[layer].supportComponentIds.push_back(
                static_cast<std::uint32_t>(component.localId));
            reverseLayerWholeSupportRunCount[layer] += component.runs.size();
            layerSupportPixels += component.area;
          } else if (!finalSupportRuns.empty()) {
            const auto supportPixels = semanticRunPixelCount(finalSupportRuns);
            layerSupportPixels += supportPixels;
            result.layers[layer].projectedSupportRuns.insert(
                result.layers[layer].projectedSupportRuns.end(),
                finalSupportRuns.begin(), finalSupportRuns.end());
            ++result.summary.bidirectionalMixedComponentCount;
          }
        }

        if (modelSeed && modelPixels != 0u) {
          ++result.summary.reverseModelSeedCount;
        } else if (modelLineage.continued && modelPixels != 0u) {
          ++result.summary.reverseModelContinuationCount;
        }

        if (options.captureDecisionTrace) {
          const auto decisionIterator = decisionByLayer[layer].find(
              component.localId);
          if (decisionIterator != decisionByLayer[layer].end()) {
            auto& trace = result.decisions[decisionIterator->second];
            trace.reverseModelEvidencePixels = modelPixels;
            trace.reverseSupportCorePixels = supportCorePixels;
            trace.finalSupportPixels = modelPixels == 0u && protectSupportEvidence
                ? component.area
                : semanticRunPixelCount(finalSupportRuns);
            trace.finalModelPixels = component.area >= trace.finalSupportPixels
                ? component.area - trace.finalSupportPixels
                : modelPixels;
            trace.reverseModelLineageContinued = modelLineage.continued;
            trace.reverseModelSeed = modelSeed;
            trace.bidirectionalConflict = modelPixels != 0u && protectSupportEvidence;
            if (trace.finalSupportPixels != 0u && trace.finalModelPixels != 0u) {
              trace.mixedSemanticProjection = true;
              trace.accepted = true;
              trace.decision = MaterialSemantic::Support;
              trace.reason = SupportDecisionReason::BidirectionalMixedReconciliation;
            } else if (trace.finalModelPixels != 0u) {
              trace.accepted = false;
              trace.decision = MaterialSemantic::Model;
              trace.reason = SupportDecisionReason::BidirectionalModelContinuation;
            }
          }
        }
        }
      }
      currentModel.normalize();

      auto& componentIds = result.layers[layer].supportComponentIds;
      std::sort(componentIds.begin(), componentIds.end());
      componentIds.erase(
          std::unique(componentIds.begin(), componentIds.end()),
          componentIds.end());
      auto& projected = result.layers[layer].projectedSupportRuns;
      canonicalizeSemanticRuns(projected, MaterialSemantic::Support);
      result.summary.projectedSupportRunCount += projected.size();

      if (layerSupportPixels != 0u) {
        result.summary.lastSupportLayer = std::max(
            result.summary.lastSupportLayer, layer);
      }
      if (layerModelPixels != 0u
          && (result.summary.firstModelLayer == 0u
              || layer < result.summary.firstModelLayer)) {
        result.summary.firstModelLayer = layer;
      }

      upperUpperModel.assignFrom(upperModel);
      upperModel.swap(currentModel);
      ++reverseCompleted;
      if (callbacks.progress) {
        publishComputeTelemetry();
        publishPerformanceTelemetry();
        callbacks.progress(
            supportProgressPhaseWork * 2u + reverseCompleted,
            supportProgressTotal);
      }
    }
    // `SupportsOnly` materialization deliberately treats every exposed run as
    // support, even when pass 1 did not need to create an explicit graph node
    // for that run. Recompute the compact run counters from the final phase
    // boundary so diagnostics and verification describe exactly what
    // materializeLayerSemantics() will emit.
    result.summary.freeSupportRunCount = 0u;
    for (std::size_t layer = result.summary.raftLastLayer + 1u;
         layer < result.layers.size(); ++layer) {
      if (result.summary.firstModelLayer == 0u
          || layer < result.summary.firstModelLayer) {
        result.summary.freeSupportRunCount += reverseLayerMaterialRunCount[layer];
      } else {
        result.summary.freeSupportRunCount += reverseLayerWholeSupportRunCount[layer];
      }
    }
    result.summary.supportRunCount = result.summary.freeSupportRunCount
                                     + result.summary.projectedSupportRunCount;
  }

  std::sort(result.forcedSampleLayers.begin(), result.forcedSampleLayers.end());
  result.forcedSampleLayers.erase(
      std::unique(result.forcedSampleLayers.begin(), result.forcedSampleLayers.end()),
      result.forcedSampleLayers.end());
  result.summary.forcedSemanticSampleCount = result.forcedSampleLayers.size();

  for (std::size_t layer = result.summary.raftLastLayer + 1;
       layer < result.layers.size(); ++layer) {
    if (result.summary.firstModelLayer == 0 || layer < result.summary.firstModelLayer) {
      result.layers[layer].phase = PrintPhase::SupportsOnly;
    } else if (layer <= result.summary.lastSupportLayer) {
      result.layers[layer].phase = PrintPhase::ModelAndSupports;
    } else {
      result.layers[layer].phase = PrintPhase::ModelMostly;
    }
    auto& componentIds = result.layers[layer].supportComponentIds;
    std::sort(componentIds.begin(), componentIds.end());
    componentIds.erase(
        std::unique(componentIds.begin(), componentIds.end()), componentIds.end());

    auto& projected = result.layers[layer].projectedSupportRuns;
    canonicalizeSemanticRuns(projected, MaterialSemantic::Support);
    if (!reversePassExecuted) {
      result.summary.projectedSupportRunCount += projected.size();
    }
  }

  if (!reversePassExecuted) {
    result.summary.supportRunCount = result.summary.freeSupportRunCount
                                     + result.summary.projectedSupportRunCount;
    if (callbacks.progress) {
      publishComputeTelemetry();
      publishPerformanceTelemetry();
      callbacks.progress(supportProgressTotal, supportProgressTotal);
    }
  }

  const auto performanceTelemetry = performanceCounters.snapshot();
  result.summary.supportPreparationWindowCapacity =
      performanceTelemetry.preparationWindowCapacity;
  result.summary.supportPreparedLayerCount = performanceTelemetry.preparedLayerCount;
  result.summary.supportMaximumPreparationInflight =
      performanceTelemetry.maximumPreparationInflight;
  result.summary.supportPreparationLoadMicroseconds =
      performanceTelemetry.preparationLoadMicroseconds;
  result.summary.supportPreparationDescribeMicroseconds =
      performanceTelemetry.preparationDescribeMicroseconds;
  result.summary.supportForwardSemanticMicroseconds =
      performanceTelemetry.forwardSemanticMicroseconds;
  result.summary.supportReverseSemanticMicroseconds =
      performanceTelemetry.reverseSemanticMicroseconds;
  result.summary.supportForwardClassificationMicroseconds =
      performanceTelemetry.forwardClassificationMicroseconds;
  result.summary.supportForwardCommitMicroseconds =
      performanceTelemetry.forwardCommitMicroseconds;
  result.summary.supportForwardLineageMicroseconds =
      performanceTelemetry.forwardLineageMicroseconds;
  result.summary.supportForwardLineageCommitMicroseconds =
      performanceTelemetry.forwardLineageCommitMicroseconds;
  result.summary.supportReversePreparationMicroseconds =
      performanceTelemetry.reversePreparationMicroseconds;
  result.summary.supportReverseCommitMicroseconds =
      performanceTelemetry.reverseCommitMicroseconds;
  result.summary.supportSemanticEvidenceMicroseconds =
      performanceTelemetry.semanticEvidenceMicroseconds;
  result.summary.supportSemanticEvidenceLotCount =
      performanceTelemetry.semanticEvidenceLotCount;
  result.summary.supportSemanticEvidenceLayerPairCount =
      performanceTelemetry.semanticEvidenceLayerPairCount;
  result.summary.supportSemanticEvidenceEdgeCount =
      performanceTelemetry.semanticEvidenceEdgeCount;
  publishPerformanceTelemetry();

  if (computeBackend) {
    const auto telemetry = computeBackend->telemetry();
    result.summary.vulkanEligibleJobCount = telemetry.eligibleJobs;
    result.summary.vulkanSubmittedJobCount = telemetry.submittedJobs;
    result.summary.vulkanGpuJobCount = telemetry.completedGpuJobs;
    result.summary.vulkanCpuFallbackJobCount = telemetry.cpuFallbackJobs;
    result.summary.vulkanDispatchCount = telemetry.successfulDispatches;
    result.summary.vulkanDispatchFailureCount = telemetry.failedDispatches;
    result.summary.vulkanMaximumBatchJobCount = telemetry.maximumBatchJobs;
    result.summary.vulkanUploadBytes = telemetry.uploadBytes;
    result.summary.vulkanReadbackBytes = telemetry.readbackBytes;
    result.summary.vulkanHostPreparationMicroseconds =
        telemetry.hostPreparationNanoseconds / 1000u;
    result.summary.vulkanQueueWaitMicroseconds = telemetry.queueWaitNanoseconds / 1000u;
    result.summary.vulkanBatchExecutionMicroseconds =
        telemetry.batchExecutionNanoseconds / 1000u;
    result.summary.vulkanRunSourceJobCount = telemetry.runSourceJobs;
    result.summary.vulkanResidentReferenceUploadCount =
        telemetry.residentReferenceUploads;
    result.summary.vulkanResidentReferenceReuseCount =
        telemetry.residentReferenceReuses;
    result.summary.vulkanSubmittedWorkgroupCount = telemetry.submittedWorkgroups;
    result.summary.vulkanSemanticLayerBatchCallCount =
        telemetry.semanticLayerBatchCalls;
    result.summary.vulkanSemanticLayerBatchJobCount =
        telemetry.semanticLayerBatchJobs;
  }
  result.ok = true;
  return result;
}

bool SupportAnalyzer::materializeLayerSemantics(
    const photons::BinaryMask& mask,
    const LayerSemanticIndex& index,
    std::vector<SemanticRun>& runs,
    std::string& error) const {
  runs.clear();
  if (mask.empty()) {
    error = "support semantic materialization source is empty";
    return false;
  }

  auto description = describeLayer(mask, index.layer);
  const bool allRaft = index.phase == PrintPhase::Raft;
  const bool allSupport = index.phase == PrintPhase::SupportsOnly;
  for (auto& component : description.components) {
    MaterialSemantic semantic = MaterialSemantic::Model;
    if (allRaft) {
      semantic = MaterialSemantic::Raft;
    } else if (allSupport
               || std::binary_search(
                   index.supportComponentIds.begin(),
                   index.supportComponentIds.end(),
                   static_cast<std::uint32_t>(component.localId))) {
      semantic = MaterialSemantic::Support;
    }
    if (semantic == MaterialSemantic::Model) {
      continue;
    }
    for (auto run : component.runs) {
      run.semantic = semantic;
      runs.push_back(run);
    }
  }
  runs.insert(
      runs.end(),
      index.projectedSupportRuns.begin(),
      index.projectedSupportRuns.end());
  std::sort(runs.begin(), runs.end(), [](const SemanticRun& left, const SemanticRun& right) {
    if (left.y != right.y) {
      return left.y < right.y;
    }
    if (left.firstX != right.firstX) {
      return left.firstX < right.firstX;
    }
    return left.lastX < right.lastX;
  });
  std::size_t write = 0u;
  for (const auto& run : runs) {
    if (write != 0u
        && runs[write - 1u].semantic == run.semantic
        && runs[write - 1u].y == run.y
        && run.firstX <= runs[write - 1u].lastX) {
      runs[write - 1u].lastX = std::max(runs[write - 1u].lastX, run.lastX);
    } else {
      runs[write++] = run;
    }
  }
  runs.resize(write);
  error.clear();
  return true;
}

} // namespace accloud::render3d
