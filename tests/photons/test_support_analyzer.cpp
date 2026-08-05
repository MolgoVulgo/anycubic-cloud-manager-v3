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
         || !layer.supportComponentIds.empty();
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
        return std::is_sorted(
                   layer.supportComponentIds.begin(),
                   layer.supportComponentIds.end())
               && std::adjacent_find(
                      layer.supportComponentIds.begin(),
                      layer.supportComponentIds.end())
                      == layer.supportComponentIds.end();
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

accloud::render3d::SupportAnalysisOptions testOptions() {
  accloud::render3d::SupportAnalysisOptions options;
  options.pitchXMillimetres = 0.1;
  options.pitchYMillimetres = 0.1;
  options.pitchZMillimetres = 0.1;
  options.maximumRaftLayers = 4;
  options.raftRetainedAreaRatio = 0.55;
  options.maximumSupportDiameterMillimetres = 0.65;
  options.maximumSupportAreaMillimetres2 = 0.22;
  options.minimumTrackLayers = 3;
  options.taperLookbackLayers = 6;
  options.maximumLayerSlope = 2.2;
  options.braceMinimumSlope = 0.55;
  options.braceMaximumSlope = 1.65;
  options.terminalTaperRatio = 0.72;
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
  for (int index = 0; index < 12; ++index) {
    layers.emplace_back(width, height);
  }
  fillRect(layers[0], 3, 22, 37, 27);
  fillRect(layers[1], 3, 22, 37, 27);
  for (int layer = 2; layer <= 4; ++layer) {
    addSquare(layers[layer], 10, 22, 1);
    addSquare(layers[layer], 30, 22, 1);
  }
  // The model appears on the left while the right support remains separate.
  for (int layer = 5; layer <= 9; ++layer) {
    fillRect(layers[layer], 4, 6, 18, 22);
    addSquare(layers[layer], 30, 22 - static_cast<std::uint32_t>(layer - 5), 1);
  }
  // Narrow terminal head, then model contact.
  addSquare(layers[10], 30, 16, 0);
  fillRect(layers[11], 24, 5, 36, 17);
  return layers;
}

std::vector<accloud::photons::BinaryMask> makeModelRootedSupportScene() {
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
  // First model island reached by the lower support.
  for (int layer = 5; layer < 13; ++layer) {
    fillRect(layers[layer], 4, 8, 16, 25);
  }
  // A new support starts on that model with its smallest section.
  addSquare(layers[6], 17, 18, 0);
  addSquare(layers[7], 19, 17, 1);
  addSquare(layers[8], 21, 16, 1);
  addSquare(layers[9], 23, 15, 1);
  addSquare(layers[10], 25, 14, 0);
  // Upper model island contacted by the narrowed head.
  fillRect(layers[11], 24, 4, 39, 14);
  fillRect(layers[12], 23, 3, 40, 14);
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
  ok &= require(materializationMatchesSummary(analyzer, branchedLayers, branched),
                "all compact layer indices must rematerialize the recorded run totals");

  auto untaperedLayers = makeUntaperedContactScene();
  VectorSource untaperedSource(untaperedLayers);
  const auto untapered = analyzer.analyze(untaperedSource, testOptions());
  ok &= require(untapered.ok, "untapered contact analysis must succeed");
  ok &= require(untapered.summary.acceptedNodeCount > 0,
                "matter in the support-only phase must remain classified as support");
  ok &= require(countNodeKind(untapered, accloud::render3d::SupportNodeKind::Head) == 0,
                "a branch touching the part without narrowing must not expose a support head");
  bool untaperedSupportInsideModel = false;
  for (std::size_t layer = untapered.summary.firstModelLayer;
       layer < untapered.layers.size(); ++layer) {
    untaperedSupportInsideModel = untaperedSupportInsideModel
                                  || layerHasSupport(untapered.layers[layer]);
  }
  ok &= require(!untaperedSupportInsideModel,
                "an untapered contact must not project support semantics into the part");

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
                    && first.summary.braceEdgeCount == second.summary.braceEdgeCount,
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
