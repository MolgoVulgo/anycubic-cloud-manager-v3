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

bool candidateShape(const Component& component, const SupportAnalysisOptions& options) {
  const double area = static_cast<double>(component.area)
                      * options.pitchXMillimetres * options.pitchYMillimetres;
  return area <= options.maximumSupportAreaMillimetres2
         && equivalentDiameterMillimetres(component, options)
                <= options.maximumSupportDiameterMillimetres;
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

double centreDistanceMillimetres(
    const Component& left,
    const Component& right,
    const SupportAnalysisOptions& options) {
  const double dx = (left.centerX - right.centerX) * options.pitchXMillimetres;
  const double dy = (left.centerY - right.centerY) * options.pitchYMillimetres;
  return std::hypot(dx, dy);
}

struct NodeState {
  std::size_t nodeId = 0;
  std::size_t parent = std::numeric_limits<std::size_t>::max();
  std::size_t depth = 1;
  Component component;
  std::size_t runCount = 0;
  bool accepted = false;
  bool modelContact = false;
  bool supportContact = false;
  std::size_t contactNode = std::numeric_limits<std::size_t>::max();
  std::size_t branchOrigin = std::numeric_limits<std::size_t>::max();
  std::size_t contactLayer = std::numeric_limits<std::size_t>::max();
  std::size_t contactModelPixelCount = 0;
  double contactModelExpansionRatio = 0.0;
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
      previous.minX, previous.maxX, current.minX, current.maxX))
                      * options.pitchXMillimetres;
  const double gapY = static_cast<double>(axisGap(
      previous.minY, previous.maxY, current.minY, current.maxY))
                      * options.pitchYMillimetres;
  const double previousRadius = equivalentDiameterMillimetres(previous, options) * 0.5;
  const double allowedMotion = options.pitchZMillimetres * options.maximumLayerSlope;
  return std::hypot(gapX, gapY) <= previousRadius + allowedMotion;
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
        centreDistanceMillimetres(previous, current, options),
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

std::vector<std::size_t> acceptPath(
    std::size_t nodeId,
    std::vector<NodeState>& states) {
  std::vector<std::size_t> acceptedNodes;
  while (nodeId != std::numeric_limits<std::size_t>::max()) {
    if (states[nodeId].accepted) {
      break;
    }
    states[nodeId].accepted = true;
    acceptedNodes.push_back(nodeId);
    nodeId = states[nodeId].parent;
  }
  return acceptedNodes;
}

double branchSlope(
    std::size_t nodeId,
    const std::vector<NodeState>& states,
    const SupportAnalysisOptions& options) {
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
  const double lateral = centreDistanceMillimetres(
      lower.component, upper.component, options);
  const double vertical = static_cast<double>(zLayers) * options.pitchZMillimetres;
  return vertical > 0.0 ? lateral / vertical : 0.0;
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
      || options.maximumRaftLayers == 0
      || options.minimumTrackLayers == 0
      || !(options.terminalTaperRatio > 0.0 && options.terminalTaperRatio <= 1.0)
      || !(options.modelRootTaperRatio > 0.0 && options.modelRootTaperRatio <= 1.0)) {
    result.error = "support analysis options are invalid";
    return result;
  }

  result.layers.resize(source.layerCount());
  std::vector<NodeState> states;
  std::vector<std::size_t> previousCandidateNodes;
  std::optional<LayerDescription> previousLayer;
  std::size_t firstArea = 0;
  std::size_t firstLargestArea = 0;
  bool raftEnded = false;
  bool modelSeen = false;
  std::size_t lastLayerWithCandidate = 0;

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
      firstArea = current.totalArea;
      firstLargestArea = current.largestArea;
    }

    if (!raftEnded) {
      const bool withinSearch = layer < options.maximumRaftLayers;
      const bool retainedArea = firstArea != 0
                                && static_cast<double>(current.totalArea)
                                       >= static_cast<double>(firstArea)
                                              * options.raftRetainedAreaRatio;
      const bool retainedLargest = firstLargestArea != 0
                                   && static_cast<double>(current.largestArea)
                                          >= static_cast<double>(firstLargestArea)
                                                 * options.raftRetainedAreaRatio;
      if (layer == 0 || (withinSearch && (retainedArea || retainedLargest))) {
        result.summary.raftLastLayer = layer;
        result.layers[layer].layer = layer;
        result.layers[layer].phase = PrintPhase::Raft;
        for (const auto& component : current.components) {
          result.summary.raftRunCount += component.runs.size();
        }
        previousLayer = std::move(current);
        previousCandidateNodes.clear();
        if (callbacks.progress) {
          callbacks.progress(layer + 1, source.layerCount());
        }
        continue;
      }
      raftEnded = true;
    }

    result.layers[layer].layer = layer;
    const bool layerContainsModelShape = std::any_of(
        current.components.begin(), current.components.end(),
        [&](const Component& component) {
          return !candidateShape(component, options);
        });
    if (!modelSeen && layerContainsModelShape) {
      modelSeen = true;
      result.summary.firstModelLayer = layer;
    }
    const bool supportOnlyLayer = !modelSeen;

    std::vector<std::size_t> currentCandidateNodes;
    std::vector<bool> currentIsCandidate(current.components.size(), false);
    const auto previousStateCount = states.size();
    std::vector<bool> previousHasStructuralChild(previousStateCount, false);
    for (std::size_t index = 0; index < current.components.size(); ++index) {
      currentIsCandidate[index] = supportOnlyLayer
                                  || candidateShape(current.components[index], options);
      if (!currentIsCandidate[index]) {
        continue;
      }

      const auto matches = matchingPreviousNodes(
          current.components[index], previousCandidateNodes, states, options);
      std::size_t parent = std::numeric_limits<std::size_t>::max();
      bool rootedInRaft = supportOnlyLayer
                              || layer == result.summary.raftLastLayer + 1u;
      bool rootedInModel = false;
      if (!matches.empty()) {
        parent = matches.front().previousNode;
        rootedInRaft = supportOnlyLayer || result.nodes[parent].rootedInRaft;
        rootedInModel = result.nodes[parent].rootedInModel;
      } else if (!supportOnlyLayer && previousLayer) {
        for (const auto& previousComponent : previousLayer->components) {
          if (candidateShape(previousComponent, options)) {
            continue;
          }
          if (nearEnough(previousComponent, current.components[index], options)) {
            rootedInModel = true;
            break;
          }
        }
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
      lastLayerWithCandidate = layer;

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
        // the validated approximately-45-degree slope.
        for (std::size_t matchIndex = 1; matchIndex < matches.size(); ++matchIndex) {
          const auto other = matches[matchIndex].previousNode;
          const double currentSlope = branchSlope(nodeId, states, options);
          const double otherSlope = branchSlope(other, states, options);
          if (currentSlope >= options.braceMinimumSlope
              && currentSlope <= options.braceMaximumSlope) {
            states[nodeId].supportContact = true;
            states[nodeId].contactNode = other;
            result.edges.push_back(SupportGraphEdge{
                nodeId, other, SupportEdgeKind::Brace});
            ++result.summary.braceEdgeCount;
          } else if (otherSlope >= options.braceMinimumSlope
                     && otherSlope <= options.braceMaximumSlope) {
            states[other].supportContact = true;
            states[other].contactNode = nodeId;
            result.edges.push_back(SupportGraphEdge{
                other, nodeId, SupportEdgeKind::Brace});
            ++result.summary.braceEdgeCount;
          }
        }
      }
    }

    // A compact branch that reaches a non-compact component has touched the
    // part. The branch remains independent and stops on its last free layer.
    // The larger connected component on the contact layer is model matter in
    // full: support semantics are never projected into it.
    for (const auto previousNode : previousCandidateNodes) {
      // A branch that still has a continuation or a split on this layer has
      // not reached its terminal head yet. Proximity to a large model
      // component must not create an early contact projection while the
      // branch is still narrowing beside the part.
      if (previousNode < previousHasStructuralChild.size()
          && previousHasStructuralChild[previousNode]) {
        continue;
      }
      const auto& previousComponent = states[previousNode].component;
      for (std::size_t index = 0; index < current.components.size(); ++index) {
        if (currentIsCandidate[index]) {
          continue;
        }
        if (!nearEnough(previousComponent, current.components[index], options)) {
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

    for (const auto previousNode : previousCandidateNodes) {
      std::vector<SemanticRun>().swap(states[previousNode].component.runs);
    }
    previousCandidateNodes = std::move(currentCandidateNodes);
    previousLayer = std::move(current);
    if (callbacks.progress) {
      callbacks.progress(layer + 1, source.layerCount());
    }
  }

  for (const auto previousNode : previousCandidateNodes) {
    std::vector<SemanticRun>().swap(states[previousNode].component.runs);
  }

  // By construction, all matter between the mandatory raft and the first
  // detected part layer belongs to the support network. This is the stable
  // support-only phase of consumer resin printing and does not require a
  // local shape guess.
  for (auto& state : states) {
    if (result.summary.firstModelLayer != 0
        && result.nodes[state.nodeId].layer < result.summary.firstModelLayer) {
      state.accepted = true;
    }
  }

  // Continue every already established raft-rooted branch through the mixed
  // phase while it remains a separate candidate component. The semantic path
  // stops before the non-candidate model component, so the part itself is not
  // absorbed. Supports that start on the model are handled separately below
  // and still require a terminal taper.
  for (auto& state : states) {
    if (state.parent == std::numeric_limits<std::size_t>::max()) {
      continue;
    }
    if (states[state.parent].accepted
        && result.nodes[state.nodeId].rootedInRaft
        && !result.nodes[state.nodeId].rootedInModel) {
      state.accepted = true;
    }
  }

  // Validate mixed-phase heads. A support branch must be rooted in the raft or
  // in a previously established part, and must narrow before touching the part.
  for (auto& state : states) {
    if (!state.modelContact || state.depth < options.minimumTrackLayers) {
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
  // repeatedly rescanning the complete graph, which keeps large support
  // networks linear in the number of nodes and contacts.
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
      if (dependent.accepted) {
        continue;
      }
      const double slope = branchSlope(dependent.nodeId, states, options);
      if (slope < options.braceMinimumSlope || slope > options.braceMaximumSlope) {
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
    if (!state.accepted) {
      result.nodes[state.nodeId].kind = SupportNodeKind::Rejected;
      continue;
    }
    ++result.summary.acceptedNodeCount;
    auto& layer = result.layers[result.nodes[state.nodeId].layer];
    layer.supportComponentIds.push_back(
        static_cast<std::uint32_t>(state.component.localId));
    result.summary.freeSupportRunCount += state.runCount;
    result.summary.lastSupportLayer = std::max(
        result.summary.lastSupportLayer, result.nodes[state.nodeId].layer);

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
      // The contact component is the model. Deliberately keep the projected
      // support run list empty: the terminal support ends on the preceding
      // free layer and no pixel of the larger component inherits Support.
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
