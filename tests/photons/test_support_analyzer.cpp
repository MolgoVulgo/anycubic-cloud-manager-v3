#include "domain/photons/BinaryMask.h"
#include "domain/photons/LayerMaskSource.h"
#include "render3d/analysis/SupportAnalyzer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
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
      error = "layer outside support analyzer test source";
      return std::nullopt;
    }
    return layers_[layerNumber];
  }

private:
  std::vector<accloud::photons::BinaryMask> layers_;
};

class TrackingSource final : public accloud::photons::LayerMaskSource {
public:
  TrackingSource(
      std::vector<accloud::photons::BinaryMask> layers,
      bool concurrentLoads)
      : layers_(std::move(layers)), concurrentLoads_(concurrentLoads) {}

  std::size_t layerCount() const noexcept override { return layers_.size(); }
  std::uint32_t width() const noexcept override { return layers_.front().width(); }
  std::uint32_t height() const noexcept override { return layers_.front().height(); }
  bool supportsConcurrentMaskLoads() const noexcept override {
    return concurrentLoads_;
  }

  std::optional<accloud::photons::BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) override {
    if (layerNumber >= layers_.size()) {
      error = "layer outside tracking support analyzer source";
      return std::nullopt;
    }
    totalLoads_.fetch_add(1u, std::memory_order_relaxed);
    const auto active = activeLoads_.fetch_add(1u, std::memory_order_relaxed) + 1u;
    auto observed = maximumActiveLoads_.load(std::memory_order_relaxed);
    while (active > observed
           && !maximumActiveLoads_.compare_exchange_weak(
               observed, active, std::memory_order_relaxed)) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    auto result = layers_[layerNumber];
    activeLoads_.fetch_sub(1u, std::memory_order_relaxed);
    return result;
  }

  [[nodiscard]] std::size_t maximumActiveLoads() const noexcept {
    return maximumActiveLoads_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] std::size_t totalLoads() const noexcept {
    return totalLoads_.load(std::memory_order_relaxed);
  }

private:
  std::vector<accloud::photons::BinaryMask> layers_;
  bool concurrentLoads_ = false;
  std::atomic<std::size_t> activeLoads_{0u};
  std::atomic<std::size_t> maximumActiveLoads_{0u};
  std::atomic<std::size_t> totalLoads_{0u};
};

void fillRect(
    accloud::photons::BinaryMask& mask,
    std::uint32_t firstX,
    std::uint32_t firstY,
    std::uint32_t lastX,
    std::uint32_t lastY) {
  for (std::uint32_t y = firstY; y < lastY; ++y) {
    for (std::uint32_t x = firstX; x < lastX; ++x) {
      mask.set(x, y, true);
    }
  }
}

void addSquare(
    accloud::photons::BinaryMask& mask,
    std::uint32_t centerX,
    std::uint32_t centerY,
    std::uint32_t radius) {
  fillRect(mask, centerX - radius, centerY - radius,
           centerX + radius + 1u, centerY + radius + 1u);
}

std::size_t countSemanticRuns(
    const accloud::render3d::SupportAnalysisResult& result,
    accloud::render3d::MaterialSemantic semantic) {
  if (semantic == accloud::render3d::MaterialSemantic::Support) {
    return result.summary.supportRunCount;
  }
  if (semantic == accloud::render3d::MaterialSemantic::Raft) {
    return result.summary.raftRunCount;
  }
  return 0;
}

bool layerHasSupport(const accloud::render3d::LayerSemanticIndex& layer) {
  return layer.phase == accloud::render3d::PrintPhase::SupportsOnly
         || !layer.supportComponentIds.empty()
         || !layer.projectedSupportRuns.empty();
}

bool layerHasRaftSupportProvenance(
    const accloud::render3d::LayerSemanticIndex& layer) {
  return !layer.raftSupportProvenanceComponentIds.empty();
}

bool pixelHasSemantic(
    const accloud::render3d::SupportAnalyzer& analyzer,
    const accloud::photons::BinaryMask& mask,
    const accloud::render3d::LayerSemanticIndex& index,
    std::uint32_t x,
    std::uint32_t y,
    accloud::render3d::MaterialSemantic semantic) {
  std::vector<accloud::render3d::SemanticRun> runs;
  std::string error;
  if (!analyzer.materializeLayerSemantics(mask, index, runs, error)) {
    return false;
  }
  const auto iterator = std::find_if(
      runs.begin(), runs.end(), [&](const auto& run) {
        return run.semantic == semantic && run.y == y
               && x >= run.firstX && x < run.lastX;
      });
  if (semantic == accloud::render3d::MaterialSemantic::Model) {
    return mask.test(x, y) && iterator == runs.end()
           && std::none_of(runs.begin(), runs.end(), [&](const auto& run) {
                return run.y == y && x >= run.firstX && x < run.lastX;
              });
  }
  return iterator != runs.end();
}

std::size_t countNodeKind(
    const accloud::render3d::SupportAnalysisResult& result,
    accloud::render3d::SupportNodeKind kind) {
  return static_cast<std::size_t>(std::count_if(
      result.nodes.begin(), result.nodes.end(),
      [&](const auto& node) { return node.kind == kind; }));
}

bool structuralMergesAreExplicit(
    const accloud::render3d::SupportAnalysisResult& result) {
  std::vector<std::size_t> incoming(result.nodes.size(), 0u);
  std::vector<std::size_t> incomingJoins(result.nodes.size(), 0u);
  for (const auto& edge : result.edges) {
    if (edge.kind != accloud::render3d::SupportEdgeKind::Continuation
        && edge.kind != accloud::render3d::SupportEdgeKind::Split
        && edge.kind != accloud::render3d::SupportEdgeKind::Join) {
      continue;
    }
    if (edge.upperNode >= incoming.size()) {
      return false;
    }
    ++incoming[edge.upperNode];
    incomingJoins[edge.upperNode] +=
        edge.kind == accloud::render3d::SupportEdgeKind::Join ? 1u : 0u;
  }
  for (std::size_t nodeId = 0; nodeId < result.nodes.size(); ++nodeId) {
    if (incoming[nodeId] > 1u && incomingJoins[nodeId] == 0u) {
      return false;
    }
    if (result.nodes[nodeId].kind == accloud::render3d::SupportNodeKind::Junction
        && (incoming[nodeId] < 2u || incomingJoins[nodeId] == 0u)) {
      return false;
    }
  }
  return true;
}

std::size_t countIncomingStructuralEdges(
    const accloud::render3d::SupportAnalysisResult& result,
    std::size_t nodeId) {
  return static_cast<std::size_t>(std::count_if(
      result.edges.begin(), result.edges.end(), [&](const auto& edge) {
        return edge.upperNode == nodeId
               && (edge.kind == accloud::render3d::SupportEdgeKind::Continuation
                   || edge.kind == accloud::render3d::SupportEdgeKind::Split
                   || edge.kind == accloud::render3d::SupportEdgeKind::Join);
      }));
}

std::size_t countIncomingJoinEdges(
    const accloud::render3d::SupportAnalysisResult& result,
    std::size_t nodeId) {
  return static_cast<std::size_t>(std::count_if(
      result.edges.begin(), result.edges.end(), [&](const auto& edge) {
        return edge.upperNode == nodeId
               && edge.kind == accloud::render3d::SupportEdgeKind::Join;
      }));
}

bool compactIndexIsCanonical(
    const accloud::render3d::SupportAnalysisResult& result) {
  return std::all_of(
      result.layers.begin(), result.layers.end(),
      [](const auto& layer) {
        const bool componentIdsCanonical = std::is_sorted(
            layer.supportComponentIds.begin(), layer.supportComponentIds.end())
            && std::adjacent_find(
                   layer.supportComponentIds.begin(),
                   layer.supportComponentIds.end())
                   == layer.supportComponentIds.end();
        const bool provenanceIdsCanonical = std::is_sorted(
            layer.raftSupportProvenanceComponentIds.begin(),
            layer.raftSupportProvenanceComponentIds.end())
            && std::adjacent_find(
                   layer.raftSupportProvenanceComponentIds.begin(),
                   layer.raftSupportProvenanceComponentIds.end())
                   == layer.raftSupportProvenanceComponentIds.end();
        const bool projectedCanonical = std::is_sorted(
            layer.projectedSupportRuns.begin(),
            layer.projectedSupportRuns.end(),
            [](const auto& left, const auto& right) {
              if (left.y != right.y) {
                return left.y < right.y;
              }
              if (left.firstX != right.firstX) {
                return left.firstX < right.firstX;
              }
              return left.lastX < right.lastX;
            })
            && std::adjacent_find(
                   layer.projectedSupportRuns.begin(),
                   layer.projectedSupportRuns.end(),
                   [](const auto& left, const auto& right) {
                     return left.y == right.y && right.firstX <= left.lastX;
                   })
                   == layer.projectedSupportRuns.end();
        return componentIdsCanonical && provenanceIdsCanonical
               && projectedCanonical;
      });
}

bool materializationMatchesSummary(
    const accloud::render3d::SupportAnalyzer& analyzer,
    const std::vector<accloud::photons::BinaryMask>& layers,
    const accloud::render3d::SupportAnalysisResult& result) {
  if (layers.size() != result.layers.size()) {
    return false;
  }
  std::size_t supportRuns = 0;
  std::size_t raftRuns = 0;
  for (std::size_t layer = 0; layer < layers.size(); ++layer) {
    std::vector<accloud::render3d::SemanticRun> runs;
    std::string error;
    if (!analyzer.materializeLayerSemantics(
            layers[layer], result.layers[layer], runs, error)) {
      return false;
    }
    for (const auto& run : runs) {
      supportRuns += run.semantic == accloud::render3d::MaterialSemantic::Support;
      raftRuns += run.semantic == accloud::render3d::MaterialSemantic::Raft;
    }
  }
  return supportRuns == result.summary.supportRunCount
         && raftRuns == result.summary.raftRunCount;
}

bool forcedSamplesAreCanonical(
    const accloud::render3d::SupportAnalysisResult& result) {
  return std::is_sorted(
             result.forcedSampleLayers.begin(),
             result.forcedSampleLayers.end())
         && std::adjacent_find(
                result.forcedSampleLayers.begin(),
                result.forcedSampleLayers.end())
                == result.forcedSampleLayers.end()
         && std::all_of(
                result.forcedSampleLayers.begin(),
                result.forcedSampleLayers.end(),
                [&](std::size_t layer) { return layer < result.layers.size(); })
         && result.summary.forcedSemanticSampleCount
                == result.forcedSampleLayers.size();
}


bool sameSemanticClassification(
    const accloud::render3d::SupportAnalysisResult& left,
    const accloud::render3d::SupportAnalysisResult& right) {
  const auto& a = left.summary;
  const auto& b = right.summary;
  if (left.ok != right.ok
      || a.raftLastLayer != b.raftLastLayer
      || a.firstModelLayer != b.firstModelLayer
      || a.lastSupportLayer != b.lastSupportLayer
      || a.candidateNodeCount != b.candidateNodeCount
      || a.acceptedNodeCount != b.acceptedNodeCount
      || a.supportRunCount != b.supportRunCount
      || a.terminalSupportStopCount != b.terminalSupportStopCount
      || a.continuationEdgeCount != b.continuationEdgeCount
      || a.splitEdgeCount != b.splitEdgeCount
      || a.junctionEdgeCount != b.junctionEdgeCount
      || a.braceEdgeCount != b.braceEdgeCount
      || a.modelContactEdgeCount != b.modelContactEdgeCount
      || a.reverseModelSeedCount != b.reverseModelSeedCount
      || a.reverseModelTopSeedCount != b.reverseModelTopSeedCount
      || a.reverseModelLocalMaximumSeedCount
             != b.reverseModelLocalMaximumSeedCount
      || a.reverseModelContinuationCount != b.reverseModelContinuationCount
      || a.bidirectionalMixedComponentCount != b.bidirectionalMixedComponentCount
      || a.raftSupportProvenanceComponentCount
             != b.raftSupportProvenanceComponentCount
      || left.layers.size() != right.layers.size()
      || left.nodes.size() != right.nodes.size()
      || left.edges.size() != right.edges.size()
      || left.forcedSampleLayers != right.forcedSampleLayers) {
    return false;
  }
  for (std::size_t index = 0; index < left.layers.size(); ++index) {
    const auto& leftLayer = left.layers[index];
    const auto& rightLayer = right.layers[index];
    if (leftLayer.layer != rightLayer.layer
        || leftLayer.phase != rightLayer.phase
        || leftLayer.supportComponentIds != rightLayer.supportComponentIds
        || leftLayer.raftSupportProvenanceComponentIds
               != rightLayer.raftSupportProvenanceComponentIds
        || leftLayer.projectedSupportRuns.size()
               != rightLayer.projectedSupportRuns.size()) {
      return false;
    }
    for (std::size_t run = 0; run < leftLayer.projectedSupportRuns.size(); ++run) {
      const auto& leftRun = leftLayer.projectedSupportRuns[run];
      const auto& rightRun = rightLayer.projectedSupportRuns[run];
      if (leftRun.y != rightRun.y
          || leftRun.firstX != rightRun.firstX
          || leftRun.lastX != rightRun.lastX
          || leftRun.semantic != rightRun.semantic) {
        return false;
      }
    }
  }
  for (std::size_t index = 0; index < left.nodes.size(); ++index) {
    const auto& leftNode = left.nodes[index];
    const auto& rightNode = right.nodes[index];
    if (leftNode.id != rightNode.id
        || leftNode.layer != rightNode.layer
        || leftNode.areaPixels != rightNode.areaPixels
        || leftNode.kind != rightNode.kind
        || leftNode.rootedInRaft != rightNode.rootedInRaft
        || leftNode.rootedInModel != rightNode.rootedInModel
        || leftNode.terminalTaper != rightNode.terminalTaper) {
      return false;
    }
  }
  for (std::size_t index = 0; index < left.edges.size(); ++index) {
    const auto& leftEdge = left.edges[index];
    const auto& rightEdge = right.edges[index];
    if (leftEdge.lowerNode != rightEdge.lowerNode
        || leftEdge.upperNode != rightEdge.upperNode
        || leftEdge.kind != rightEdge.kind) {
      return false;
    }
  }
  return true;
}

accloud::render3d::SupportAnalysisOptions testOptions() {
  accloud::render3d::SupportAnalysisOptions options;
  options.pitchXMillimetres = 0.1;
  options.pitchYMillimetres = 0.1;
  options.pitchZMillimetres = 0.1;
  options.minimumTrackLayers = 3;
  options.taperLookbackLayers = 6;
  options.modelContactConfirmationLayers = 2;
  options.raftHeightMillimetres = 0.2;
  options.maximumLayerMotionPixels = 2.2;
  options.minimumSupportShapeOverlapRatio = 0.85;
  options.supportSegmentLookbackLayers = 8u;
  options.supportSegmentMaximumResidualPixels = 1.5;
  options.braceTargetAngleDegrees = 45.0;
  options.braceAngleToleranceDegrees = 8.0;
  options.braceMinimumSegmentLayerSpan = 2u;
  options.minimumModelExpansionRatio = 1.2;
  options.abruptModelExpansionRatio = 4.0;
  options.terminalTaperRatio = 0.72;
  options.modelRootTaperRatio = 0.72;
  return options;
}

std::vector<accloud::photons::BinaryMask> makeBranchedSupportScene() {
  constexpr std::uint32_t width = 48;
  constexpr std::uint32_t height = 32;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 13; ++index) {
    layers.emplace_back(width, height);
  }

  // Mandatory raft.
  fillRect(layers[0], 4, 23, 44, 30);
  fillRect(layers[1], 4, 23, 44, 30);

  // Two independent pillars.
  for (int layer = 2; layer <= 6; ++layer) {
    addSquare(layers[layer], 13, 23, 1);
    addSquare(layers[layer], 34, 23, 1);
  }

  // The left pillar splits into two branches. Neither branch rejoins another.
  addSquare(layers[7], 12, 22, 1);
  addSquare(layers[7], 15, 21, 1);
  addSquare(layers[7], 34, 23, 1);
  addSquare(layers[8], 11, 21, 1);
  addSquare(layers[8], 16, 20, 1);
  addSquare(layers[8], 34, 23, 1);

  // A diagonal brace leaves the right pillar at approximately 45 degrees.
  addSquare(layers[7], 31, 22, 0);
  addSquare(layers[8], 30, 21, 0);
  addSquare(layers[9], 29, 20, 0);

  // Every terminal head narrows before touching the part.
  addSquare(layers[9], 10, 20, 0);
  addSquare(layers[9], 17, 19, 0);
  addSquare(layers[9], 34, 22, 0);

  // Large part component contacted by the three heads.
  fillRect(layers[10], 8, 8, 39, 21);
  fillRect(layers[11], 7, 7, 40, 21);
  fillRect(layers[12], 6, 6, 41, 21);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makePhysicalBraceScene() {
  constexpr std::uint32_t width = 48;
  constexpr std::uint32_t height = 34;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 12; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 28, 45, 33);
  fillRect(layers[1], 3, 28, 45, 33);

  // Two established vertical supports. Keep the target pillar far enough
  // away that the adjacency tolerance cannot create an early synthetic join.
  for (int layer = 2; layer <= 9; ++layer) {
    addSquare(layers[layer], 10, 27, 0);
    addSquare(layers[layer], 24, 27, 0);
  }

  // A separate branch leaves the left support and advances exactly one native
  // X pixel per native Z layer. With equal X/Z pitches this is a 45-degree bar.
  // It remains separate until layer 10, where the raster itself joins the bar
  // to the right pillar. The right pillar is then the overlapping primary
  // parent and the 45-degree lineage must stay a secondary Brace edge.
  for (int layer = 3; layer <= 9; ++layer) {
    addSquare(layers[layer], 12 + static_cast<std::uint32_t>(layer - 3), 27, 0);
  }
  addSquare(layers[10], 10, 27, 0);
  fillRect(layers[10], 19, 27, 25, 28); // brace + right pillar become one component
  addSquare(layers[11], 10, 27, 0);
  addSquare(layers[11], 24, 27, 0);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makePiecewiseLinearSupportScene() {
  constexpr std::uint32_t width = 40;
  constexpr std::uint32_t height = 30;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 10; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 25, 37, 29);
  fillRect(layers[1], 3, 25, 37, 29);

  // First straight segment: +1 X pixel per layer.
  addSquare(layers[2], 10, 24, 0);
  addSquare(layers[3], 11, 24, 0);
  addSquare(layers[4], 12, 24, 0);
  addSquare(layers[5], 13, 24, 0);

  // Direction changes abruptly but remains inside the native adjacency window.
  // This must terminate the first segment; subsequent layers form a new
  // straight segment instead of being fitted as a continuous curve.
  addSquare(layers[6], 11, 24, 0);
  addSquare(layers[7], 10, 24, 0);
  addSquare(layers[8], 9, 24, 0);
  addSquare(layers[9], 8, 24, 0);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeAccumulatedBendSupportScene() {
  constexpr std::uint32_t width = 48;
  constexpr std::uint32_t height = 30;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 14; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 25, 45, 29);
  fillRect(layers[1], 3, 25, 45, 29);

  // Each local change remains within the adjacency window, but the complete
  // history gradually departs from one straight line. P4 must eventually cut
  // the lineage and restart a straight segment instead of retaining a curve.
  const std::uint32_t xs[] = {10u, 11u, 12u, 13u, 14u, 16u, 18u, 20u, 22u, 24u};
  for (std::size_t i = 0; i < std::size(xs); ++i) {
    addSquare(layers[2u + i], xs[i], 23, 0);
  }
  fillRect(layers[12], 34, 4, 42, 10);
  fillRect(layers[13], 34, 4, 42, 10);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeUntaperedContactScene() {
  constexpr std::uint32_t width = 64;
  constexpr std::uint32_t height = 24;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 10; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 4, 18, 28, 23);
  fillRect(layers[1], 4, 18, 28, 23);
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 16, 18, 1);
  }
  fillRect(layers[6], 8, 6, 25, 19);
  fillRect(layers[7], 7, 5, 26, 19);
  // Keep this local support-continuation fixture physically consistent with
  // the bidirectional invariant: the global top is model matter, not support.
  fillRect(layers[8], 50, 2, 60, 10);
  fillRect(layers[9], 50, 2, 60, 10);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeMixedPhaseContinuationScene() {
  constexpr std::uint32_t width = 40;
  constexpr std::uint32_t height = 28;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 13; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 22, 37, 27);
  fillRect(layers[1], 3, 22, 37, 27);
  for (int layer = 2; layer <= 3; ++layer) {
    addSquare(layers[layer], 10, 22, 1);
    addSquare(layers[layer], 30, 22, 1);
  }
  addSquare(layers[4], 10, 22, 0);
  addSquare(layers[4], 30, 22, 1);

  // The model appears on the left after a real tapered contact while the right
  // support remains an independent raft-rooted branch.
  for (int layer = 5; layer <= 12; ++layer) {
    fillRect(layers[layer], 4, 6, 18, 22);
  }
  for (int layer = 5; layer <= 9; ++layer) {
    addSquare(layers[layer], 30, 22 - static_cast<std::uint32_t>(layer - 5), 1);
  }
  // Narrow terminal head, then two persistent model-contact layers.
  addSquare(layers[10], 30, 16, 0);
  fillRect(layers[11], 24, 5, 36, 17);
  fillRect(layers[12], 23, 4, 37, 17);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeGradualMixedContactScene() {
  constexpr std::uint32_t width = 48;
  constexpr std::uint32_t height = 36;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 13; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 30, 45, 35);
  fillRect(layers[1], 3, 30, 45, 35);

  // Establish an independent model island before the gradual contact on the
  // right. Its presence elsewhere must not alter the local branch decision.
  for (int layer = 2; layer <= 3; ++layer) {
    addSquare(layers[layer], 10, 30, 1);
  }
  addSquare(layers[4], 10, 29, 0);
  for (int layer = 5; layer < 13; ++layer) {
    fillRect(layers[layer], 3, 5, 17, 30);
  }

  // Wide support body followed by a terminal reduction.
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 32, 30, 2);
  }
  addSquare(layers[6], 32, 29, 1);
  fillRect(layers[7], 31, 27, 34, 29); // 6-pixel terminal support section.

  // First real part section starts at layer 8. Every individual increase is
  // smaller than 20%, so a per-layer threshold would keep it as support. The
  // persistent cumulative growth across following layers confirms the contact
  // and moves the boundary retroactively back to layer 8.
  fillRect(layers[8], 32, 27, 39, 28); // 7 pixels: +16.7%.
  fillRect(layers[9], 35, 27, 43, 28); // 8 pixels: +14.3%.
  fillRect(layers[10], 38, 27, 47, 28); // 9 pixels: +12.5%.
  fillRect(layers[11], 36, 26, 47, 29);
  fillRect(layers[12], 34, 25, 47, 30);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeTranslatedTaperContinuationScene() {
  constexpr std::uint32_t width = 80;
  constexpr std::uint32_t height = 32;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 14; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 2, 26, 78, 31);
  fillRect(layers[1], 2, 26, 78, 31);

  // A raft-rooted support narrows persistently, then changes once from a
  // near-vertical segment to a straight inclined segment while its raster
  // section grows gradually. Width parity causes only a +/-0.5 pixel centre
  // quantization around the same +3 X / -1 Y direction. The support therefore
  // remains piecewise-linear rather than being accepted as a curved lineage.
  fillRect(layers[2], 20, 24, 40, 25); // 20 pixels.
  fillRect(layers[3], 21, 23, 39, 24); // 18 pixels.
  fillRect(layers[4], 22, 22, 38, 23); // 16 pixels.
  fillRect(layers[5], 23, 21, 37, 22); // 14 pixels.
  fillRect(layers[6], 27, 20, 39, 21); // 12 pixels; direction break.
  fillRect(layers[7], 29, 19, 42, 20); // 13 pixels.
  fillRect(layers[8], 32, 18, 46, 19); // 14 pixels.
  fillRect(layers[9], 34, 17, 49, 18); // 15 pixels.
  fillRect(layers[10], 37, 16, 53, 17); // 16 pixels.
  fillRect(layers[11], 39, 15, 56, 16); // 17 pixels.
  fillRect(layers[12], 3, 3, 10, 9);
  fillRect(layers[13], 3, 3, 10, 9);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeDelayedTerminalContactScene() {
  constexpr std::uint32_t width = 48;
  constexpr std::uint32_t height = 36;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 15; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 30, 45, 35);
  fillRect(layers[1], 3, 30, 45, 35);
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 32, 30, 2);
  }
  // A separate model island is already established before the terminal
  // sequence so the support evolves inside a generic mixed model/support phase.
  for (int layer = 5; layer < 15; ++layer) {
    fillRect(layers[layer], 2, 4, 12, 14);
  }
  addSquare(layers[6], 32, 28, 1);
  addSquare(layers[7], 32, 27, 0);

  // A first expansion after a narrow section can still be a terminal support
  // shape. It remains below the
  // confirmed-contact growth threshold and must be committed back to support,
  // not used as the retroactive start of model matter.
  layers[8].set(31, 26, true);
  layers[8].set(32, 26, true);
  layers[8].set(32, 27, true);
  layers[9].set(31, 25, true);
  layers[9].set(32, 25, true);
  layers[9].set(32, 26, true);
  layers[10].set(31, 24, true);
  layers[10].set(32, 24, true);
  layers[10].set(32, 25, true);
  layers[11].set(31, 23, true);
  layers[11].set(32, 23, true);
  layers[11].set(32, 24, true);

  // The actual part starts only after the terminal support sequence. The next
  // layer confirms that this larger connected matter persists; the boundary is
  // then positioned retroactively at layer 12 (zero-based index).
  fillRect(layers[12], 24, 8, 41, 24);
  fillRect(layers[13], 23, 7, 42, 24);
  fillRect(layers[14], 22, 6, 43, 24);
  return layers;
}


std::vector<accloud::photons::BinaryMask> makeStaleTaperPlateauScene() {
  constexpr std::uint32_t width = 64;
  constexpr std::uint32_t height = 36;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 14; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 30, 61, 35);
  fillRect(layers[1], 3, 30, 61, 35);

  // A support arm grows laterally, then returns abruptly to its regular pillar
  // section and stays stable. The old large section must not be reused as a
  // terminal taper after that stable plateau.
  fillRect(layers[2], 24, 27, 36, 28); // 12
  fillRect(layers[3], 23, 26, 37, 27); // 14
  fillRect(layers[4], 22, 25, 38, 26); // 16
  fillRect(layers[5], 21, 24, 39, 25); // 18
  fillRect(layers[6], 26, 23, 34, 24); // abrupt return to 8
  fillRect(layers[7], 26, 22, 34, 23); // stable 8
  fillRect(layers[8], 26, 21, 34, 22); // stable 8

  // A following support enlargement persists and exceeds the old cumulative
  // growth threshold. It remains support because the parent had only one old
  // abrupt decrease, not a real multi-step terminal taper.
  fillRect(layers[9], 24, 20, 37, 21);  // 13
  fillRect(layers[10], 22, 19, 40, 20); // 18
  fillRect(layers[11], 20, 18, 41, 19); // 21
  fillRect(layers[12], 3, 3, 10, 9);
  fillRect(layers[13], 3, 3, 10, 9);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeSupportJunctionScene() {
  constexpr std::uint32_t width = 56;
  constexpr std::uint32_t height = 36;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 10; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 30, 53, 35);
  fillRect(layers[1], 3, 30, 53, 35);
  for (int layer = 2; layer <= 6; ++layer) {
    addSquare(layers[layer], 20, 27 - static_cast<std::uint32_t>(layer - 2), 2);
    addSquare(layers[layer], 31, 27 - static_cast<std::uint32_t>(layer - 2), 2);
  }
  // Two independent support heads enlarge until they become one connected
  // structural junction. This is still support topology; the size of the
  // merged head must not be interpreted as model evidence.
  fillRect(layers[7], 18, 21, 27, 26);
  fillRect(layers[7], 26, 21, 34, 26);
  fillRect(layers[8], 22, 20, 31, 25);
  fillRect(layers[9], 23, 19, 30, 24);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeBoundingBoxHoleScene() {
  constexpr std::uint32_t width = 64;
  constexpr std::uint32_t height = 44;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 11; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 38, 61, 43);
  fillRect(layers[1], 3, 38, 61, 43);
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 10, 37, 1);
    addSquare(layers[layer], 32, 37, 1);
  }
  addSquare(layers[5], 10, 36, 0);
  addSquare(layers[5], 32, 35, 1);
  for (int layer = 6; layer < 11; ++layer) {
    fillRect(layers[layer], 4, 7, 17, 36); // established model island
  }
  addSquare(layers[6], 32, 33, 1);
  addSquare(layers[7], 32, 31, 1);
  addSquare(layers[8], 32, 29, 1);
  addSquare(layers[9], 32, 27, 1);
  addSquare(layers[10], 32, 25, 1);

  // A large model ring has a bounding box around the support but no material
  // within the layer-motion tolerance. Bounding-box overlap alone must not
  // create a support parent for this component.
  for (int layer = 8; layer < 11; ++layer) {
    fillRect(layers[layer], 21, 10, 44, 13);
    fillRect(layers[layer], 21, 32, 44, 35);
    fillRect(layers[layer], 21, 10, 24, 35);
    fillRect(layers[layer], 41, 10, 44, 35);
  }
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeModelRootedSupportScene() {
  constexpr std::uint32_t width = 44;
  constexpr std::uint32_t height = 32;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 14; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 25, 41, 31);
  fillRect(layers[1], 3, 25, 41, 31);
  for (int layer = 2; layer <= 3; ++layer) {
    addSquare(layers[layer], 10, 25, 1);
  }
  addSquare(layers[4], 10, 25, 0);

  // First model island reached by the tapered lower support and persisted long
  // enough to confirm the contact before a new support starts on it.
  for (int layer = 5; layer < 14; ++layer) {
    fillRect(layers[layer], 4, 8, 16, 25);
  }
  // A new support starts on that established model with its smallest section.
  addSquare(layers[7], 17, 18, 0);
  addSquare(layers[8], 19, 17, 1);
  addSquare(layers[9], 21, 16, 1);
  addSquare(layers[10], 23, 15, 1);
  addSquare(layers[11], 25, 14, 0);
  // Upper model island contacted by the narrowed head and confirmed next layer.
  fillRect(layers[12], 24, 4, 39, 14);
  fillRect(layers[13], 23, 3, 40, 14);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeUntaperedModelRootScene() {
  constexpr std::uint32_t width = 44;
  constexpr std::uint32_t height = 32;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 13; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 25, 41, 31);
  fillRect(layers[1], 3, 25, 41, 31);
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 10, 25, 1);
  }
  for (int layer = 5; layer < 13; ++layer) {
    fillRect(layers[layer], 4, 8, 16, 25);
  }
  // This protrusion starts with its maximum section. It may taper at the
  // upper contact, but it is a model feature rather than a support born on
  // the part because the required narrow root is absent.
  for (int layer = 6; layer <= 9; ++layer) {
    addSquare(layers[layer], 20 + static_cast<std::uint32_t>(layer - 6), 18, 1);
  }
  addSquare(layers[10], 24, 18, 0);
  fillRect(layers[11], 23, 4, 39, 18);
  fillRect(layers[12], 22, 3, 40, 18);
  return layers;
}

enum class RaftVariant {
  Plate,
  Grid,
  Pads,
};

void addRaftVariant(
    accloud::photons::BinaryMask& mask,
    RaftVariant variant) {
  switch (variant) {
  case RaftVariant::Plate:
    fillRect(mask, 4, 38, 76, 47);
    break;
  case RaftVariant::Grid:
    fillRect(mask, 4, 37, 76, 40);
    fillRect(mask, 4, 44, 76, 47);
    fillRect(mask, 10, 35, 13, 47);
    fillRect(mask, 34, 35, 37, 47);
    fillRect(mask, 58, 35, 61, 47);
    break;
  case RaftVariant::Pads:
    addSquare(mask, 12, 42, 4);
    addSquare(mask, 30, 42, 4);
    addSquare(mask, 50, 42, 4);
    addSquare(mask, 68, 42, 4);
    break;
  }
}

std::vector<accloud::photons::BinaryMask> makeRaftVariantScene(
    RaftVariant variant) {
  constexpr std::uint32_t width = 80;
  constexpr std::uint32_t height = 48;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 12; ++index) {
    layers.emplace_back(width, height);
  }
  for (int layer = 0; layer < 3; ++layer) {
    addRaftVariant(layers[layer], variant);
  }
  // Raft geometry may evolve inside its physical height. Deliberately alter
  // the second and third native layers enough that first-layer similarity
  // cannot be used as the semantic boundary.
  layers[1].set(1, 1, true);
  fillRect(layers[2], 2, 34, 7, 47);

  for (int layer = 3; layer <= 6; ++layer) {
    addSquare(layers[layer], 40, 39, 2);
  }
  addSquare(layers[7], 40, 38, 1);
  addSquare(layers[8], 40, 37, 0);
  fillRect(layers[9], 39, 35, 41, 37);
  addSquare(layers[10], 40, 35, 1);
  addSquare(layers[11], 40, 34, 2);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeMandatoryRaftSupportProvenanceScene() {
  constexpr std::uint32_t width = 128;
  constexpr std::uint32_t height = 72;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 6; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 2, 62, 126, 71);
  fillRect(layers[1], 2, 62, 126, 71);

  // Every disconnected component on the first native layer above the raft is
  // a mandatory support-provenance root, regardless of its size.
  addSquare(layers[2], 22, 56, 2);
  addSquare(layers[2], 68, 56, 8);

  // Both continuations inherit raft provenance. A third island appears later
  // without any geometric path to the raft and must not inherit that evidence.
  addSquare(layers[3], 23, 54, 6);
  addSquare(layers[3], 69, 54, 9);
  addSquare(layers[3], 108, 40, 2);

  addSquare(layers[4], 24, 52, 8);
  addSquare(layers[4], 70, 52, 10);
  addSquare(layers[4], 108, 37, 2);

  addSquare(layers[5], 25, 50, 8);
  addSquare(layers[5], 71, 50, 10);
  addSquare(layers[5], 108, 34, 2);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeEarlyDetachedLocalMaximumScene() {
  constexpr std::uint32_t width = 72;
  constexpr std::uint32_t height = 52;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 12; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 46, 69, 51);
  fillRect(layers[1], 3, 46, 69, 51);

  // Main raft-rooted support; its later contact establishes the conventional
  // part well above the detached early island below.
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 48, 44, 2);
  }
  addSquare(layers[6], 48, 42, 1);
  addSquare(layers[7], 48, 40, 0);
  for (int layer = 8; layer < 12; ++layer) {
    fillRect(layers[layer], 36, 12, 62, 41);
  }

  // This matter appears only on layers 3-4, has no path to the first post-raft
  // support roots and ends before the main model is established. P5 must still
  // open a reverse-model root on its topological Z maximum at layer 4.
  addSquare(layers[3], 10, 24, 1);
  addSquare(layers[4], 10, 23, 1);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeHeavySupportScene() {
  constexpr std::uint32_t width = 112;
  constexpr std::uint32_t height = 104;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 12; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 4, 94, 108, 103);
  fillRect(layers[1], 4, 94, 108, 103);
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 56, 80, 20);
  }
  addSquare(layers[6], 56, 77, 14);
  addSquare(layers[7], 56, 73, 8);
  addSquare(layers[8], 56, 69, 3);
  addSquare(layers[9], 56, 65, 0);
  addSquare(layers[10], 56, 64, 2);
  addSquare(layers[11], 56, 63, 3);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeSmallModelContactScene() {
  constexpr std::uint32_t width = 32;
  constexpr std::uint32_t height = 24;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 10; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 4, 19, 28, 23);
  fillRect(layers[1], 4, 19, 28, 23);
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 16, 18, 1);
  }
  addSquare(layers[6], 16, 17, 0);
  // The first part section is deliberately tiny. It remains below the former
  // absolute support-size thresholds but is still model matter after the tip.
  fillRect(layers[7], 15, 16, 17, 18);
  addSquare(layers[8], 16, 16, 1);
  addSquare(layers[9], 16, 15, 2);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeSecondaryMatchScene() {
  constexpr std::uint32_t width = 48;
  constexpr std::uint32_t height = 32;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 12; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 26, 45, 31);
  fillRect(layers[1], 3, 26, 45, 31);
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 20, 24, 2);
    addSquare(layers[layer], 25, 24, 1);
  }
  addSquare(layers[6], 20, 24, 2);
  addSquare(layers[6], 24, 24, 0);

  // This stable main pillar is close to both its real parent and the tapered
  // secondary branch. The secondary match is a brace candidate and must not
  // declare model matter on this layer.
  addSquare(layers[7], 20, 24, 2);
  addSquare(layers[8], 20, 23, 1);
  addSquare(layers[9], 20, 22, 0);
  fillRect(layers[10], 19, 21, 21, 23);
  addSquare(layers[11], 20, 21, 1);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makePrematureModelOverlapScene() {
  constexpr std::uint32_t width = 48;
  constexpr std::uint32_t height = 36;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 14; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 29, 45, 35);
  fillRect(layers[1], 3, 29, 45, 35);
  for (int layer = 2; layer <= 3; ++layer) {
    addSquare(layers[layer], 10, 29, 1);
    addSquare(layers[layer], 38, 29, 2);
  }
  addSquare(layers[4], 10, 29, 0);
  addSquare(layers[4], 38, 28, 2);

  // Establish model matter on the left through a confirmed tapered contact.
  for (int layer = 5; layer < 14; ++layer) {
    fillRect(layers[layer], 4, 7, 18, 29);
  }

  // The right support remains continuous while its section and inclination
  // evolve. A temporary model arm exists only on layer 7. On layer 8 the
  // support directly overlaps both its prior support section and that previous
  // model footprint. This overlap must not cut the established support branch.
  addSquare(layers[5], 37, 27, 2);
  addSquare(layers[6], 36, 25, 2);
  addSquare(layers[7], 34, 23, 2);
  fillRect(layers[7], 18, 15, 31, 20);
  addSquare(layers[8], 30, 19, 2);
  addSquare(layers[9], 29, 18, 2);
  addSquare(layers[10], 28, 17, 1);
  addSquare(layers[11], 27, 16, 0);

  // The real contact begins only after the terminal minimum and is confirmed
  // by the following layer.
  fillRect(layers[12], 25, 4, 39, 17);
  fillRect(layers[13], 24, 3, 40, 17);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeFreeSupportNearModelScene() {
  constexpr std::uint32_t width = 56;
  constexpr std::uint32_t height = 38;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 15; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 32, 53, 37);
  fillRect(layers[1], 3, 32, 53, 37);

  // Establish model matter on the left through a validated terminal contact.
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 10, 32, 1);
  }
  addSquare(layers[5], 10, 31, 0);
  fillRect(layers[6], 3, 8, 15, 31);
  fillRect(layers[7], 3, 8, 15, 31);

  // Let the already-classified model expand toward the support faster than the
  // stable pass-1 model mask can grow. On layer 9 the previous *component* is
  // close enough to mark near_previous_model, while the stable model envelope
  // still does not reach the support. This mirrors the curved Torus boundary:
  // pass 1 has no model projection in the support component, but pass 2 can see
  // model matter one layer above it.
  fillRect(layers[8], 3, 8, 18, 31);
  fillRect(layers[9], 3, 8, 19, 31);

  // Independent raft-rooted support. Layer 9 widens one native pixel toward the
  // nearby model while remaining physically separate from it.
  for (int layer = 2; layer <= 8; ++layer) {
    fillRect(layers[layer], 21, 20, 25, 33);
  }
  fillRect(layers[9], 20, 20, 25, 33);

  // Real contact starts on layer 10. The descending pass therefore reaches the
  // newly exposed layer-9 fringe, but that fringe must remain support because
  // pass 1 still owns the complete free support component on layer 9.
  for (int layer = 10; layer < 15; ++layer) {
    fillRect(layers[layer], 3, 8, 23, 31);
    fillRect(layers[layer], 20, 20, 25, 33);
  }
  return layers;
}

std::vector<accloud::photons::BinaryMask> makePersistentMixedSemanticScene() {
  constexpr std::uint32_t width = 52;
  constexpr std::uint32_t height = 38;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 15; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 32, 49, 37);
  fillRect(layers[1], 3, 32, 49, 37);

  // The left support establishes the model first.
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 10, 32, 1);
  }
  addSquare(layers[5], 10, 31, 0);
  for (int layer = 6; layer < 15; ++layer) {
    fillRect(layers[layer], 3, 8, 15, 31);
  }

  // A second raft-rooted support remains active beside the model.
  for (int layer = 2; layer < 14; ++layer) {
    addSquare(layers[layer], 34, 30 - static_cast<std::uint32_t>((layer - 2) / 3), 2);
  }

  // The model arm exists on two independent layers before it reaches the
  // support. It is therefore established model lineage, not a transient
  // one-layer overlap. From layer 10 onward both materials belong to the same
  // connected component and must retain their own semantics.
  fillRect(layers[8], 20, 25, 29, 28);
  fillRect(layers[9], 20, 25, 29, 28);
  for (int layer = 10; layer < 15; ++layer) {
    fillRect(layers[layer], 20, 25, 34, 28);
  }
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeTaperedSupportEstablishedModelMergeScene() {
  constexpr std::uint32_t width = 96;
  constexpr std::uint32_t height = 56;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 16; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 50, 93, 55);
  fillRect(layers[1], 3, 50, 93, 55);

  // Establish model matter independently on the left through a conventional
  // tapered contact before the support under test ever reaches it.
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 12, 49, 1);
  }
  addSquare(layers[5], 12, 48, 0);
  for (int layer = 6; layer < 16; ++layer) {
    fillRect(layers[layer], 4, 8, 24, 49);
  }

  // Independent raft-rooted support with a persistent terminal taper.
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 74, 47, 4);
  }
  for (int layer = 6; layer <= 7; ++layer) {
    addSquare(layers[layer], 74, 47, 3);
  }
  for (int layer = 8; layer <= 9; ++layer) {
    addSquare(layers[layer], 74, 47, 2);
  }

  // The already-established model approaches independently, then joins the
  // tapered support. On the first merged layer the global component becomes
  // very large while 20/25 pixels (80%) of the terminal support footprint are
  // still present. This must be reconciled as model + inherited support, not
  // as an all-model component merely because contact confirmation succeeds.
  fillRect(layers[8], 24, 20, 60, 24);
  fillRect(layers[9], 24, 20, 60, 24);
  for (int layer = 10; layer <= 12; ++layer) {
    fillRect(layers[layer], 24, 20, 76, 50);
  }

  // Once the model retracts from the former support footprint, no support
  // provenance should remain there.
  for (int layer = 13; layer < 16; ++layer) {
    fillRect(layers[layer], 24, 20, 60, 40);
  }
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeAbsorbedSupportEstablishedModelScene() {
  constexpr std::uint32_t width = 96;
  constexpr std::uint32_t height = 56;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 16; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 50, 93, 55);
  fillRect(layers[1], 3, 50, 93, 55);

  // Establish an independent model lineage on the left first.
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 12, 49, 1);
  }
  addSquare(layers[5], 12, 48, 0);
  for (int layer = 6; layer < 16; ++layer) {
    fillRect(layers[layer], 4, 8, 24, 49);
  }

  // A separate raft-rooted support tapers normally up to the last free layer.
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 74, 47, 4);
  }
  for (int layer = 6; layer <= 7; ++layer) {
    addSquare(layers[layer], 74, 47, 3);
  }
  for (int layer = 8; layer <= 9; ++layer) {
    addSquare(layers[layer], 74, 47, 2);
  }

  // From layer 10 upward, independently established model matter occupies the
  // complete former support-tip footprint and continues above it. The raw mask
  // alone therefore still contains those pixels, but pass 2 has persistent
  // model provenance on them. They must be absorbed by model semantics instead
  // of surviving as a frozen support island inside the part.
  fillRect(layers[8], 24, 20, 60, 24);
  fillRect(layers[9], 24, 20, 60, 24);
  for (int layer = 10; layer < 16; ++layer) {
    fillRect(layers[layer], 24, 20, 80, 52);
  }
  return layers;
}


std::vector<accloud::photons::BinaryMask> makeLocalModelOverhangScene() {
  constexpr std::uint32_t width = 96;
  constexpr std::uint32_t height = 56;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 16; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 50, 93, 55);
  fillRect(layers[1], 3, 50, 93, 55);

  // Establish model matter independently on the left through a conventional
  // tapered contact before the support under test ever reaches it.
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 12, 49, 1);
  }
  addSquare(layers[5], 12, 48, 0);
  for (int layer = 6; layer < 16; ++layer) {
    fillRect(layers[layer], 4, 8, 24, 49);
  }

  // Independent raft-rooted support with a persistent terminal taper.
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 74, 47, 4);
  }
  for (int layer = 6; layer <= 7; ++layer) {
    addSquare(layers[layer], 74, 47, 3);
  }
  for (int layer = 8; layer <= 9; ++layer) {
    addSquare(layers[layer], 74, 47, 2);
  }

  fillRect(layers[8], 24, 20, 60, 24);
  fillRect(layers[9], 24, 20, 60, 24);

  // Same established-model merge as the existing regression, except the
  // model's outer contour advances by two raw pixels per native layer. The
  // support core remains geometrically present inside the merged component.
  // Pass 2 therefore sees genuine newly appeared raw model matter on the
  // advancing local contour; its bounded local envelope must claim the nearby
  // support fringe on layer 10 without converting the complete support core.
  fillRect(layers[10], 24, 20, 76, 50);
  fillRect(layers[11], 24, 20, 74, 50);
  fillRect(layers[12], 24, 20, 72, 50);

  for (int layer = 13; layer < 16; ++layer) {
    fillRect(layers[layer], 24, 20, 60, 40);
  }
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeForwardModelCoreMergeScene() {
  constexpr std::uint32_t width = 64;
  constexpr std::uint32_t height = 42;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 16; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 36, 61, 41);
  fillRect(layers[1], 3, 36, 61, 41);

  // Establish model matter through a conventional tapered contact, but stop
  // the independent model column before the later support merge. The short arm
  // on layers 8-9 is therefore a stable forward model core with no path to the
  // global top of the print.
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 10, 36, 1);
  }
  addSquare(layers[5], 10, 35, 0);
  for (int layer = 6; layer <= 9; ++layer) {
    fillRect(layers[layer], 4, 8, 16, 35);
  }
  fillRect(layers[8], 16, 24, 29, 28);
  fillRect(layers[9], 16, 24, 29, 28);

  // A separate raft-rooted support exists before the merge and remains valid
  // afterwards.
  for (int layer = 2; layer <= 14; ++layer) {
    fillRect(layers[layer], 34, 24, 38, 37);
  }

  // From layer 10 onward the former model arm and the support are one raw
  // component. Pass 1 still has a stable model lineage on the arm, but pass 2
  // has no descending model path from above. The arm must not fall back to
  // support merely because the first merged component is support-rooted.
  for (int layer = 10; layer <= 14; ++layer) {
    fillRect(layers[layer], 20, 24, 38, 28);
  }

  // Unrelated final-layer matter keeps pass 2 rooted at the true top without
  // providing reverse-model evidence to the merged component.
  fillRect(layers[15], 48, 4, 58, 12);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeSupportSweepsAcrossModelScene() {
  constexpr std::uint32_t width = 72;
  constexpr std::uint32_t height = 46;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 16; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 40, 69, 45);
  fillRect(layers[1], 3, 40, 69, 45);

  // Establish model matter on the left through a conventional support contact.
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 10, 40, 1);
  }
  addSquare(layers[5], 10, 39, 0);
  for (int layer = 6; layer < 16; ++layer) {
    fillRect(layers[layer], 4, 8, 18, 39);
  }

  // A raft-rooted support remains independent on the right until the model
  // grows a stable arm into it.
  for (int layer = 2; layer <= 9; ++layer) {
    fillRect(layers[layer], 38, 22, 42, 41);
  }
  fillRect(layers[8], 18, 24, 34, 28);
  fillRect(layers[9], 18, 24, 34, 28);

  // First mixed layer: x=31 is established model while the support core is
  // still on x=38..42.
  fillRect(layers[10], 18, 24, 42, 28);
  fillRect(layers[10], 38, 22, 42, 41);

  // The model arm ends as an independent shape, but the raft-rooted support
  // continues through part of the exact footprint that was already model on
  // the preceding native layer. The raw matter is continuous there; support
  // provenance must not make those pixels revert from model to support.
  for (int layer = 11; layer < 16; ++layer) {
    fillRect(layers[layer], 34, 22, 38, 41);
  }
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeMovingMixedSemanticScene() {
  constexpr std::uint32_t width = 84;
  constexpr std::uint32_t height = 52;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 17; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 46, 81, 51);
  fillRect(layers[1], 3, 46, 81, 51);

  // Establish model matter independently on the left. The moving region below
  // is therefore an already-confirmed model lineage before it joins a support.
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 10, 46, 1);
  }
  addSquare(layers[5], 10, 45, 0);
  for (int layer = 6; layer < 17; ++layer) {
    fillRect(layers[layer], 4, 10, 16, 45);
  }

  // A second raft-rooted support footprint remains stationary while a stable
  // model island moves along its side. The model advances by four native
  // pixels per layer, so several consecutive positions have no exact XY
  // intersection. Once both regions share one component, the support footprint
  // must remain support and the independently established model lineage must
  // keep following its bounded motion.
  for (int layer = 2; layer < 16; ++layer) {
    fillRect(layers[layer], 40, 13, 43, 45);
  }
  fillRect(layers[8], 38, 5, 41, 9);
  fillRect(layers[9], 38, 5, 41, 9);
  for (int layer = 10; layer < 17; ++layer) {
    const auto firstY = 9u + static_cast<std::uint32_t>(layer - 10) * 4u;
    fillRect(layers[layer], 38, firstY, 41, firstY + 4u);
  }
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeFragmentedBitsetScene() {
  constexpr std::uint32_t width = 1536u;
  constexpr std::uint32_t height = 28u;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 9; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 2u, 23u, width - 2u, 27u);
  fillRect(layers[1], 2u, 23u, width - 2u, 27u);

  for (std::size_t layer = 2u; layer < layers.size(); ++layer) {
    for (std::uint32_t y = 5u; y < 22u; ++y) {
      if ((y % 2u) != 0u) {
        fillRect(layers[layer], 8u, y, width - 8u, y + 1u);
        continue;
      }
      for (std::uint32_t x = 12u; x + 2u < width - 12u; x += 9u) {
        fillRect(layers[layer], x, y, x + 2u, y + 1u);
      }
    }
  }
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeHollowModelScene() {
  constexpr std::uint32_t width = 40;
  constexpr std::uint32_t height = 40;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 10; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 5, 31, 35, 38);
  fillRect(layers[1], 5, 31, 35, 38);
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 12, 31, 1);
    addSquare(layers[layer], 28, 31, 1);
  }
  addSquare(layers[5], 12, 30, 0);
  addSquare(layers[5], 28, 30, 0);
  for (int layer = 6; layer < 10; ++layer) {
    // Thick hollow ring: it is model matter, not a support network.
    fillRect(layers[layer], 7, 7, 33, 31);
    fillRect(layers[layer], 12, 12, 28, 27);
    for (std::uint32_t y = 12; y < 27; ++y) {
      for (std::uint32_t x = 12; x < 28; ++x) {
        layers[layer].set(x, y, false);
      }
    }
  }
  return layers;
}


std::vector<accloud::photons::BinaryMask> makeRetroactiveSiblingScene() {
  constexpr std::uint32_t width = 48;
  constexpr std::uint32_t height = 32;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 12; ++index) {
    layers.emplace_back(width, height);
  }

  fillRect(layers[0], 3, 26, 45, 31);
  fillRect(layers[1], 3, 26, 45, 31);

  // One raft-rooted support body narrows to a real terminal tip.
  fillRect(layers[2], 16, 24, 27, 25); // 11
  fillRect(layers[3], 17, 23, 26, 24); // 9
  fillRect(layers[4], 18, 22, 25, 23); // 7
  fillRect(layers[5], 19, 21, 24, 22); // 5
  fillRect(layers[6], 20, 20, 23, 21); // 3-pixel tip

  // First growth opens a pending contact while remaining one support node.
  fillRect(layers[7], 15, 19, 25, 20); // 10

  // On the confirmation layer the same previous parent geometrically explains
  // two disconnected children. The left child grows enough to be retroactively
  // classified as model and is removed from currentCandidateNodes. The right
  // child is only a two-pixel support continuation and must therefore remain
  // the sole structural child of that parent.
  fillRect(layers[8], 8, 18, 21, 19);  // 13 -> confirmed model
  fillRect(layers[8], 23, 18, 25, 19); // 2 -> surviving support child

  // Persistent model above the confirmed left contact; unrelated top matter
  // keeps the descending model pass well-defined without touching the right
  // support child.
  fillRect(layers[9], 7, 8, 22, 19);
  fillRect(layers[10], 6, 7, 23, 19);
  fillRect(layers[11], 6, 7, 23, 19);
  return layers;
}

} // namespace

int main() {
  bool ok = true;
  accloud::render3d::SupportAnalyzer analyzer;

  auto retroactiveSiblingLayers = makeRetroactiveSiblingScene();
  VectorSource retroactiveSiblingSource(retroactiveSiblingLayers);
  const auto retroactiveSibling = analyzer.analyze(
      retroactiveSiblingSource, testOptions());
  ok &= require(retroactiveSibling.ok,
                "retroactive sibling analysis must succeed");
  const auto survivingSibling = std::find_if(
      retroactiveSibling.nodes.begin(), retroactiveSibling.nodes.end(),
      [](const auto& node) {
        return node.layer == 8u && node.areaPixels == 2u;
      });
  ok &= require(survivingSibling != retroactiveSibling.nodes.end(),
                "the surviving two-pixel support sibling must exist");
  if (survivingSibling != retroactiveSibling.nodes.end()) {
    const bool hasContinuation = std::any_of(
        retroactiveSibling.edges.begin(), retroactiveSibling.edges.end(),
        [&](const auto& edge) {
          return edge.upperNode == survivingSibling->id
                 && edge.kind == accloud::render3d::SupportEdgeKind::Continuation;
        });
    const bool hasSplit = std::any_of(
        retroactiveSibling.edges.begin(), retroactiveSibling.edges.end(),
        [&](const auto& edge) {
          return edge.upperNode == survivingSibling->id
                 && edge.kind == accloud::render3d::SupportEdgeKind::Split;
        });
    ok &= require(hasContinuation && !hasSplit,
                  "a sibling left after retroactive model removal must remain a continuation");
  }

  auto branchedLayers = makeBranchedSupportScene();
  VectorSource branchedSource(branchedLayers);
  const auto branched = analyzer.analyze(branchedSource, testOptions());
  ok &= require(branched.ok, "branched support graph analysis must succeed");
  ok &= require(branched.summary.raftLastLayer == 1,
                "the first two broad layers must remain the mandatory raft");
  ok &= require(branched.summary.acceptedNodeCount > 0,
                "a raft-rooted support tree with tapered heads must be accepted");
  ok &= require(branched.summary.splitEdgeCount > 0,
                "one support pillar must be allowed to split into several branches");
  ok &= require(countNodeKind(branched, accloud::render3d::SupportNodeKind::Head) >= 2,
                "each terminal narrowing before the part must expose support heads");
  ok &= require(structuralMergesAreExplicit(branched),
                "every structural merge must be represented explicitly by join topology");
  ok &= require(std::all_of(
                    branched.nodes.begin(), branched.nodes.end(),
                    [](const auto& node) {
                      return node.kind != accloud::render3d::SupportNodeKind::Head
                             || node.terminalTaper;
                    }),
                "every support head must carry a validated terminal taper");
  ok &= require(compactIndexIsCanonical(branched),
                "the compact semantic index must keep sorted unique component ids");
  ok &= require(forcedSamplesAreCanonical(branched)
                    && branched.summary.forcedSemanticSampleCount > 0,
                "terminal heads must expose sorted unique mandatory mesh samples");
  ok &= require(countSemanticRuns(branched, accloud::render3d::MaterialSemantic::Support) > 0,
                "accepted graph nodes must produce support semantic runs");
  ok &= require(countSemanticRuns(branched, accloud::render3d::MaterialSemantic::Raft) > 0,
                "the mandatory lower footprint must produce raft semantic runs");
  std::vector<accloud::render3d::SemanticRun> materialized;
  std::string materializeError;
  ok &= require(analyzer.materializeLayerSemantics(
                    branchedLayers[8], branched.layers[8], materialized, materializeError),
                "a compact semantic index must rematerialize one reconstruction layer");
  ok &= require(!materialized.empty()
                    && std::all_of(
                        materialized.begin(), materialized.end(),
                        [](const auto& run) {
                          return run.semantic
                                 == accloud::render3d::MaterialSemantic::Support;
                        }),
                "mixed-phase materialization must expose only classified support runs");
  std::vector<accloud::render3d::SemanticRun> contactLayerSemantics;
  ok &= require(analyzer.materializeLayerSemantics(
                    branchedLayers[10],
                    branched.layers[10],
                    contactLayerSemantics,
                    materializeError),
                "the first model-contact layer must rematerialize successfully");
  ok &= require(contactLayerSemantics.empty()
                    && branched.layers[10].projectedSupportRuns.empty(),
                "a larger model component must never inherit support semantics at contact");
  ok &= require(branched.summary.projectedSupportRunCount == 0u
                    && branched.summary.projectedContactPixelCount == 0u
                    && branched.summary.maximumContactGrowthRatio == 0.0,
                "support semantics must stop on the last free head layer");
  ok &= require(branched.summary.terminalSupportStopCount >= 2u
                    && branched.summary.expandingModelContactCount >= 2u
                    && branched.summary.maximumModelExpansionRatio > 1.0
                    && branched.summary.rejectedGrowthPixelCount > 0u,
                "small tapered heads followed by larger sections must terminate before the model");
  ok &= require(branched.summary.freeSupportRunCount > 0
                    && branched.summary.supportRunCount
                           == branched.summary.freeSupportRunCount,
                "support summaries must contain only free support matter");
  ok &= require(materializationMatchesSummary(analyzer, branchedLayers, branched),
                "all compact layer indices must rematerialize the recorded run totals");

  for (const auto variant : {RaftVariant::Plate, RaftVariant::Grid, RaftVariant::Pads}) {
    auto raftLayers = makeRaftVariantScene(variant);
    VectorSource raftSource(raftLayers);
    auto raftOptions = testOptions();
    raftOptions.raftHeightMillimetres = 0.3;
    const auto raftResult = analyzer.analyze(raftSource, raftOptions);
    ok &= require(raftResult.ok,
                  "plate, grid and pad raft variants must all analyze successfully");
    ok &= require(raftResult.summary.raftLastLayer == 2u
                      && raftResult.summary.firstModelLayer == 9u,
                  "the configured physical raft height must own the first three native layers despite XY shape changes");
  }

  auto provenanceLayers = makeMandatoryRaftSupportProvenanceScene();
  VectorSource provenanceSource(provenanceLayers);
  auto provenanceOptions = testOptions();
  provenanceOptions.captureDecisionTrace = true;
  const auto provenance = analyzer.analyze(provenanceSource, provenanceOptions);
  ok &= require(provenance.ok,
                "mandatory raft-support provenance analysis must succeed");
  ok &= require(
      provenance.layers[2].raftSupportProvenanceComponentIds.size() == 2u,
      "every component on the first native layer above the raft must seed support provenance");
  ok &= require(
      provenance.layers[3].raftSupportProvenanceComponentIds.size() == 2u
          && provenance.layers[4].raftSupportProvenanceComponentIds.size() == 2u,
      "raft support provenance must follow geometric continuity across large section changes without spreading to a detached island");
  const auto detachedProvenanceDecision = std::find_if(
      provenance.decisions.begin(), provenance.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 3u && decision.minX > 100u;
      });
  ok &= require(
      detachedProvenanceDecision != provenance.decisions.end()
          && !detachedProvenanceDecision->raftSupportProvenance,
      "matter born later without a geometric path to the raft must not inherit raft support provenance");

  auto earlyLocalMaximumLayers = makeEarlyDetachedLocalMaximumScene();
  VectorSource earlyLocalMaximumSource(earlyLocalMaximumLayers);
  auto earlyLocalMaximumOptions = testOptions();
  earlyLocalMaximumOptions.captureDecisionTrace = true;
  const auto earlyLocalMaximum = analyzer.analyze(
      earlyLocalMaximumSource, earlyLocalMaximumOptions);
  ok &= require(earlyLocalMaximum.ok,
                "early detached local-maximum analysis must succeed");
  const auto earlyLocalMaximumDecision = std::find_if(
      earlyLocalMaximum.decisions.begin(), earlyLocalMaximum.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 4u && decision.minX < 20u;
      });
  ok &= require(
      earlyLocalMaximumDecision != earlyLocalMaximum.decisions.end()
          && !earlyLocalMaximumDecision->raftSupportProvenance
          && earlyLocalMaximumDecision->reverseModelSeed
          && earlyLocalMaximumDecision->reverseModelLocalMaximumSeed
          && !earlyLocalMaximumDecision->reverseModelTopSeed
          && earlyLocalMaximumDecision->reverseModelEvidencePixels != 0u,
      "a detached local Z maximum must seed reverse-model provenance independently of the forward model floor and raft-support provenance");

  auto heavyLayers = makeHeavySupportScene();
  VectorSource heavySource(heavyLayers);
  auto heavyOptions = testOptions();
  heavyOptions.captureDecisionTrace = true;
  const auto heavy = analyzer.analyze(heavySource, heavyOptions);
  ok &= require(heavy.ok, "heavy support analysis must succeed");
  ok &= require(heavy.summary.firstModelLayer == 10u
                    && layerHasSupport(heavy.layers[2])
                    && !layerHasSupport(heavy.layers[10]),
                "a wide heavy support must remain support until its terminal tip reaches the model");
  ok &= require(
      layerHasRaftSupportProvenance(heavy.layers[10])
          && layerHasRaftSupportProvenance(heavy.layers[11]),
      "a provisional model/contact verdict must not erase the raft-rooted support provenance that geometrically reaches the same component");
  const auto heavyContactDecision = std::find_if(
      heavy.decisions.begin(), heavy.decisions.end(), [](const auto& decision) {
        return decision.layer == 10u;
      });
  ok &= require(
      heavyContactDecision != heavy.decisions.end()
          && heavyContactDecision->raftSupportProvenance,
      "decision diagnostics must retain raft support provenance across the legacy contact boundary");
  ok &= require(std::any_of(
                    heavy.nodes.begin(), heavy.nodes.end(),
                    [](const auto& node) {
                      return node.kind != accloud::render3d::SupportNodeKind::Rejected
                             && node.equivalentDiameterMillimetres > 4.0;
                    }),
                "support classification must not depend on the former four-millimetre diameter limit");

  auto smallModelLayers = makeSmallModelContactScene();
  VectorSource smallModelSource(smallModelLayers);
  const auto smallModel = analyzer.analyze(smallModelSource, testOptions());
  ok &= require(smallModel.ok, "small model-contact analysis must succeed");
  ok &= require(smallModel.summary.firstModelLayer == 7u
                    && smallModel.layers[7].supportComponentIds.empty(),
                "the first small part section after a tapered tip must be model matter immediately");

  auto secondaryMatchLayers = makeSecondaryMatchScene();
  VectorSource secondaryMatchSource(secondaryMatchLayers);
  const auto secondaryMatch = analyzer.analyze(
      secondaryMatchSource, testOptions());
  ok &= require(secondaryMatch.ok, "secondary-match analysis must succeed");
  ok &= require(secondaryMatch.summary.firstModelLayer == 10u
                    && layerHasSupport(secondaryMatch.layers[7]),
                "a tapered secondary brace match must not turn a continuing primary support into model");

  auto prematureOverlapLayers = makePrematureModelOverlapScene();
  VectorSource prematureOverlapSource(prematureOverlapLayers);
  const auto prematureOverlap = analyzer.analyze(
      prematureOverlapSource, testOptions());
  ok &= require(prematureOverlap.ok,
                "premature model-overlap regression analysis must succeed");
  ok &= require(prematureOverlap.summary.firstModelLayer == 5u
                    && layerHasSupport(prematureOverlap.layers[8])
                    && layerHasSupport(prematureOverlap.layers[11]),
                "a raft-rooted support must survive local model overlap and section changes");
  ok &= require(!layerHasSupport(prematureOverlap.layers[12])
                    && countNodeKind(
                           prematureOverlap,
                           accloud::render3d::SupportNodeKind::Head) >= 2u,
                "the support-to-model boundary must move to the confirmed contact after the terminal tip");

  auto freeSupportNearModelLayers = makeFreeSupportNearModelScene();
  VectorSource freeSupportNearModelSource(freeSupportNearModelLayers);
  auto freeSupportNearModelOptions = testOptions();
  freeSupportNearModelOptions.captureDecisionTrace = true;
  const auto freeSupportNearModel = analyzer.analyze(
      freeSupportNearModelSource, freeSupportNearModelOptions);
  ok &= require(freeSupportNearModel.ok,
                "free support near model analysis must succeed");
  ok &= require(
      pixelHasSemantic(
          analyzer,
          freeSupportNearModelLayers[9],
          freeSupportNearModel.layers[9],
          20u,
          24u,
          accloud::render3d::MaterialSemantic::Support),
      "new pixels of a free raft-rooted support must not become descending model evidence before contact");
  const auto freeSupportNode = std::find_if(
      freeSupportNearModel.nodes.begin(), freeSupportNearModel.nodes.end(),
      [](const auto& node) {
        return node.layer == 9u && node.rootedInRaft && node.areaPixels == 65u;
      });
  ok &= require(freeSupportNode != freeSupportNearModel.nodes.end(),
                "the widened free support component must exist on the pre-contact layer");
  if (freeSupportNode != freeSupportNearModel.nodes.end()) {
    const auto freeSupportDecision = std::find_if(
        freeSupportNearModel.decisions.begin(), freeSupportNearModel.decisions.end(),
        [&](const auto& decision) {
          return decision.nodeId == freeSupportNode->id;
        });
    ok &= require(
        freeSupportDecision != freeSupportNearModel.decisions.end()
            && !freeSupportDecision->contactCandidate
            && !freeSupportDecision->contactConfirmed
            && !freeSupportDecision->modelLineageContinued
            && freeSupportDecision->reverseModelLineageContinued
            && freeSupportDecision->reverseModelEvidencePixels != 0u
            && freeSupportDecision->reverseModelConflictDeferred
            && freeSupportDecision->finalModelPixels == 0u
            && freeSupportDecision->finalSupportPixels
                   == freeSupportDecision->currentAreaPixels,
        "P5 reverse provenance may cross a whole-support barrier, but the new conflict must stay deferred without consuming the protected support partition");
  }
  const auto topLayerReverseSeed = std::find_if(
      freeSupportNearModel.decisions.begin(), freeSupportNearModel.decisions.end(),
      [&](const auto& decision) {
        return decision.layer + 1u == freeSupportNearModelLayers.size()
               && decision.reverseModelSeed;
      });
  ok &= require(
      topLayerReverseSeed != freeSupportNearModel.decisions.end(),
      "pass 2 must start on the same final native layer processed by pass 1");

  auto persistentMixedLayers = makePersistentMixedSemanticScene();
  VectorSource persistentMixedSource(persistentMixedLayers);
  const auto persistentMixed = analyzer.analyze(
      persistentMixedSource, testOptions());
  ok &= require(persistentMixed.ok,
                "persistent mixed-semantic analysis must succeed");
  for (std::size_t layer = 10; layer + 1u < persistentMixedLayers.size(); ++layer) {
    ok &= require(
        pixelHasSemantic(
            analyzer, persistentMixedLayers[layer], persistentMixed.layers[layer],
            24u, 26u, accloud::render3d::MaterialSemantic::Model),
        "established model lineage must remain model through a mixed component");
    ok &= require(
        pixelHasSemantic(
            analyzer, persistentMixedLayers[layer], persistentMixed.layers[layer],
            34u, 27u, accloud::render3d::MaterialSemantic::Support),
        "a raft-rooted support must remain support inside the same mixed component");
  }
  ok &= require(
      persistentMixed.summary.projectedSupportRunCount > 0u,
      "mixed components must use partial support runs instead of a whole-component decision");

  auto taperedEstablishedMergeLayers = makeTaperedSupportEstablishedModelMergeScene();
  VectorSource taperedEstablishedMergeSource(taperedEstablishedMergeLayers);
  auto taperedEstablishedMergeOptions = testOptions();
  taperedEstablishedMergeOptions.captureDecisionTrace = true;
  const auto taperedEstablishedMerge = analyzer.analyze(
      taperedEstablishedMergeSource, taperedEstablishedMergeOptions);
  ok &= require(taperedEstablishedMerge.ok,
                "tapered support / established-model merge analysis must succeed");
  for (std::size_t layer = 10u; layer <= 12u; ++layer) {
    ok &= require(
        pixelHasSemantic(
            analyzer, taperedEstablishedMergeLayers[layer],
            taperedEstablishedMerge.layers[layer],
            74u, 47u, accloud::render3d::MaterialSemantic::Support),
        "a tapered raft-rooted support core must survive contact confirmation when it merges into independently established model matter");
    ok &= require(
        pixelHasSemantic(
            analyzer, taperedEstablishedMergeLayers[layer],
            taperedEstablishedMerge.layers[layer],
            30u, 22u, accloud::render3d::MaterialSemantic::Model),
        "independently established model matter must remain model in the same merged component");
    ok &= require(
        !taperedEstablishedMerge.layers[layer].projectedSupportRuns.empty(),
        "an established-model merge must retain an explicit partial support projection while inherited support pixels survive");
  }
  ok &= require(
      !layerHasSupport(taperedEstablishedMerge.layers[13]),
      "support provenance must disappear once no inherited support footprint survives geometrically");
  const auto taperedEstablishedMergeDecision = std::find_if(
      taperedEstablishedMerge.decisions.begin(),
      taperedEstablishedMerge.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 10u && decision.contactConfirmed;
      });
  ok &= require(
      taperedEstablishedMergeDecision != taperedEstablishedMerge.decisions.end(),
      "the regression fixture must actually exercise confirmed contact on the first established-model merge layer");
  auto absorbedSupportLayers = makeAbsorbedSupportEstablishedModelScene();
  VectorSource absorbedSupportSource(absorbedSupportLayers);
  auto absorbedSupportOptions = testOptions();
  absorbedSupportOptions.captureDecisionTrace = true;
  const auto absorbedSupport = analyzer.analyze(
      absorbedSupportSource, absorbedSupportOptions);
  ok &= require(absorbedSupport.ok,
                "absorbed support / established-model analysis must succeed");
  ok &= require(
      pixelHasSemantic(
          analyzer, absorbedSupportLayers[9], absorbedSupport.layers[9],
          74u, 47u, accloud::render3d::MaterialSemantic::Support),
      "the terminal footprint must still be support on its last free native layer");
  for (std::size_t layer = 10u; layer < 15u; ++layer) {
    ok &= require(
        pixelHasSemantic(
            analyzer, absorbedSupportLayers[layer], absorbedSupport.layers[layer],
            74u, 47u, accloud::render3d::MaterialSemantic::Model),
        "reverse model provenance must absorb a support footprint that is fully occupied by persistent model matter");
  }
  const auto absorbedSupportDecision = std::find_if(
      absorbedSupport.decisions.begin(), absorbedSupport.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 10u
               && decision.reverseSupportCorePixels != 0u;
      });
  ok &= require(
      absorbedSupportDecision != absorbedSupport.decisions.end()
          && absorbedSupportDecision->finalSupportPixels
                 < absorbedSupportDecision->reverseSupportCorePixels,
      "the absorbed-contact regression must exercise pixel-level conflict between forward support and reverse model evidence");


  auto localOverhangLayers = makeLocalModelOverhangScene();
  VectorSource localOverhangSource(localOverhangLayers);
  auto localOverhangOptions = testOptions();
  localOverhangOptions.captureDecisionTrace = true;
  const auto localOverhang = analyzer.analyze(
      localOverhangSource, localOverhangOptions);
  ok &= require(localOverhang.ok,
                "local model-overhang analysis must succeed");
  ok &= require(
      pixelHasSemantic(
          analyzer, localOverhangLayers[9], localOverhang.layers[9],
          74u, 47u, accloud::render3d::MaterialSemantic::Support),
      "the advancing model front must not retroactively consume the last free support layer");
  ok &= require(
      pixelHasSemantic(
          analyzer, localOverhangLayers[10], localOverhang.layers[10],
          75u, 47u, accloud::render3d::MaterialSemantic::Model),
      "an observed local model front must extend reverse-model ownership beyond exact upper-layer overlap across support pixels reached by the bounded overhang projection");
  const auto localOverhangDecision = std::find_if(
      localOverhang.decisions.begin(), localOverhang.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 10u
               && decision.overhangAbsorbedSupportPixels != 0u;
      });
  ok &= require(
      localOverhangDecision != localOverhang.decisions.end()
          && localOverhangDecision->finalSupportPixels != 0u,
      "local overhang projection must be recorded and remain partial when a contracting support is reached by a genuinely advancing model front");

  auto forwardModelCoreLayers = makeForwardModelCoreMergeScene();
  VectorSource forwardModelCoreSource(forwardModelCoreLayers);
  auto forwardModelCoreOptions = testOptions();
  forwardModelCoreOptions.captureDecisionTrace = true;
  const auto forwardModelCore = analyzer.analyze(
      forwardModelCoreSource, forwardModelCoreOptions);
  ok &= require(forwardModelCore.ok,
                "forward model-core merge analysis must succeed");
  for (std::size_t layer = 10u; layer <= 14u; ++layer) {
    ok &= require(
        pixelHasSemantic(
            analyzer, forwardModelCoreLayers[layer], forwardModelCore.layers[layer],
            24u, 26u, accloud::render3d::MaterialSemantic::Model),
        "a model core confirmed on preceding native layers must not fall back to support when it first merges with a raft-rooted support component");
    ok &= require(
        pixelHasSemantic(
            analyzer, forwardModelCoreLayers[layer], forwardModelCore.layers[layer],
            36u, 32u, accloud::render3d::MaterialSemantic::Support),
        "preserving a confirmed forward model core must not consume the independent raft-rooted support core in the same merged component");
  }
  const auto forwardModelCoreDecision = std::find_if(
      forwardModelCore.decisions.begin(), forwardModelCore.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 10u && decision.modelLineageContinued;
      });
  ok &= require(
      forwardModelCoreDecision != forwardModelCore.decisions.end(),
      "the model-core merge fixture must exercise forward model-lineage continuity on the first merged layer");
  const auto localMaximumSeedDecision = std::find_if(
      forwardModelCore.decisions.begin(), forwardModelCore.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 14u
               && decision.reverseModelSeed
               && decision.reverseModelLocalMaximumSeed;
      });
  ok &= require(
      localMaximumSeedDecision != forwardModelCore.decisions.end()
          && !localMaximumSeedDecision->reverseModelTopSeed
          && localMaximumSeedDecision->reverseModelEvidencePixels != 0u
          && localMaximumSeedDecision->finalSupportPixels != 0u
          && localMaximumSeedDecision->finalModelPixels != 0u,
      "a lower local Z maximum must seed independent reverse-model provenance even when pass 1 also carries raft-support evidence");
  ok &= require(
      forwardModelCoreDecision != forwardModelCore.decisions.end()
          && forwardModelCoreDecision->reverseModelLineageContinued
          && forwardModelCoreDecision->reverseModelEvidencePixels != 0u
          && forwardModelCoreDecision->reverseModelConflictDeferred
          && forwardModelCoreDecision->forwardModelCorePixels == 0u
          && forwardModelCoreDecision->finalSupportPixels != 0u
          && forwardModelCoreDecision->finalModelPixels != 0u,
      "the local-maximum reverse lineage must reach the first merged layer without consuming its independently preserved support core");
  ok &= require(
      forwardModelCore.summary.reverseModelTopSeedCount > 0u
          && forwardModelCore.summary.reverseModelLocalMaximumSeedCount > 0u
          && forwardModelCore.summary.reverseModelSeedCount
                 == forwardModelCore.summary.reverseModelTopSeedCount
                        + forwardModelCore.summary.reverseModelLocalMaximumSeedCount,
      "reverse-model seed diagnostics must distinguish mandatory top roots from lower topological local maxima");

  auto monotonicModelLayers = makeSupportSweepsAcrossModelScene();
  VectorSource monotonicModelSource(monotonicModelLayers);
  auto monotonicModelOptions = testOptions();
  monotonicModelOptions.captureDecisionTrace = true;
  const auto monotonicModel = analyzer.analyze(
      monotonicModelSource, monotonicModelOptions);
  ok &= require(monotonicModel.ok,
                "support-to-model monotonic lineage analysis must succeed");
  ok &= require(
      pixelHasSemantic(
          analyzer, monotonicModelLayers[10], monotonicModel.layers[10],
          36u, 26u, accloud::render3d::MaterialSemantic::Model),
      "the fixture must establish model ownership before the raft-rooted support sweeps across it");
  for (std::size_t layer = 11u; layer < monotonicModelLayers.size(); ++layer) {
    ok &= require(
        pixelHasSemantic(
            analyzer, monotonicModelLayers[layer], monotonicModel.layers[layer],
            36u, 26u, accloud::render3d::MaterialSemantic::Model),
        "a continuous model footprint must never revert to support on the same lineage");
  }
  auto movingMixedLayers = makeMovingMixedSemanticScene();
  VectorSource movingMixedSource(movingMixedLayers);
  auto movingMixedOptions = testOptions();
  movingMixedOptions.maximumLayerMotionPixels = 4.0;
  movingMixedOptions.captureDecisionTrace = true;
  const auto movingMixed = analyzer.analyze(
      movingMixedSource, movingMixedOptions);
  ok &= require(movingMixed.ok,
                "moving mixed-semantic analysis must succeed");
  for (std::size_t layer = 10; layer + 1u < movingMixedLayers.size(); ++layer) {
    const auto firstY = 9u + static_cast<std::uint32_t>(layer - 10u) * 4u;
    ok &= require(
        pixelHasSemantic(
            analyzer, movingMixedLayers[layer], movingMixed.layers[layer],
            39u, firstY + 1u, accloud::render3d::MaterialSemantic::Model),
        "a confirmed model lineage must follow bounded native-layer motion inside a mixed component");
    ok &= require(
        pixelHasSemantic(
            analyzer, movingMixedLayers[layer], movingMixed.layers[layer],
            42u, std::max<std::uint32_t>(14u, firstY + 1u), accloud::render3d::MaterialSemantic::Support),
        "the predicted raft-rooted support footprint must remain support beside moving model matter");
    ok &= require(
        !movingMixed.layers[layer].projectedSupportRuns.empty(),
        "a moving mixed component must keep an explicit partial support projection");
  }
  const auto movingMixedDecision = std::find_if(
      movingMixed.decisions.begin(), movingMixed.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 10u
               && decision.mixedSemanticProjection;
      });
  ok &= require(
      movingMixedDecision != movingMixed.decisions.end()
          && movingMixedDecision->modelLineageContinued
          && movingMixedDecision->modelLineageOverlapPixels > 0u
          && std::abs(movingMixedDecision->modelLineageShiftXPixels) <= 4.0
          && std::abs(movingMixedDecision->modelLineageShiftYPixels) <= 4.0,
      "mixed-semantic diagnostics must expose the bounded model-lineage motion used for the partition");

  auto pitchLowOptions = testOptions();
  pitchLowOptions.pitchZMillimetres = 0.01;
  pitchLowOptions.raftHeightMillimetres = 0.02;
  auto pitchNominalOptions = testOptions();
  pitchNominalOptions.pitchZMillimetres = 0.05;
  pitchNominalOptions.raftHeightMillimetres = 0.10;
  auto pitchHighOptions = testOptions();
  pitchHighOptions.pitchZMillimetres = 0.5;
  pitchHighOptions.raftHeightMillimetres = 1.0;
  VectorSource pitchLowSource(branchedLayers);
  VectorSource pitchNominalSource(branchedLayers);
  VectorSource pitchHighSource(branchedLayers);
  const auto pitchLow = analyzer.analyze(pitchLowSource, pitchLowOptions);
  const auto pitchNominal = analyzer.analyze(
      pitchNominalSource, pitchNominalOptions);
  const auto pitchHigh = analyzer.analyze(pitchHighSource, pitchHighOptions);
  ok &= require(sameSemanticClassification(pitchLow, pitchNominal)
                    && sameSemanticClassification(pitchNominal, pitchHigh),
                "equivalent physical raft configurations must preserve the same layer-native support semantics");

  auto fixedRaftFineOptions = testOptions();
  fixedRaftFineOptions.pitchZMillimetres = 0.05;
  fixedRaftFineOptions.raftHeightMillimetres = 0.2;
  auto fixedRaftCoarseOptions = testOptions();
  fixedRaftCoarseOptions.pitchZMillimetres = 0.1;
  fixedRaftCoarseOptions.raftHeightMillimetres = 0.2;
  VectorSource fixedRaftFineSource(branchedLayers);
  VectorSource fixedRaftCoarseSource(branchedLayers);
  const auto fixedRaftFine = analyzer.analyze(
      fixedRaftFineSource, fixedRaftFineOptions);
  const auto fixedRaftCoarse = analyzer.analyze(
      fixedRaftCoarseSource, fixedRaftCoarseOptions);
  ok &= require(fixedRaftFine.ok && fixedRaftCoarse.ok
                    && fixedRaftFine.summary.raftLastLayer == 3u
                    && fixedRaftCoarse.summary.raftLastLayer == 1u,
                "a fixed physical raft height must resolve to the native layer count through pitch Z");

  auto untaperedLayers = makeUntaperedContactScene();
  VectorSource untaperedSource(untaperedLayers);
  const auto untapered = analyzer.analyze(untaperedSource, testOptions());
  ok &= require(untapered.ok, "untapered contact analysis must succeed");
  ok &= require(untapered.summary.firstModelLayer == 8u
                    && untapered.summary.acceptedNodeCount > 0
                    && untapered.summary.lastSupportLayer == 7u,
                "an established raft-rooted branch must remain support without a terminal taper");
  ok &= require(countNodeKind(untapered, accloud::render3d::SupportNodeKind::Head) == 0
                    && untapered.summary.modelContactEdgeCount == 0u
                    && untapered.summary.projectedSupportRunCount == 0u,
                "an untapered area increase must not create a support-to-model contact");

  auto mixedLayers = makeMixedPhaseContinuationScene();
  VectorSource mixedSource(mixedLayers);
  const auto mixed = analyzer.analyze(mixedSource, testOptions());
  ok &= require(mixed.ok, "mixed-phase continuation analysis must succeed");
  bool supportPersistsInMixedPhase = false;
  for (std::size_t layer = mixed.summary.firstModelLayer;
       layer <= mixed.summary.lastSupportLayer && layer < mixed.layers.size(); ++layer) {
    supportPersistsInMixedPhase = supportPersistsInMixedPhase
                                  || layerHasSupport(mixed.layers[layer]);
  }
  ok &= require(supportPersistsInMixedPhase,
                "a raft-rooted branch must retain support semantics beside the model");
  ok &= require(countNodeKind(mixed, accloud::render3d::SupportNodeKind::Head) >= 1,
                "a narrowed mixed-phase branch must expose a terminal head");

  auto gradualContactLayers = makeGradualMixedContactScene();
  VectorSource gradualContactSource(gradualContactLayers);
  const auto gradualContact = analyzer.analyze(gradualContactSource, testOptions());
  ok &= require(gradualContact.ok,
                "gradual mixed contact analysis must succeed");
  ok &= require(gradualContact.summary.firstModelLayer == 5u
                    && layerHasSupport(gradualContact.layers[7])
                    && !layerHasSupport(gradualContact.layers[8])
                    && !layerHasSupport(gradualContact.layers[9])
                    && !layerHasSupport(gradualContact.layers[10]),
                "persistent cumulative growth must place the local boundary on the first part layer");
  ok &= require(std::binary_search(
                    gradualContact.forcedSampleLayers.begin(),
                    gradualContact.forcedSampleLayers.end(), 7u)
                    && std::binary_search(
                        gradualContact.forcedSampleLayers.begin(),
                        gradualContact.forcedSampleLayers.end(), 8u),
                "gradual contact must retain the support tip and first part layer as mandatory samples");

  auto translatedContinuationLayers = makeTranslatedTaperContinuationScene();
  VectorSource translatedContinuationSource(translatedContinuationLayers);
  auto translatedContinuationOptions = testOptions();
  translatedContinuationOptions.captureDecisionTrace = true;
  const auto translatedContinuation = analyzer.analyze(
      translatedContinuationSource, translatedContinuationOptions);
  ok &= require(translatedContinuation.ok,
                "translated tapered support analysis must succeed");
  ok &= require(translatedContinuation.summary.firstModelLayer == 12u
                    && translatedContinuation.summary.lastSupportLayer == 11u
                    && translatedContinuation.summary.modelContactEdgeCount == 0u,
                "a translated support profile must not become model after cumulative growth");
  const auto translatedDirectionBreak = std::find_if(
      translatedContinuation.decisions.begin(),
      translatedContinuation.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 6u && decision.componentId == 0u;
      });
  const auto translatedStraightSegment = std::find_if(
      translatedContinuation.decisions.begin(),
      translatedContinuation.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 10u && decision.componentId == 0u;
      });
  ok &= require(translatedDirectionBreak != translatedContinuation.decisions.end()
                    && translatedDirectionBreak->decision
                           == accloud::render3d::MaterialSemantic::Support
                    && translatedDirectionBreak->supportSegmentDirectionBreak
                    && translatedDirectionBreak->alignedOverlapRatio >= 0.85,
                "an inclined support must remain support when a real vector change starts a new straight segment");
  ok &= require(translatedStraightSegment != translatedContinuation.decisions.end()
                    && translatedStraightSegment->decision
                           == accloud::render3d::MaterialSemantic::Support
                    && translatedStraightSegment->supportSegmentLinear
                    && !translatedStraightSegment->supportSegmentDirectionBreak
                    && translatedStraightSegment->supportSegmentResidualPixels
                           <= translatedContinuationOptions.supportSegmentMaximumResidualPixels,
                "translated support motion after the direction break must follow a straight segment");

  auto staleTaperLayers = makeStaleTaperPlateauScene();
  VectorSource staleTaperSource(staleTaperLayers);
  auto staleTaperOptions = testOptions();
  staleTaperOptions.captureDecisionTrace = true;
  const auto staleTaper = analyzer.analyze(staleTaperSource, staleTaperOptions);
  ok &= require(staleTaper.ok, "stale taper plateau analysis must succeed");
  ok &= require(staleTaper.summary.firstModelLayer == 12u
                    && staleTaper.summary.modelContactEdgeCount == 0u
                    && staleTaper.summary.lastSupportLayer == 11u,
                "an old abrupt reduction followed by a stable pillar must not become a terminal contact");
  const auto staleExpansion = std::find_if(
      staleTaper.decisions.begin(), staleTaper.decisions.end(),
      [](const auto& decision) { return decision.layer == 9u; });
  ok &= require(staleExpansion != staleTaper.decisions.end()
                    && staleExpansion->decision
                           == accloud::render3d::MaterialSemantic::Support
                    && !staleExpansion->terminalTaperOnParent
                    && staleExpansion->terminalTaperDecreaseSteps == 1u,
                "the stale single-step reduction must remain an ordinary support continuation");

  auto piecewiseLayers = makePiecewiseLinearSupportScene();
  VectorSource piecewiseSource(piecewiseLayers);
  auto piecewiseOptions = testOptions();
  piecewiseOptions.captureDecisionTrace = true;
  piecewiseOptions.supportSegmentMaximumResidualPixels = 0.5;
  const auto piecewise = analyzer.analyze(piecewiseSource, piecewiseOptions);
  ok &= require(piecewise.ok, "piecewise-linear support analysis must succeed");
  const auto directionBreak = std::find_if(
      piecewise.decisions.begin(), piecewise.decisions.end(),
      [](const auto& decision) { return decision.layer == 6u; });
  const auto restartedSegment = std::find_if(
      piecewise.decisions.begin(), piecewise.decisions.end(),
      [](const auto& decision) { return decision.layer == 9u; });
  ok &= require(directionBreak != piecewise.decisions.end()
                    && directionBreak->decision
                           == accloud::render3d::MaterialSemantic::Support
                    && directionBreak->supportSegmentDirectionBreak,
                "a real support direction change must terminate the previous straight segment");
  ok &= require(restartedSegment != piecewise.decisions.end()
                    && restartedSegment->supportSegmentLinear
                    && !restartedSegment->supportSegmentDirectionBreak
                    && restartedSegment->supportSegmentResidualPixels
                           <= piecewiseOptions.supportSegmentMaximumResidualPixels,
                "motion after a direction break must be fitted as a new straight segment, not a curve");

  auto accumulatedBendLayers = makeAccumulatedBendSupportScene();
  VectorSource accumulatedBendSource(accumulatedBendLayers);
  auto accumulatedBendOptions = testOptions();
  accumulatedBendOptions.captureDecisionTrace = true;
  accumulatedBendOptions.supportSegmentMaximumResidualPixels = 1.0;
  const auto accumulatedBend = analyzer.analyze(
      accumulatedBendSource, accumulatedBendOptions);
  ok &= require(accumulatedBend.ok,
                "accumulated direction-change support analysis must succeed");
  const auto accumulatedBreak = std::find_if(
      accumulatedBend.decisions.begin(), accumulatedBend.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 9u && decision.componentId == 0u;
      });
  const auto accumulatedRestart = std::find_if(
      accumulatedBend.decisions.begin(), accumulatedBend.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 11u && decision.componentId == 0u;
      });
  ok &= require(accumulatedBreak != accumulatedBend.decisions.end()
                    && accumulatedBreak->decision
                           == accloud::render3d::MaterialSemantic::Support
                    && accumulatedBreak->supportSegmentDirectionBreak
                    && accumulatedBreak->supportSegmentResidualPixels
                           > accumulatedBendOptions.supportSegmentMaximumResidualPixels,
                "a gradually bending history must be cut once it no longer fits one straight segment");
  ok &= require(accumulatedRestart != accumulatedBend.decisions.end()
                    && accumulatedRestart->decision
                           == accloud::render3d::MaterialSemantic::Support
                    && accumulatedRestart->supportSegmentLinear
                    && !accumulatedRestart->supportSegmentDirectionBreak,
                "support motion after an accumulated bend must restart as a straight segment");

  auto braceLayers = makePhysicalBraceScene();
  VectorSource braceSource(braceLayers);
  auto braceOptions = testOptions();
  braceOptions.captureDecisionTrace = true;
  braceOptions.supportSegmentMaximumResidualPixels = 0.25;
  const auto physicalBrace = analyzer.analyze(braceSource, braceOptions);
  ok &= require(physicalBrace.ok, "physical-angle brace analysis must succeed");
  const auto braceDecision = std::find_if(
      physicalBrace.decisions.begin(), physicalBrace.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 10u && decision.braceAngleMatched;
      });
  ok &= require(physicalBrace.summary.braceEdgeCount > 0u
                    && braceDecision != physicalBrace.decisions.end()
                    && braceDecision->braceAngleMatched
                    && std::abs(braceDecision->braceAngleDegrees - 45.0) <= 1.0,
                "a straight support-to-support reinforcement near 45 degrees must become a brace edge");
  if (braceDecision != physicalBrace.decisions.end()) {
    const auto primaryBraceEdge = std::find_if(
        physicalBrace.edges.begin(), physicalBrace.edges.end(),
        [&](const auto& edge) {
          return edge.upperNode == braceDecision->nodeId
                 && edge.kind == accloud::render3d::SupportEdgeKind::Brace
                 && edge.lowerNode == braceDecision->parentNodeId;
        });
    ok &= require(primaryBraceEdge == physicalBrace.edges.end(),
                  "a brace lineage must never replace the primary structural parent");
  }

  VectorSource anisotropicBraceSource(braceLayers);
  auto anisotropicBraceOptions = braceOptions;
  anisotropicBraceOptions.pitchXMillimetres = 0.05;
  anisotropicBraceOptions.pitchZMillimetres = 0.1;
  const auto anisotropicBrace = analyzer.analyze(
      anisotropicBraceSource, anisotropicBraceOptions);
  ok &= require(anisotropicBrace.ok,
                "anisotropic physical-angle brace analysis must succeed");
  ok &= require(anisotropicBrace.summary.braceEdgeCount == 0u
                    && anisotropicBrace.summary.junctionEdgeCount > 0u,
                "brace classification must use physical XYZ angle rather than identical raster drift");

  auto junctionLayers = makeSupportJunctionScene();
  VectorSource junctionSource(junctionLayers);
  auto junctionOptions = testOptions();
  junctionOptions.captureDecisionTrace = true;
  // Keep the old fusion coverage gate deliberately impossible for this
  // fixture: topology must remain valid without it. The converging parents are
  // not 45-degree brace lineages and therefore remain structural joins.
  junctionOptions.braceAngleToleranceDegrees = 4.0;
  junctionOptions.minimumSupportFusionCoverageRatio = 0.99;
  const auto junction = analyzer.analyze(junctionSource, junctionOptions);
  ok &= require(junction.ok, "support junction analysis must succeed");
  const auto junctionDecision = std::find_if(
      junction.decisions.begin(), junction.decisions.end(),
      [](const auto& decision) { return decision.layer == 7u; });
  const auto junctionNode = std::find_if(
      junction.nodes.begin(), junction.nodes.end(),
      [](const auto& node) {
        return node.layer == 7u
               && node.kind == accloud::render3d::SupportNodeKind::Junction;
      });
  ok &= require(junctionDecision != junction.decisions.end()
                    && junctionDecision->decision
                           == accloud::render3d::MaterialSemantic::Support
                    && junctionDecision->reason
                           == accloud::render3d::SupportDecisionReason::SupportJunctionContinuation
                    && junctionDecision->supportJunctionContinuation
                    && junctionDecision->supportJunctionParentCount >= 2u
                    && !junctionDecision->supportFusionContinuation,
                "multiple support-provenance parents must remain support without the legacy fusion gate");
  ok &= require(junctionNode != junction.nodes.end()
                    && junction.summary.junctionEdgeCount > 0u
                    && countIncomingStructuralEdges(junction, junctionNode->id) >= 2u
                    && countIncomingJoinEdges(junction, junctionNode->id) >= 1u
                    && structuralMergesAreExplicit(junction),
                "support convergence must expose an explicit junction with multiple structural parents");

  auto holeLayers = makeBoundingBoxHoleScene();
  VectorSource holeSource(holeLayers);
  auto holeOptions = testOptions();
  holeOptions.captureDecisionTrace = true;
  const auto hole = analyzer.analyze(holeSource, holeOptions);
  ok &= require(hole.ok, "bounding-box hole analysis must succeed");
  const auto ringDecision = std::find_if(
      hole.decisions.begin(), hole.decisions.end(),
      [](const auto& decision) {
        return decision.layer == 8u && decision.currentAreaPixels > 200u;
      });
  ok &= require(ringDecision != hole.decisions.end()
                    && ringDecision->decision
                           == accloud::render3d::MaterialSemantic::Model
                    && ringDecision->parentNodeId
                           == std::numeric_limits<std::size_t>::max()
                    && ringDecision->matchedSupportParentCount == 0u,
                "a support inside a model bounding-box hole must not become its geometric parent");

  auto delayedContactLayers = makeDelayedTerminalContactScene();
  VectorSource delayedContactSource(delayedContactLayers);
  const auto delayedContact = analyzer.analyze(delayedContactSource, testOptions());
  ok &= require(delayedContact.ok,
                "delayed terminal-contact analysis must succeed");
  ok &= require(delayedContact.summary.firstModelLayer == 5u
                    && layerHasSupport(delayedContact.layers[8])
                    && layerHasSupport(delayedContact.layers[9])
                    && layerHasSupport(delayedContact.layers[10])
                    && layerHasSupport(delayedContact.layers[11]),
                "a provisional expansion must return to support while the terminal branch continues");
  ok &= require(!layerHasSupport(delayedContact.layers[12])
                    && delayedContact.layers[12].projectedSupportRuns.empty()
                    && delayedContact.summary.projectedContactPixelCount == 0u
                    && delayedContact.summary.terminalSupportStopCount == 1u
                    && delayedContact.summary.expandingModelContactCount == 1u
                    && delayedContact.summary.maximumModelExpansionRatio > 1.0,
                "the confirmed part must start only after the last terminal support layer");
  ok &= require(std::binary_search(
                    delayedContact.forcedSampleLayers.begin(),
                    delayedContact.forcedSampleLayers.end(), 11u)
                    && std::binary_search(
                        delayedContact.forcedSampleLayers.begin(),
                        delayedContact.forcedSampleLayers.end(), 12u),
                "the last terminal support layer and confirmed part layer must both be mandatory samples");

  auto modelRootedLayers = makeModelRootedSupportScene();
  VectorSource modelRootedSource(modelRootedLayers);
  const auto modelRooted = analyzer.analyze(modelRootedSource, testOptions());
  ok &= require(modelRooted.ok, "model-rooted support analysis must succeed");
  ok &= require(std::any_of(
                    modelRooted.nodes.begin(), modelRooted.nodes.end(),
                    [](const auto& node) {
                      return node.rootedInModel
                             && node.kind == accloud::render3d::SupportNodeKind::Head
                             && node.terminalTaper;
                    }),
                "a support starting on the model must begin independently and taper at contact");
  ok &= require(
      pixelHasSemantic(
          analyzer, modelRootedLayers[8], modelRooted.layers[8],
          19u, 17u, accloud::render3d::MaterialSemantic::Support),
      "a validated small branch born from established model must remain a new support lineage instead of inheriting model lock");
  ok &= require(
      pixelHasSemantic(
          analyzer, modelRootedLayers[8], modelRooted.layers[8],
          10u, 16u, accloud::render3d::MaterialSemantic::Model),
      "allowing a model-rooted support spawn must not turn its established model parent back into support");

  auto untaperedModelRootLayers = makeUntaperedModelRootScene();
  VectorSource untaperedModelRootSource(untaperedModelRootLayers);
  const auto untaperedModelRoot = analyzer.analyze(
      untaperedModelRootSource, testOptions());
  ok &= require(untaperedModelRoot.ok,
                "untapered model-root analysis must succeed");
  ok &= require(std::none_of(
                    untaperedModelRoot.nodes.begin(),
                    untaperedModelRoot.nodes.end(),
                    [](const auto& node) {
                      return node.rootedInModel
                             && node.kind == accloud::render3d::SupportNodeKind::Head;
                    }),
                "a model protrusion without a narrow root must never become a support");

  auto hollowLayers = makeHollowModelScene();
  VectorSource hollowSource(hollowLayers);
  const auto hollow = analyzer.analyze(hollowSource, testOptions());
  ok &= require(hollow.ok, "hollow model analysis must succeed");
  ok &= require(hollow.summary.acceptedNodeCount > 0,
                "external raft-rooted supports for a hollow part must be retained");
  const auto firstModel = hollow.summary.firstModelLayer;
  bool modelLayerContainsSupport = false;
  for (std::size_t layer = firstModel + 1; layer < hollow.layers.size(); ++layer) {
    modelLayerContainsSupport = modelLayerContainsSupport
                                || layerHasSupport(hollow.layers[layer]);
  }
  ok &= require(!modelLayerContainsSupport,
                "the closed hollow shell must not be reclassified as support");

  auto serialOptions = testOptions();
  serialOptions.workerCount = 1u;
  auto parallelOptions = testOptions();
  parallelOptions.workerCount = 4u;
  VectorSource serialWorkerSource(branchedLayers);
  VectorSource parallelWorkerSource(branchedLayers);
  const auto serialWorkers = analyzer.analyze(serialWorkerSource, serialOptions);
  const auto parallelWorkers = analyzer.analyze(parallelWorkerSource, parallelOptions);
  VectorSource parallelWorkerSourceRepeat(branchedLayers);
  const auto parallelWorkersRepeat = analyzer.analyze(
      parallelWorkerSourceRepeat, parallelOptions);
  ok &= require(serialWorkers.ok && parallelWorkers.ok && parallelWorkersRepeat.ok
                    && sameSemanticClassification(serialWorkers, parallelWorkers)
                    && sameSemanticClassification(parallelWorkers, parallelWorkersRepeat)
                    && serialWorkers.summary.supportSemanticEvidenceLotCount == 1u
                    && parallelWorkers.summary.supportSemanticEvidenceLotCount > 1u
                    && parallelWorkers.summary.supportSemanticEvidenceLayerPairCount > 0u
                    && parallelWorkers.summary.supportSemanticEvidenceEdgeCount > 0u,
                "parallel semantic-evidence lots must preserve deterministic support semantics while constructing adjacent-layer continuity facts independently");

  auto fragmentedBitsetLayers = makeFragmentedBitsetScene();
  auto bitsetOptions = parallelOptions;
  bitsetOptions.enableBitsetAcceleration = true;
  auto runOnlyOptions = parallelOptions;
  runOnlyOptions.enableBitsetAcceleration = false;
  VectorSource bitsetSource(fragmentedBitsetLayers);
  VectorSource runOnlySource(fragmentedBitsetLayers);
  const auto bitsetResult = analyzer.analyze(bitsetSource, bitsetOptions);
  const auto runOnlyResult = analyzer.analyze(runOnlySource, runOnlyOptions);
  ok &= require(bitsetResult.ok && runOnlyResult.ok
                    && sameSemanticClassification(bitsetResult, runOnlyResult),
                "the hybrid CPU bitset/SIMD path must preserve the exact run-only semantic result on heavily fragmented rows");

  auto cpuComputeOptions = parallelOptions;
  cpuComputeOptions.computePreference =
      accloud::render3d::compute::SupportComputePreference::Cpu;
  VectorSource cpuComputeSource(branchedLayers);
  bool computeStatusObserved = false;
  bool computeStatusActive = true;
  std::string computeStatusBackend;
  bool performanceTelemetryObserved = false;
  accloud::render3d::SupportAnalysisPerformanceTelemetry lastPerformanceTelemetry;
  std::size_t lastSupportProgressCompleted = 0u;
  std::size_t lastSupportProgressTotal = 0u;
  bool supportProgressMonotonic = true;
  accloud::render3d::SupportAnalysisCallbacks cpuComputeCallbacks;
  cpuComputeCallbacks.computeStatus = [&](bool,
                                          bool active,
                                          const std::string& backend,
                                          const std::string&,
                                          const std::string&) {
    computeStatusObserved = true;
    computeStatusActive = active;
    computeStatusBackend = backend;
  };
  cpuComputeCallbacks.performanceTelemetry = [&](const auto& telemetry) {
    performanceTelemetryObserved = true;
    lastPerformanceTelemetry = telemetry;
  };
  cpuComputeCallbacks.progress = [&](std::size_t completed, std::size_t total) {
    supportProgressMonotonic = supportProgressMonotonic
                               && completed >= lastSupportProgressCompleted
                               && completed <= total;
    lastSupportProgressCompleted = completed;
    lastSupportProgressTotal = total;
  };
  const auto cpuComputeResult = analyzer.analyze(
      cpuComputeSource, cpuComputeOptions, cpuComputeCallbacks);
  ok &= require(cpuComputeResult.ok
                    && !cpuComputeResult.summary.vulkanComputeActive
                    && cpuComputeResult.summary.vulkanDispatchCount == 0u
                    && computeStatusObserved
                    && !computeStatusActive
                    && computeStatusBackend == "cpu"
                    && performanceTelemetryObserved
                    && lastPerformanceTelemetry.preparedLayerCount
                           == branchedLayers.size()
                    && lastPerformanceTelemetry.preparationWindowCapacity == 4u
                    && lastPerformanceTelemetry.semanticEvidenceLotCount > 1u
                    && lastPerformanceTelemetry.semanticEvidenceLayerPairCount > 0u
                    && lastPerformanceTelemetry.semanticEvidenceEdgeCount > 0u
                    && cpuComputeResult.summary.supportPreparedLayerCount
                           == branchedLayers.size()
                    && cpuComputeResult.summary.supportPreparationWindowCapacity == 4u
                    && supportProgressMonotonic
                    && lastSupportProgressCompleted <= lastSupportProgressTotal
                    && lastSupportProgressTotal == branchedLayers.size() * 3u
                    && sameSemanticClassification(cpuComputeResult, parallelWorkers),
                "forcing the CPU compute backend must preserve support semantics, perform no Vulkan dispatch and publish compute/performance telemetry");

  auto hybridComputeOptions = parallelOptions;
  hybridComputeOptions.computePreference =
      accloud::render3d::compute::SupportComputePreference::Auto;
  // Lower the translated-lineage threshold so a local Vulkan-enabled gate can
  // exercise the hybrid backend even on this compact regression scene.
  hybridComputeOptions.vulkanMinimumComponentAreaPixels = 1u;
  VectorSource hybridComputeSource(branchedLayers);
  const auto hybridComputeResult = analyzer.analyze(
      hybridComputeSource, hybridComputeOptions);
  ok &= require(hybridComputeResult.ok
                    && sameSemanticClassification(
                        cpuComputeResult, hybridComputeResult)
                    && hybridComputeResult.summary.vulkanSemanticLayerBatchCallCount == 0u
                    && hybridComputeResult.summary.vulkanSemanticLayerBatchJobCount == 0u,
                "Auto/hybrid compute must preserve CPU support semantics and must never reactivate the retired P6.4 layer-wide semantic batch");

  auto monotonicCpuOptions = monotonicModelOptions;
  monotonicCpuOptions.computePreference =
      accloud::render3d::compute::SupportComputePreference::Cpu;
  auto monotonicHybridOptions = monotonicModelOptions;
  monotonicHybridOptions.computePreference =
      accloud::render3d::compute::SupportComputePreference::Auto;
  monotonicHybridOptions.vulkanMinimumComponentAreaPixels = 1u;
  VectorSource monotonicCpuSource(monotonicModelLayers);
  VectorSource monotonicHybridSource(monotonicModelLayers);
  const auto monotonicCpu = analyzer.analyze(
      monotonicCpuSource, monotonicCpuOptions);
  const auto monotonicHybrid = analyzer.analyze(
      monotonicHybridSource, monotonicHybridOptions);
  ok &= require(
      monotonicCpu.ok && monotonicHybrid.ok
          && sameSemanticClassification(monotonicCpu, monotonicHybrid),
      "CPU and Auto/hybrid must preserve the same monotonic MODEL-to-SUPPORT prohibition on continuous material");

  TrackingSource concurrentTrackingSource(branchedLayers, true);
  const auto concurrentTracking = analyzer.analyze(
      concurrentTrackingSource, parallelOptions);
  ok &= require(concurrentTracking.ok
                    && concurrentTrackingSource.maximumActiveLoads() >= 2u
                    && concurrentTrackingSource.maximumActiveLoads() <= 4u
                    && concurrentTrackingSource.totalLoads() == branchedLayers.size()
                    && concurrentTracking.summary.supportPreparationWindowCapacity == 4u
                    && concurrentTracking.summary.supportMaximumPreparationInflight <= 4u,
                "the adaptive preparation window must use the available four-worker pool without exceeding its worker bound");

  auto wideWorkerOptions = parallelOptions;
  wideWorkerOptions.workerCount = 16u;
  TrackingSource wideWorkerTrackingSource(branchedLayers, true);
  const auto wideWorkerTracking = analyzer.analyze(
      wideWorkerTrackingSource, wideWorkerOptions);
  ok &= require(wideWorkerTracking.ok
                    && wideWorkerTrackingSource.maximumActiveLoads() >= 5u
                    && wideWorkerTrackingSource.maximumActiveLoads()
                           <= branchedLayers.size()
                    && wideWorkerTrackingSource.totalLoads() == branchedLayers.size()
                    && wideWorkerTracking.summary.supportPreparationWindowCapacity
                           == branchedLayers.size()
                    && wideWorkerTracking.summary.supportMaximumPreparationInflight
                           == branchedLayers.size()
                    && sameSemanticClassification(
                        concurrentTracking, wideWorkerTracking),
                "a 16-worker run must widen preparation beyond four layers when the estimated mask budget permits it without changing semantics");

  auto budgetLimitedOptions = wideWorkerOptions;
  // 48x32 uses one 64-bit word per row => 256 bytes per native mask. A
  // 512-byte budget therefore permits exactly two outstanding preparations.
  budgetLimitedOptions.preparationMemoryBudgetBytes = 512u;
  TrackingSource budgetLimitedTrackingSource(branchedLayers, true);
  const auto budgetLimitedTracking = analyzer.analyze(
      budgetLimitedTrackingSource, budgetLimitedOptions);
  ok &= require(budgetLimitedTracking.ok
                    && budgetLimitedTrackingSource.maximumActiveLoads() <= 2u
                    && budgetLimitedTracking.summary.supportPreparationWindowCapacity == 2u
                    && budgetLimitedTracking.summary.supportMaximumPreparationInflight <= 2u
                    && sameSemanticClassification(
                        concurrentTracking, budgetLimitedTracking),
                "the adaptive preparation window must honor the native-mask memory budget independently of worker count");

  TrackingSource serializedTrackingSource(branchedLayers, false);
  const auto serializedTracking = analyzer.analyze(
      serializedTrackingSource, parallelOptions);
  ok &= require(serializedTracking.ok
                    && serializedTrackingSource.maximumActiveLoads() == 1u
                    && serializedTrackingSource.totalLoads() == branchedLayers.size()
                    && sameSemanticClassification(
                        concurrentTracking, serializedTracking),
                "non-concurrent mask sources must serialize one load per layer without changing semantics");

  accloud::photons::BinaryMask denseComponentMask(128u, 8u);
  constexpr std::size_t denseComponentCount = 24u;
  for (std::size_t component = 0u; component < denseComponentCount; ++component) {
    const auto firstX = static_cast<std::uint32_t>(2u + component * 5u);
    fillRect(denseComponentMask, firstX, 2u, firstX + 2u, 4u);
  }
  accloud::render3d::LayerSemanticIndex denseComponentIndex;
  denseComponentIndex.layer = 0u;
  denseComponentIndex.phase = accloud::render3d::PrintPhase::ModelAndSupports;
  for (std::size_t component = 1u; component < denseComponentCount; component += 2u) {
    denseComponentIndex.supportComponentIds.push_back(
        static_cast<std::uint32_t>(component));
  }
  std::vector<accloud::render3d::SemanticRun> denseComponentRuns;
  std::string denseComponentError;
  const bool denseComponentMaterialized = analyzer.materializeLayerSemantics(
      denseComponentMask, denseComponentIndex, denseComponentRuns, denseComponentError);
  bool denseComponentIdsStable = denseComponentMaterialized;
  for (std::size_t component = 0u;
       denseComponentIdsStable && component < denseComponentCount; ++component) {
    const auto x = static_cast<std::uint32_t>(2u + component * 5u);
    const bool emittedAsSupport = std::any_of(
        denseComponentRuns.begin(), denseComponentRuns.end(),
        [&](const auto& run) {
          return run.semantic == accloud::render3d::MaterialSemantic::Support
                 && run.y == 2u && x >= run.firstX && x < run.lastX;
        });
    denseComponentIdsStable = emittedAsSupport == ((component % 2u) != 0u);
  }
  ok &= require(
      denseComponentIdsStable,
      "dense connected-component indexing must preserve deterministic local component ids during semantic materialization");

  auto invalidWorkerOptions = testOptions();
  invalidWorkerOptions.workerCount = 0u;
  VectorSource invalidWorkerSource(branchedLayers);
  const auto invalidWorkers = analyzer.analyze(
      invalidWorkerSource, invalidWorkerOptions);
  ok &= require(!invalidWorkers.ok && !invalidWorkers.error.empty(),
                "support analysis must reject an invalid worker count");

  auto invalidBudgetOptions = testOptions();
  invalidBudgetOptions.preparationMemoryBudgetBytes = 0u;
  VectorSource invalidBudgetSource(branchedLayers);
  const auto invalidBudget = analyzer.analyze(
      invalidBudgetSource, invalidBudgetOptions);
  ok &= require(!invalidBudget.ok && !invalidBudget.error.empty(),
                "support analysis must reject a zero preparation memory budget");

  VectorSource deterministicSourceA(branchedLayers);
  VectorSource deterministicSourceB(branchedLayers);
  const auto first = analyzer.analyze(deterministicSourceA, testOptions());
  const auto second = analyzer.analyze(deterministicSourceB, testOptions());
  ok &= require(first.summary.acceptedNodeCount == second.summary.acceptedNodeCount
                    && first.summary.supportRunCount == second.summary.supportRunCount
                    && first.summary.braceEdgeCount == second.summary.braceEdgeCount
                    && first.summary.junctionEdgeCount == second.summary.junctionEdgeCount
                    && first.summary.projectedContactPixelCount
                           == second.summary.projectedContactPixelCount
                    && first.summary.rejectedGrowthPixelCount
                           == second.summary.rejectedGrowthPixelCount
                    && first.summary.maximumContactGrowthRatio
                           == second.summary.maximumContactGrowthRatio
                    && first.summary.terminalSupportStopCount
                           == second.summary.terminalSupportStopCount
                    && first.summary.expandingModelContactCount
                           == second.summary.expandingModelContactCount
                    && first.summary.maximumModelExpansionRatio
                           == second.summary.maximumModelExpansionRatio,
                "support graph analysis must be deterministic");

  std::vector<accloud::photons::BinaryMask> emptyRaftLayers;
  emptyRaftLayers.emplace_back(16, 16);
  emptyRaftLayers.emplace_back(16, 16);
  VectorSource emptyRaftSource(emptyRaftLayers);
  const auto emptyRaft = analyzer.analyze(emptyRaftSource, testOptions());
  ok &= require(!emptyRaft.ok && !emptyRaft.error.empty(),
                "support analysis must reject a print without raft matter on layer one");

  bool cancel = true;
  VectorSource cancelledSource(branchedLayers);
  const auto cancelled = analyzer.analyze(
      cancelledSource,
      testOptions(),
      accloud::render3d::SupportAnalysisCallbacks{
          [&]() { return cancel; },
          {},
      });
  ok &= require(cancelled.cancelled,
                "support graph analysis must honour cancellation before decoding");

  return ok ? 0 : 1;
}
