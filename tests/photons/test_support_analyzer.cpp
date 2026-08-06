#include "domain/photons/BinaryMask.h"
#include "domain/photons/LayerMaskSource.h"
#include "render3d/analysis/SupportAnalyzer.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
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

bool hasSingleStructuralParent(
    const accloud::render3d::SupportAnalysisResult& result) {
  std::vector<std::size_t> incoming(result.nodes.size(), 0u);
  for (const auto& edge : result.edges) {
    if (edge.kind != accloud::render3d::SupportEdgeKind::Continuation
        && edge.kind != accloud::render3d::SupportEdgeKind::Split) {
      continue;
    }
    if (edge.upperNode >= incoming.size()) {
      return false;
    }
    if (++incoming[edge.upperNode] > 1u) {
      return false;
    }
  }
  return true;
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
        return componentIdsCanonical && projectedCanonical;
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
      || a.braceEdgeCount != b.braceEdgeCount
      || a.modelContactEdgeCount != b.modelContactEdgeCount
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
  options.raftMaximumChangedPixelRatio = 0.001;
  options.maximumLayerMotionPixels = 2.2;
  options.braceMinimumDriftPixelsPerLayer = 0.55;
  options.braceMaximumDriftPixelsPerLayer = 1.65;
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

std::vector<accloud::photons::BinaryMask> makeUntaperedContactScene() {
  constexpr std::uint32_t width = 32;
  constexpr std::uint32_t height = 24;
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 8; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 4, 18, 28, 23);
  fillRect(layers[1], 4, 18, 28, 23);
  for (int layer = 2; layer <= 5; ++layer) {
    addSquare(layers[layer], 16, 18, 1);
  }
  fillRect(layers[6], 8, 6, 25, 19);
  fillRect(layers[7], 7, 5, 26, 19);
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
  // sequence, matching the mixed-phase context of Beetle layers 632-637.
  for (int layer = 5; layer < 15; ++layer) {
    fillRect(layers[layer], 2, 4, 12, 14);
  }
  addSquare(layers[6], 32, 28, 1);
  addSquare(layers[7], 32, 27, 0);

  // Regression modelled on Beetle layers 632-637: the first expansion after a
  // narrow section is still a terminal support shape. It remains below the
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
  // Real PWSZ raft masks can differ by a few antialias pixels while remaining
  // the repeated raft prefix. One isolated changed pixel must not end it.
  layers[1].set(1, 1, true);

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
  for (int layer = 2; layer < 15; ++layer) {
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

} // namespace

int main() {
  bool ok = true;
  accloud::render3d::SupportAnalyzer analyzer;

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
  ok &= require(branched.summary.braceEdgeCount > 0,
                "a diagonal support reinforcement must be represented as a brace edge");
  ok &= require(countNodeKind(branched, accloud::render3d::SupportNodeKind::Head) >= 2,
                "each terminal narrowing before the part must expose support heads");
  ok &= require(hasSingleStructuralParent(branched),
                "support branches may split but must never structurally merge");
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
    const auto raftResult = analyzer.analyze(raftSource, testOptions());
    ok &= require(raftResult.ok,
                  "plate, grid and pad raft variants must all analyze successfully");
    ok &= require(raftResult.summary.raftLastLayer == 2u
                      && raftResult.summary.firstModelLayer == 9u,
                  "the repeated prefix must remain raft and its next layer must start supports");
  }

  auto heavyLayers = makeHeavySupportScene();
  VectorSource heavySource(heavyLayers);
  const auto heavy = analyzer.analyze(heavySource, testOptions());
  ok &= require(heavy.ok, "heavy support analysis must succeed");
  ok &= require(heavy.summary.firstModelLayer == 10u
                    && layerHasSupport(heavy.layers[2])
                    && !layerHasSupport(heavy.layers[10]),
                "a wide heavy support must remain support until its terminal tip reaches the model");
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

  auto persistentMixedLayers = makePersistentMixedSemanticScene();
  VectorSource persistentMixedSource(persistentMixedLayers);
  const auto persistentMixed = analyzer.analyze(
      persistentMixedSource, testOptions());
  ok &= require(persistentMixed.ok,
                "persistent mixed-semantic analysis must succeed");
  for (std::size_t layer = 10; layer < persistentMixedLayers.size(); ++layer) {
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

  auto pitchLowOptions = testOptions();
  pitchLowOptions.pitchZMillimetres = 0.01;
  auto pitchNominalOptions = testOptions();
  pitchNominalOptions.pitchZMillimetres = 0.05;
  auto pitchHighOptions = testOptions();
  pitchHighOptions.pitchZMillimetres = 0.5;
  VectorSource pitchLowSource(branchedLayers);
  VectorSource pitchNominalSource(branchedLayers);
  VectorSource pitchHighSource(branchedLayers);
  const auto pitchLow = analyzer.analyze(pitchLowSource, pitchLowOptions);
  const auto pitchNominal = analyzer.analyze(
      pitchNominalSource, pitchNominalOptions);
  const auto pitchHigh = analyzer.analyze(pitchHighSource, pitchHighOptions);
  ok &= require(sameSemanticClassification(pitchLow, pitchNominal)
                    && sameSemanticClassification(pitchNominal, pitchHigh),
                "PWSZ layer height may change Z placement but not layer-native support semantics");

  auto untaperedLayers = makeUntaperedContactScene();
  VectorSource untaperedSource(untaperedLayers);
  const auto untapered = analyzer.analyze(untaperedSource, testOptions());
  ok &= require(untapered.ok, "untapered contact analysis must succeed");
  ok &= require(untapered.summary.firstModelLayer == 0u
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

  VectorSource deterministicSourceA(branchedLayers);
  VectorSource deterministicSourceB(branchedLayers);
  const auto first = analyzer.analyze(deterministicSourceA, testOptions());
  const auto second = analyzer.analyze(deterministicSourceB, testOptions());
  ok &= require(first.summary.acceptedNodeCount == second.summary.acceptedNodeCount
                    && first.summary.supportRunCount == second.summary.supportRunCount
                    && first.summary.braceEdgeCount == second.summary.braceEdgeCount
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
