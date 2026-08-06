#include "render3d/analysis/SupportAnalyzer.h"

#include "domain/photons/BinaryMask.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <queue>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace accloud::render3d {
namespace {

using photons::BinaryMask;
using PixelRun = std::pair<std::uint32_t, std::uint32_t>;

class DisjointSet {
public:
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
  return runs;
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

class ComponentGridIndex {
public:
  ComponentGridIndex(
      const LayerDescription* layer,
      const std::vector<bool>& selected,
      double marginPixels)
      : componentCount_(layer == nullptr ? 0u : layer->components.size()),
        stamps_(componentCount_, 0u) {
    if (layer == nullptr) {
      return;
    }
    const auto margin = static_cast<std::uint32_t>(
        std::max(0.0, std::ceil(marginPixels)));
    for (std::size_t index = 0; index < layer->components.size(); ++index) {
      if (index >= selected.size() || !selected[index]) {
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

  [[nodiscard]] std::vector<std::size_t> query(const Component& component) {
    std::vector<std::size_t> result;
    if (componentCount_ == 0u) {
      return result;
    }
    if (++generation_ == 0u) {
      std::fill(stamps_.begin(), stamps_.end(), 0u);
      generation_ = 1u;
    }
    forEachCell(
        component.minX, component.minY, component.maxX, component.maxY,
        [&](std::uint64_t key) {
          const auto iterator = cells_.find(key);
          if (iterator == cells_.end()) {
            return;
          }
          for (const auto index : iterator->second) {
            if (stamps_[index] == generation_) {
              continue;
            }
            stamps_[index] = generation_;
            result.push_back(index);
          }
        });
    return result;
  }

private:
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
  std::vector<std::size_t> previousRow;
  DisjointSet sets;
  for (std::uint32_t y = 0; y < mask.height(); ++y) {
    const auto rowRuns = collectRuns(
        mask.width(), mask.wordsPerRow(),
        [&](std::size_t wordIndex) { return mask.rowWord(y, wordIndex); });
    std::vector<std::size_t> currentRow;
    currentRow.reserve(rowRuns.size());
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
    previousRow = std::move(currentRow);
  }

  std::map<std::size_t, std::size_t> componentIndex;
  for (auto& run : runs) {
    const auto root = sets.find(run.label);
    auto [iterator, inserted] = componentIndex.try_emplace(root, description.components.size());
    if (inserted) {
      description.components.push_back(Component{});
      description.components.back().localId = iterator->second;
    }
    auto& component = description.components[iterator->second];
    const auto length = static_cast<std::size_t>(run.last - run.first);
    component.area += length;
    component.minX = std::min(component.minX, run.first);
    component.maxX = std::max(component.maxX, run.last);
    component.minY = std::min(component.minY, run.y);
    component.maxY = std::max(component.maxY, run.y + 1u);
    component.centerX += (static_cast<double>(run.first + run.last - 1u) * 0.5)
                         * static_cast<double>(length);
    component.centerY += static_cast<double>(run.y) * static_cast<double>(length);
    component.runs.push_back(SemanticRun{
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
  std::vector<SemanticRun> merged;
  merged.reserve(runs.size());
  for (const auto& run : runs) {
    if (run.firstX >= run.lastX) {
      continue;
    }
    if (!merged.empty()
        && merged.back().y == run.y
        && run.firstX <= merged.back().lastX) {
      merged.back().lastX = std::max(merged.back().lastX, run.lastX);
    } else {
      merged.push_back(run);
    }
  }
  runs = std::move(merged);
}

class SparseRunMask {
public:
  explicit SparseRunMask(std::uint32_t height = 0u)
      : rows_(height) {}

  void clear() {
    for (const auto y : touchedRows_) {
      rows_[y].clear();
    }
    touchedRows_.clear();
  }

  void swap(SparseRunMask& other) noexcept {
    rows_.swap(other.rows_);
    touchedRows_.swap(other.touchedRows_);
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

  void normalize() {
    for (const auto y : touchedRows_) {
      auto& row = rows_[y];
      std::sort(row.begin(), row.end());
      std::vector<PixelRun> merged;
      merged.reserve(row.size());
      for (const auto& interval : row) {
        if (!merged.empty() && interval.first <= merged.back().second) {
          merged.back().second = std::max(merged.back().second, interval.second);
        } else {
          merged.push_back(interval);
        }
      }
      row = std::move(merged);
    }
    std::sort(touchedRows_.begin(), touchedRows_.end());
    touchedRows_.erase(
        std::unique(touchedRows_.begin(), touchedRows_.end()),
        touchedRows_.end());
  }

  void assignFrom(const SparseRunMask& source) {
    clear();
    for (const auto y : source.touchedRows_) {
      rows_[y] = source.rows_[y];
      touchedRows_.push_back(y);
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
    const auto height = static_cast<std::uint32_t>(rows_.size());
    for (std::uint32_t y = 0; y < height; ++y) {
      const auto& a = left.rows_[y];
      const auto& b = right.rows_[y];
      if (a.empty() || b.empty()) {
        continue;
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
    row.emplace_back(firstX, lastX);
  }

  void addMask(const SparseRunMask& source) {
    for (const auto y : source.touchedRows_) {
      auto& row = rows_[y];
      if (row.empty()) {
        touchedRows_.push_back(y);
      }
      row.insert(row.end(), source.rows_[y].begin(), source.rows_[y].end());
    }
  }

  std::vector<std::vector<PixelRun>> rows_;
  std::vector<std::uint32_t> touchedRows_;
};

struct NodeState {
  std::size_t nodeId = 0;
  std::size_t parent = std::numeric_limits<std::size_t>::max();
  std::size_t depth = 1;
  Component component;
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
  std::vector<SemanticRun> projectedSupportRuns;
};

class NodeGridIndex {
public:
  NodeGridIndex(
      const std::vector<std::size_t>& nodes,
      const std::vector<NodeState>& states,
      double marginPixels)
      : stamps_(states.size(), 0u) {
    const auto margin = static_cast<std::uint32_t>(
        std::max(0.0, std::ceil(marginPixels)));
    for (const auto nodeId : nodes) {
      if (nodeId >= states.size()) {
        continue;
      }
      const auto& component = states[nodeId].component;
      const auto minX = component.minX > margin ? component.minX - margin : 0u;
      const auto minY = component.minY > margin ? component.minY - margin : 0u;
      const auto maxX = component.maxX + margin;
      const auto maxY = component.maxY + margin;
      add(nodeId, minX, minY, maxX, maxY);
    }
  }

  [[nodiscard]] std::vector<std::size_t> query(const Component& component) {
    std::vector<std::size_t> result;
    if (cells_.empty()) {
      return result;
    }
    if (++generation_ == 0u) {
      std::fill(stamps_.begin(), stamps_.end(), 0u);
      generation_ = 1u;
    }
    forEachCell(
        component.minX, component.minY, component.maxX, component.maxY,
        [&](std::uint64_t key) {
          const auto iterator = cells_.find(key);
          if (iterator == cells_.end()) {
            return;
          }
          for (const auto nodeId : iterator->second) {
            if (stamps_[nodeId] == generation_) {
              continue;
            }
            stamps_[nodeId] = generation_;
            result.push_back(nodeId);
          }
        });
    return result;
  }

private:
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
      std::size_t nodeId,
      std::uint32_t minX,
      std::uint32_t minY,
      std::uint32_t maxX,
      std::uint32_t maxY) {
    forEachCell(minX, minY, maxX, maxY, [&](std::uint64_t key) {
      cells_[key].push_back(nodeId);
    });
  }

  std::unordered_map<std::uint64_t, std::vector<std::size_t>> cells_;
  std::vector<std::uint32_t> stamps_;
  std::uint32_t generation_ = 0u;
};

struct Match {
  std::size_t previousNode = 0;
  std::size_t overlap = 0;
  double distance = 0.0;
};

bool nearEnough(
    const Component& previous,
    const Component& current,
    const SupportAnalysisOptions& options) {
  if (overlapPixels(previous, current) != 0) {
    return true;
  }
  const auto axisGap = [](std::uint32_t firstMin,
                          std::uint32_t firstMax,
                          std::uint32_t secondMin,
                          std::uint32_t secondMax) {
    if (firstMax < secondMin) {
      return secondMin - firstMax;
    }
    if (secondMax < firstMin) {
      return firstMin - secondMax;
    }
    return 0u;
  };
  const double gapX = static_cast<double>(axisGap(
      previous.minX, previous.maxX, current.minX, current.maxX));
  const double gapY = static_cast<double>(axisGap(
      previous.minY, previous.maxY, current.minY, current.maxY));
  return std::hypot(gapX, gapY) <= options.maximumLayerMotionPixels;
}

std::vector<Match> matchingPreviousNodes(
    const Component& current,
    const std::vector<std::size_t>& previousNodes,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
  std::vector<Match> matches;
  for (const auto nodeId : previousNodes) {
    const auto& previous = states[nodeId].component;
    if (!nearEnough(previous, current, options)) {
      continue;
    }
    matches.push_back(Match{
        nodeId,
        overlapPixels(previous, current),
        centreDistancePixels(previous, current),
    });
  }
  std::sort(matches.begin(), matches.end(), [](const Match& left, const Match& right) {
    if (left.overlap != right.overlap) {
      return left.overlap > right.overlap;
    }
    if (left.distance != right.distance) {
      return left.distance < right.distance;
    }
    return left.previousNode < right.previousNode;
  });
  return matches;
}

bool hasModelRootTaper(
    std::size_t nodeId,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
  std::size_t cursor = nodeId;
  std::size_t root = nodeId;
  std::size_t maximumArea = states[nodeId].component.area;
  std::size_t observed = 0;
  while (cursor != std::numeric_limits<std::size_t>::max()) {
    root = cursor;
    maximumArea = std::max(maximumArea, states[cursor].component.area);
    cursor = states[cursor].parent;
    ++observed;
  }
  if (observed < options.minimumTrackLayers || maximumArea == 0) {
    return false;
  }
  const auto rootArea = states[root].component.area;
  return maximumArea > rootArea
         && static_cast<double>(rootArea)
                <= static_cast<double>(maximumArea) * options.modelRootTaperRatio;
}

bool hasTerminalTaper(
    std::size_t nodeId,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
  const auto finalArea = states[nodeId].component.area;
  if (finalArea == 0) {
    return false;
  }
  std::size_t cursor = nodeId;
  std::size_t observed = 0;
  std::size_t maximumEarlierArea = finalArea;
  while (cursor != std::numeric_limits<std::size_t>::max()
         && observed < options.taperLookbackLayers) {
    maximumEarlierArea = std::max(maximumEarlierArea, states[cursor].component.area);
    cursor = states[cursor].parent;
    ++observed;
  }
  return observed >= 2
         && static_cast<double>(finalArea)
                <= static_cast<double>(maximumEarlierArea) * options.terminalTaperRatio;
}

std::size_t recentMaximumArea(
    std::size_t nodeId,
    const std::vector<NodeState>& states,
    std::size_t lookbackLayers) {
  std::size_t maximumArea = 0;
  std::size_t observed = 0;
  while (nodeId != std::numeric_limits<std::size_t>::max()
         && observed < lookbackLayers) {
    maximumArea = std::max(maximumArea, states[nodeId].component.area);
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
  return centreDistancePixels(lower.component, upper.component)
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
      || !(options.raftMaximumChangedPixelRatio >= 0.0
           && options.raftMaximumChangedPixelRatio < 1.0)
      || !(options.maximumLayerMotionPixels >= 0.0)
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

  result.layers.resize(source.layerCount());
  std::vector<NodeState> states;
  std::vector<std::size_t> previousCandidateNodes;
  std::optional<LayerDescription> previousLayer;
  std::vector<bool> previousModelComponents;
  std::optional<BinaryMask> firstRaftMask;
  std::size_t firstRaftArea = 0u;
  bool raftEnded = false;
  bool modelSeen = false;
  SparseRunMask previousSemanticModel(source.height());
  SparseRunMask previousStableSemanticModel(source.height());
  SparseRunMask currentSemanticModel(source.height());
  SparseRunMask confirmedSemanticModel(source.height());
  SparseRunMask currentStableSemanticModel(source.height());
  SparseRunMask stableSemanticIntersection(source.height());

  for (std::size_t layer = 0; layer < source.layerCount(); ++layer) {
    if (callbacks.isCancelled && callbacks.isCancelled()) {
      result.cancelled = true;
      result.error = "support analysis cancelled";
      return result;
    }
    std::string error;
    auto mask = source.loadMask(layer, error);
    if (!mask) {
      result.error = error.empty() ? "support analysis could not load a layer" : error;
      return result;
    }
    auto current = describeLayer(*mask, layer);
    result.summary.componentCount += current.components.size();
    if (layer == 0) {
      if (current.totalArea == 0) {
        result.error = "support analysis requires raft matter on the first layer";
        return result;
      }
      firstRaftMask = *mask;
      firstRaftArea = current.totalArea;
    }

    // The raft is the mandatory prefix starting at layer zero. Slicers may use
    // a plate, a grid or independent pads, but the selected raft raster is
    // repeated until support stems begin. The first different native mask is
    // therefore the first support layer; no fixed layer window or area limit is
    // involved in this decision.
    if (!raftEnded) {
      const auto maximumChangedPixels = static_cast<std::size_t>(std::ceil(
          static_cast<double>(firstRaftArea)
          * options.raftMaximumChangedPixelRatio));
      const bool sameAsFirstRaft = layer == 0
                                   || (firstRaftMask
                                       && changedPixelCount(*mask, *firstRaftMask)
                                              <= maximumChangedPixels);
      if (sameAsFirstRaft) {
        result.summary.raftLastLayer = layer;
        result.layers[layer].layer = layer;
        result.layers[layer].phase = PrintPhase::Raft;
        for (const auto& component : current.components) {
          result.summary.raftRunCount += component.runs.size();
        }
        previousLayer = std::move(current);
        previousModelComponents.assign(previousLayer->components.size(), false);
        previousCandidateNodes.clear();
        if (callbacks.progress) {
          callbacks.progress(layer + 1, source.layerCount());
        }
        continue;
      }
      raftEnded = true;
    }

    result.layers[layer].layer = layer;
    const bool modelExistedBeforeLayer = modelSeen;
    const bool firstSupportLayer = layer == result.summary.raftLastLayer + 1u;

    std::vector<std::size_t> currentCandidateNodes;
    const auto invalidNode = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> currentComponentNodes(
        current.components.size(), invalidNode);
    std::vector<bool> currentIsModel(current.components.size(), false);
    const auto previousStateCount = states.size();
    std::vector<bool> previousHasStructuralChild(previousStateCount, false);
    std::size_t maximumPreviousSupportArea = 0u;
    for (const auto nodeId : previousCandidateNodes) {
      maximumPreviousSupportArea = std::max(
          maximumPreviousSupportArea, states[nodeId].component.area);
    }
    ComponentGridIndex previousModelIndex(
        previousLayer ? &*previousLayer : nullptr,
        previousModelComponents,
        options.maximumLayerMotionPixels);
    NodeGridIndex previousSupportIndex(
        previousCandidateNodes, states, options.maximumLayerMotionPixels);

    for (std::size_t index = 0; index < current.components.size(); ++index) {
      const auto nearbySupportNodes = previousSupportIndex.query(
          current.components[index]);
      const auto matches = matchingPreviousNodes(
          current.components[index], nearbySupportNodes, states, options);

      bool overlapsPreviousModel = false;
      bool nearPreviousModel = false;
      if (previousLayer) {
        for (const auto previousIndex : previousModelIndex.query(
                 current.components[index])) {
          const auto& previousComponent = previousLayer->components[previousIndex];
          const auto modelOverlap = overlapPixels(
              previousComponent, current.components[index]);
          if (modelOverlap != 0u) {
            overlapsPreviousModel = true;
            nearPreviousModel = true;
            continue;
          }
          nearPreviousModel = nearPreviousModel
                              || nearEnough(
                                  previousComponent, current.components[index], options);
        }
      }

      bool taperedExpansion = false;
      if (!matches.empty()) {
        // Only the best structural parent may open a support-to-model contact
        // candidate. Secondary matches remain brace relations. More
        // importantly, an already tracked support keeps its semantic identity:
        // overlap with an earlier model layer or a local area increase cannot
        // cut the branch by itself.
        const auto& match = matches.front();
        const auto& previousState = states[match.previousNode];
        if (previousState.component.area != 0u
            && current.components[index].area > previousState.component.area) {
          const double expansion = static_cast<double>(current.components[index].area)
                                   / static_cast<double>(previousState.component.area);
          const bool rootedOnlyInModel =
              result.nodes[match.previousNode].rootedInModel
              && !result.nodes[match.previousNode].rootedInRaft;
          const bool validModelRoot = !rootedOnlyInModel
                                      || hasModelRootTaper(
                                          match.previousNode, states, options);
          const bool displacedGrowth = expansion > 1.0
              && centreDistancePixels(
                     previousState.component, current.components[index])
                     > options.maximumLayerMotionPixels;
          const bool growthMayOpenContact =
              expansion >= options.minimumModelExpansionRatio
              || displacedGrowth;
          taperedExpansion = previousState.depth >= options.minimumTrackLayers
                             && validModelRoot
                             && growthMayOpenContact
                             && hasTerminalTaper(
                                 match.previousNode, states, options);
        }
      }

      // A tracked branch keeps its support identity across local overlap with
      // already established model matter. The overlap may come from a brace, a
      // temporary raster merge or the beginning of a real contact. Only a
      // model-dominant merge that is far larger than the recent support profile
      // can bypass continuity, and never when a terminal taper has opened a
      // provisional contact candidate.
      std::size_t supportReferenceArea = 0u;
      if (!matches.empty()) {
        supportReferenceArea = recentMaximumArea(
            matches.front().previousNode, states, options.taperLookbackLayers);
      }
      const bool plausibleSupportContinuation = !matches.empty()
          && (taperedExpansion
              || (supportReferenceArea != 0u
                  && static_cast<double>(current.components[index].area)
                         <= static_cast<double>(supportReferenceArea)
                                * options.abruptModelExpansionRatio));
      const bool modelDominantMerge = overlapsPreviousModel
          && !plausibleSupportContinuation;
      bool isModel = modelDominantMerge;

      bool rootedInModel = false;
      bool isSupportCandidate = false;
      bool unparentedRaftSupport = false;
      if (!isModel) {
        if (!matches.empty() || firstSupportLayer) {
          isSupportCandidate = true;
          unparentedRaftSupport = firstSupportLayer && matches.empty();
        } else if (modelExistedBeforeLayer && nearPreviousModel) {
          // A separate component born beside established model matter can be a
          // model-rooted support. It remains only a candidate until both its
          // narrow root and terminal taper are validated.
          isSupportCandidate = true;
          rootedInModel = true;
        } else if (!modelExistedBeforeLayer) {
          const bool relativeModelExpansion = maximumPreviousSupportArea != 0u
              && static_cast<double>(current.components[index].area)
                     >= static_cast<double>(maximumPreviousSupportArea)
                            * options.abruptModelExpansionRatio;
          if (relativeModelExpansion) {
            isModel = true;
          } else {
            // Before the first part contact, small disconnected components are
            // still part of the support network (for example a discretised
            // diagonal brace whose previous raster does not overlap).
            isSupportCandidate = true;
            unparentedRaftSupport = true;
          }
        } else {
          // Once model matter exists, an unrelated component is model unless it
          // has an explicit support path or a narrow root beside the model.
          isModel = true;
        }
      }

      if (isModel) {
        currentIsModel[index] = true;
        continue;
      }
      if (!isSupportCandidate) {
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
      state.component = current.components[index];
      state.runCount = current.components[index].runs.size();
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
      currentCandidateNodes.push_back(nodeId);
      ++result.summary.candidateNodeCount;

      if (parent != std::numeric_limits<std::size_t>::max()) {
        if (parent < previousHasStructuralChild.size()) {
          previousHasStructuralChild[parent] = true;
        }
        std::size_t siblingCount = 0;
        for (const auto candidateNode : currentCandidateNodes) {
          if (states[candidateNode].parent == parent) {
            ++siblingCount;
          }
        }
        const auto edgeKind = siblingCount > 1
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
        for (std::size_t matchIndex = 1; matchIndex < matches.size(); ++matchIndex) {
          const auto other = matches[matchIndex].previousNode;
          const double currentDrift = branchDriftPixelsPerLayer(nodeId, states);
          const double otherDrift = branchDriftPixelsPerLayer(other, states);
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
          const auto tipArea = states[parentState.pendingContactTip].component.area;
          const bool remainsAboveTip = tipArea != 0u
              && currentState.component.area >= tipArea;
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
            if (matchedState.component.area == 0u
                || currentState.component.area <= matchedState.component.area) {
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

        const auto pendingTipArea = currentState.pendingContactTip == invalidNode
            ? 0u
            : states[currentState.pendingContactTip].component.area;
        const auto pendingStartArea = currentState.pendingContactStart == invalidNode
            ? 0u
            : states[currentState.pendingContactStart].component.area;
        const bool abruptLocalContact = pendingTipArea != 0u
            && static_cast<double>(pendingStartArea)
                   >= static_cast<double>(pendingTipArea)
                          * options.abruptModelExpansionRatio;
        const bool cumulativeGrowthConfirmed = pendingStartArea != 0u
            && static_cast<double>(currentState.component.area)
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
            tipState.contactModelPixelCount = states[startNode].component.area;
            tipState.contactModelExpansionRatio = tipState.component.area == 0u
                ? 0.0
                : static_cast<double>(states[startNode].component.area)
                      / static_cast<double>(tipState.component.area);
            result.edges.push_back(SupportGraphEdge{
                contactTip, contactTip, SupportEdgeKind::ModelContact});
            ++result.summary.modelContactEdgeCount;
          }

          currentIsModel[index] = true;
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
          const bool stillGrowing = currentState.component.area
                                    > parentState.component.area;
          const bool confirmationWindowExhausted =
              currentState.pendingContactLength >= options.taperLookbackLayers;
          if (!stillGrowing || confirmationWindowExhausted) {
            // No cumulative model growth was established. Keep the complete
            // sequence as support and allow a later terminal growth to open a
            // new local candidate.
            currentState.pendingContactTip = invalidNode;
            currentState.pendingContactStart = invalidNode;
            currentState.pendingContactLength = 0u;
            currentState.pendingContactTips.clear();
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
    // against any component already classified as model. The complete contact
    // component stays model; support semantics stop on the preceding layer.
    for (const auto previousNode : previousCandidateNodes) {
      if (previousNode < previousHasStructuralChild.size()
          && previousHasStructuralChild[previousNode]) {
        continue;
      }
      const auto& previousComponent = states[previousNode].component;
      for (std::size_t index = 0; index < current.components.size(); ++index) {
        if (!currentIsModel[index]
            || !nearEnough(previousComponent, current.components[index], options)) {
          continue;
        }
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
        break;
      }
    }

    // Preserve semantic continuity independently from the whole-component
    // graph decision. A component may contain both an established model region
    // and a raft-rooted support after a temporary raster fusion. Only model
    // matter that was stable on preceding native layers is allowed to override
    // the support decision; the remaining pixels stay attached to the support
    // branch and are emitted as projected support runs.
    currentSemanticModel.clear();
    confirmedSemanticModel.clear();
    for (std::size_t index = 0; index < current.components.size(); ++index) {
      const auto nodeId = currentComponentNodes[index];
      if (currentIsModel[index]) {
        currentSemanticModel.addRuns(current.components[index].runs);
        if (nodeId != invalidNode && states[nodeId].classifiedAsModel) {
          confirmedSemanticModel.addRuns(current.components[index].runs);
        }
        continue;
      }
      if (nodeId == invalidNode) {
        if (modelSeen) {
          currentSemanticModel.addRuns(current.components[index].runs);
        }
        continue;
      }

      const auto persistentModelPixels = previousStableSemanticModel.countSet(
          current.components[index].runs);
      if (persistentModelPixels == 0u) {
        continue;
      }
      currentSemanticModel.addIntersection(
          current.components[index].runs, previousStableSemanticModel);
      auto& state = states[nodeId];
      state.hasSemanticProjection = true;
      state.projectedSupportRuns.clear();
      previousStableSemanticModel.appendClearRuns(
          current.components[index].runs,
          MaterialSemantic::Support,
          state.projectedSupportRuns);
      canonicalizeSemanticRuns(
          state.projectedSupportRuns, MaterialSemantic::Support);
    }
    currentSemanticModel.normalize();
    confirmedSemanticModel.normalize();

    if (result.summary.firstModelLayer == layer) {
      currentStableSemanticModel.assignFrom(currentSemanticModel);
    } else {
      stableSemanticIntersection.assignIntersection(
          currentSemanticModel, previousSemanticModel);
      currentStableSemanticModel.assignUnion(
          stableSemanticIntersection, confirmedSemanticModel);
    }
    previousSemanticModel.swap(currentSemanticModel);
    previousStableSemanticModel.swap(currentStableSemanticModel);

    for (const auto previousNode : previousCandidateNodes) {
      std::vector<SemanticRun>().swap(states[previousNode].component.runs);
    }
    previousCandidateNodes = std::move(currentCandidateNodes);
    previousLayer = std::move(current);
    previousModelComponents = std::move(currentIsModel);
    if (callbacks.progress) {
      callbacks.progress(layer + 1, source.layerCount());
    }
  }

  for (const auto previousNode : previousCandidateNodes) {
    std::vector<SemanticRun>().swap(states[previousNode].component.runs);
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
      continue;
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
          static_cast<std::uint32_t>(state.component.localId));
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
      const auto terminalPixels = states[state.nodeId].component.area;
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
    std::sort(projected.begin(), projected.end(), [](const SemanticRun& left,
                                                     const SemanticRun& right) {
      if (left.y != right.y) {
        return left.y < right.y;
      }
      if (left.firstX != right.firstX) {
        return left.firstX < right.firstX;
      }
      return left.lastX < right.lastX;
    });
    std::vector<SemanticRun> merged;
    for (const auto& run : projected) {
      if (!merged.empty()
          && merged.back().y == run.y
          && run.firstX <= merged.back().lastX) {
        merged.back().lastX = std::max(merged.back().lastX, run.lastX);
      } else {
        merged.push_back(run);
      }
    }
    projected = std::move(merged);
    result.summary.projectedSupportRunCount += projected.size();
  }

  result.summary.supportRunCount = result.summary.freeSupportRunCount
                                   + result.summary.projectedSupportRunCount;

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
  std::vector<SemanticRun> merged;
  for (const auto& run : runs) {
    if (!merged.empty()
        && merged.back().semantic == run.semantic
        && merged.back().y == run.y
        && run.firstX <= merged.back().lastX) {
      merged.back().lastX = std::max(merged.back().lastX, run.lastX);
    } else {
      merged.push_back(run);
    }
  }
  runs = std::move(merged);
  error.clear();
  return true;
}

} // namespace accloud::render3d
