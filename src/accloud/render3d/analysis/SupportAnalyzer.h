#pragma once

#include "domain/photons/LayerMaskSource.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace accloud::render3d {

enum class MaterialSemantic : std::uint8_t {
  Model,
  Support,
  Raft,
};

enum class PrintPhase : std::uint8_t {
  Raft,
  SupportsOnly,
  ModelAndSupports,
  ModelMostly,
};

enum class SupportNodeKind : std::uint8_t {
  RaftRoot,
  Pillar,
  Branch,
  Brace,
  Head,
  Rejected,
};

enum class SupportEdgeKind : std::uint8_t {
  Continuation,
  Split,
  Brace,
  ModelContact,
};

struct SemanticRun {
  std::uint32_t y = 0;
  std::uint32_t firstX = 0;
  std::uint32_t lastX = 0;
  MaterialSemantic semantic = MaterialSemantic::Model;
};

struct LayerSemanticIndex {
  std::size_t layer = 0;
  PrintPhase phase = PrintPhase::ModelMostly;
  std::vector<std::uint32_t> supportComponentIds;
  std::vector<SemanticRun> projectedSupportRuns;
};

struct SupportGraphNode {
  std::size_t id = 0;
  std::size_t layer = 0;
  std::size_t areaPixels = 0;
  double centerXMillimetres = 0.0;
  double centerYMillimetres = 0.0;
  double equivalentDiameterMillimetres = 0.0;
  SupportNodeKind kind = SupportNodeKind::Rejected;
  bool rootedInRaft = false;
  bool rootedInModel = false;
  bool terminalTaper = false;
};

struct SupportGraphEdge {
  std::size_t lowerNode = 0;
  std::size_t upperNode = 0;
  SupportEdgeKind kind = SupportEdgeKind::Continuation;
};

struct SupportAnalysisOptions {
  double pitchXMillimetres = 1.0;
  double pitchYMillimetres = 1.0;
  double pitchZMillimetres = 1.0;

  std::size_t maximumRaftLayers = 32;
  std::size_t minimumTrackLayers = 3;
  std::size_t taperLookbackLayers = 8;

  double raftRetainedAreaRatio = 0.52;
  double maximumSupportDiameterMillimetres = 4.0;
  double maximumSupportAreaMillimetres2 = 14.0;
  double maximumLayerSlope = 1.8;
  double braceMinimumSlope = 0.55;
  double braceMaximumSlope = 1.65;
  double terminalTaperRatio = 0.72;
  double modelRootTaperRatio = 0.72;
};

struct SupportAnalysisCallbacks {
  std::function<bool()> isCancelled;
  std::function<void(std::size_t completedLayers, std::size_t totalLayers)> progress;
};

struct SupportAnalysisSummary {
  std::size_t raftLastLayer = 0;
  std::size_t firstModelLayer = 0;
  std::size_t lastSupportLayer = 0;
  std::size_t componentCount = 0;
  std::size_t candidateNodeCount = 0;
  std::size_t acceptedNodeCount = 0;
  std::size_t supportRunCount = 0;
  std::size_t freeSupportRunCount = 0;
  std::size_t projectedSupportRunCount = 0;
  std::size_t projectedContactPixelCount = 0;
  std::size_t rejectedProjectionRunCount = 0;
  std::size_t rejectedGrowthPixelCount = 0;
  std::size_t untaperedModelContactCount = 0;
  std::size_t contactsWithoutValidProjectionCount = 0;
  double maximumContactGrowthRatio = 0.0;
  std::size_t terminalSupportStopCount = 0;
  std::size_t expandingModelContactCount = 0;
  double maximumModelExpansionRatio = 0.0;
  std::size_t raftRunCount = 0;
  std::size_t continuationEdgeCount = 0;
  std::size_t splitEdgeCount = 0;
  std::size_t braceEdgeCount = 0;
  std::size_t modelContactEdgeCount = 0;
  std::size_t forcedSemanticSampleCount = 0;
};

struct SupportAnalysisResult {
  bool ok = false;
  bool cancelled = false;
  std::string error;
  SupportAnalysisSummary summary;
  std::vector<LayerSemanticIndex> layers;
  std::vector<SupportGraphNode> nodes;
  std::vector<SupportGraphEdge> edges;
  // Exact source layers that the second-pass mesher must retain in addition
  // to its regular preview stride. Each terminal head contributes its last
  // free layer and its first model-contact layer, preventing support matter
  // from being extruded across a skipped semantic transition.
  std::vector<std::size_t> forcedSampleLayers;
};

class SupportAnalyzer {
public:
  [[nodiscard]] SupportAnalysisResult analyze(
      photons::LayerMaskSource& source,
      const SupportAnalysisOptions& options,
      const SupportAnalysisCallbacks& callbacks = {}) const;

  [[nodiscard]] bool materializeLayerSemantics(
      const photons::BinaryMask& mask,
      const LayerSemanticIndex& index,
      std::vector<SemanticRun>& runs,
      std::string& error) const;
};

} // namespace accloud::render3d
