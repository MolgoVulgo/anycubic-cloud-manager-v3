#pragma once

#include "domain/photons/LayerMaskSource.h"
#include "render3d/compute/SupportComputeBackend.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace accloud::render3d {

inline constexpr std::size_t kMinimumSupportAnalysisWorkerCount = 1u;
inline constexpr std::size_t kMaximumSupportAnalysisWorkerCount = 16u;

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

enum class SupportDecisionReason : std::uint8_t {
  RaftPrefix,
  FirstSupportLayer,
  SupportContinuation,
  SupportMotionContinuation,
  SupportFusionContinuation,
  SupportBornBeforeModel,
  ModelRootCandidate,
  ModelDominantMerge,
  RelativeExpansionBeforeFirstModel,
  UnrelatedAfterModel,
  ContactCandidateOpened,
  ContactCandidateContinued,
  ContactConfirmedAbrupt,
  ContactConfirmedProgressive,
  MixedSemanticProjection,
  BidirectionalModelContinuation,
  BidirectionalMixedReconciliation,
  RejectedSupportPath,
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

  std::size_t minimumTrackLayers = 3;
  std::size_t taperLookbackLayers = 8;
  // The bottom-up pass never treats first local growth as sufficient evidence
  // for a support/model boundary. A candidate must persist across this many
  // native layers before it becomes forward contact evidence for reconciliation
  // with the descending model pass.
  std::size_t modelContactConfirmationLayers = 2;
  // Size of the shared support-analysis worker pool. Low-priority preparation
  // decodes/describes upcoming native layers while foreground batches compute
  // independent component decisions, model lineage and reverse reconciliation
  // from immutable adjacent-layer state. Layer order and all graph/semantic
  // commits remain deterministic; the reverse traversal reuses the retained
  // compact layer descriptions.
  std::size_t workerCount = 1u;
  // P6.3 bounds the adaptive preparation window by estimated resident native
  // mask bytes instead of a fixed four-layer cap. The scheduler can therefore
  // use all configured workers on large workstation-class systems while still
  // preventing unbounded mask retention on very high-resolution sources.
  std::size_t preparationMemoryBudgetBytes = 256u * 1024u * 1024u;
  // P5 CPU acceleration. Normal runtime keeps this enabled: fragmented sparse
  // rows are promoted to compact 64-bit bitsets for overlap/intersection hot
  // paths, with optional AVX2 word intersection on supported x86 CPUs and a
  // portable scalar fallback everywhere else. The run representation remains
  // authoritative and disabling this switch is reserved for diagnostics/tests.
  bool enableBitsetAcceleration = true;
  // P6 selects Vulkan Compute opportunistically for expensive translated-mask
  // overlap batches. Auto is the standard hybrid runtime mode and always falls
  // back to the canonical CPU path when Vulkan is not compiled, unavailable,
  // or the batch is too small to amortize transfer/dispatch cost. Cpu is the
  // only explicit override and keeps the whole semantic compute path on CPU.
  compute::SupportComputePreference computePreference =
      compute::SupportComputePreference::Auto;
  std::size_t vulkanMinimumComponentAreaPixels = 32768u;

  double raftMaximumChangedPixelRatio = 0.001;
  // Support topology is evaluated in the native resin domain: raster pixels
  // across consecutive layer indexes. The PWSZ layer height is metadata for Z
  // placement and must not change semantic classification for identical masks.
  double maximumLayerMotionPixels = 4.0;
  // After compensating the current centre translation, this fraction of the
  // smaller section must still overlap for the change to remain explainable as
  // the same support profile. A translated support is continuity, not evidence
  // that model matter has appeared.
  double minimumSupportShapeOverlapRatio = 0.85;
  // A terminal taper must contain either a direct final collapse or several
  // meaningful reductions in the recent branch history. A single old merge
  // followed by a stable smaller pillar is not a support tip.
  std::size_t minimumTerminalTaperSteps = 2;
  double terminalTaperStepRatio = 0.98;
  // Several previous support sections may temporarily merge into one raster
  // component. They are considered a support fusion only when they preserve
  // most of their own section and collectively explain a material part of the
  // current component.
  double minimumSupportParentCoverageRatio = 0.90;
  double minimumSupportFusionCoverageRatio = 0.40;
  double braceMinimumDriftPixelsPerLayer = 0.5;
  double braceMaximumDriftPixelsPerLayer = 4.0;
  double minimumModelExpansionRatio = 1.2;
  double abruptModelExpansionRatio = 4.0;
  double terminalTaperRatio = 0.72;
  double modelRootTaperRatio = 0.72;
  // Diagnostics are intentionally opt-in: the normal viewer does not retain a
  // per-component decision history. The dev probe enables it when producing an
  // auditable analysis bundle.
  bool captureDecisionTrace = false;
};

struct SupportAnalysisPerformanceTelemetry {
  std::size_t preparationWindowCapacity = 0u;
  std::size_t preparedLayerCount = 0u;
  std::size_t maximumPreparationInflight = 0u;
  std::uint64_t preparationLoadMicroseconds = 0u;
  std::uint64_t preparationDescribeMicroseconds = 0u;
  std::uint64_t forwardSemanticMicroseconds = 0u;
  std::uint64_t reverseSemanticMicroseconds = 0u;
  std::uint64_t forwardClassificationMicroseconds = 0u;
  std::uint64_t forwardCommitMicroseconds = 0u;
  std::uint64_t forwardLineageMicroseconds = 0u;
  std::uint64_t forwardLineageCommitMicroseconds = 0u;
  std::uint64_t reversePreparationMicroseconds = 0u;
  std::uint64_t reverseCommitMicroseconds = 0u;
  // P6.5 precomputes semantic-independent adjacency evidence between native
  // layers in contiguous lots, then reconciles the ordered support/model state
  // over that immutable graph. These counters expose the parallel graph stage.
  std::uint64_t semanticEvidenceMicroseconds = 0u;
  std::size_t semanticEvidenceLotCount = 0u;
  std::size_t semanticEvidenceLayerPairCount = 0u;
  std::size_t semanticEvidenceEdgeCount = 0u;
};

struct SupportAnalysisCallbacks {
  std::function<bool()> isCancelled;
  std::function<void(std::size_t completedLayers, std::size_t totalLayers)> progress;
  // Emitted once immediately after compute backend selection so runtime logs
  // reveal whether Vulkan is actually active even when a long analysis is
  // cancelled before the final summary is produced.
  std::function<void(
      bool compiled,
      bool active,
      const std::string& backend,
      const std::string& device,
      const std::string& diagnostic)> computeStatus;
  std::function<void(const compute::SupportComputeTelemetry&)> computeTelemetry;
  // Published alongside progress so a cancelled long-running analysis still
  // exposes whether time is being spent in mask loading, component extraction
  // or the ordered semantic passes.
  std::function<void(const SupportAnalysisPerformanceTelemetry&)> performanceTelemetry;
};

struct SupportDecisionTrace {
  std::size_t layer = 0;
  std::size_t componentId = 0;
  std::size_t nodeId = std::numeric_limits<std::size_t>::max();
  std::size_t parentNodeId = std::numeric_limits<std::size_t>::max();
  std::size_t currentAreaPixels = 0;
  std::size_t parentAreaPixels = 0;
  std::size_t recentSupportMaximumAreaPixels = 0;
  std::size_t overlapPixels = 0;
  std::size_t matchedSupportParentCount = 0;
  std::size_t preservedSupportParentCount = 0;
  std::size_t terminalTaperDecreaseSteps = 0;
  std::vector<std::size_t> matchedSupportParentNodeIds;
  std::vector<std::size_t> matchedSupportParentOverlapPixels;
  std::size_t alignedOverlapPixels = 0;
  std::size_t addedPixelsAfterAlignment = 0;
  std::size_t removedPixelsAfterAlignment = 0;
  std::size_t modelLineageOverlapPixels = 0;
  std::size_t reverseModelEvidencePixels = 0;
  std::size_t reverseSupportCorePixels = 0;
  std::size_t finalSupportPixels = 0;
  std::size_t finalModelPixels = 0;
  double centreDistancePixels = 0.0;
  double materialDistancePixels = 0.0;
  double primaryParentCoverageRatio = 0.0;
  double supportFusionCoverageRatio = 0.0;
  double predictedMotionXPixels = 0.0;
  double predictedMotionYPixels = 0.0;
  double motionResidualPixels = 0.0;
  double alignedOverlapRatio = 0.0;
  double alignedIntersectionOverUnion = 0.0;
  double modelLineageShiftXPixels = 0.0;
  double modelLineageShiftYPixels = 0.0;
  double parentAreaRatio = 0.0;
  double pendingStartAreaRatio = 0.0;
  std::size_t pendingContactLength = 0;
  std::uint32_t minX = 0;
  std::uint32_t minY = 0;
  std::uint32_t maxX = 0;
  std::uint32_t maxY = 0;
  bool overlapsPreviousModel = false;
  bool nearPreviousModel = false;
  bool terminalTaperOnParent = false;
  bool immediateTerminalTaperOnParent = false;
  bool terminalTaperReboundOnParent = false;
  bool supportFusionContinuation = false;
  bool supportMotionContinuation = false;
  bool modelLineageContinued = false;
  bool reverseModelLineageContinued = false;
  bool reverseModelSeed = false;
  bool bidirectionalConflict = false;
  bool contactCandidate = false;
  bool contactConfirmed = false;
  bool rootedInRaft = false;
  bool rootedInModel = false;
  bool accepted = false;
  bool mixedSemanticProjection = false;
  MaterialSemantic decision = MaterialSemantic::Model;
  SupportDecisionReason reason = SupportDecisionReason::UnrelatedAfterModel;
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
  std::size_t reverseModelSeedCount = 0;
  std::size_t reverseModelContinuationCount = 0;
  std::size_t bidirectionalMixedComponentCount = 0;
  bool vulkanComputeCompiled = false;
  bool vulkanComputeActive = false;
  std::string vulkanDeviceName;
  std::size_t vulkanEligibleJobCount = 0;
  std::size_t vulkanSubmittedJobCount = 0;
  std::size_t vulkanGpuJobCount = 0;
  std::size_t vulkanCpuFallbackJobCount = 0;
  std::size_t vulkanDispatchCount = 0;
  std::size_t vulkanDispatchFailureCount = 0;
  std::size_t vulkanMaximumBatchJobCount = 0;
  std::uint64_t vulkanUploadBytes = 0u;
  std::uint64_t vulkanReadbackBytes = 0u;
  std::uint64_t vulkanHostPreparationMicroseconds = 0u;
  std::uint64_t vulkanQueueWaitMicroseconds = 0u;
  std::uint64_t vulkanBatchExecutionMicroseconds = 0u;
  std::size_t vulkanRunSourceJobCount = 0u;
  std::size_t vulkanResidentReferenceUploadCount = 0u;
  std::size_t vulkanResidentReferenceReuseCount = 0u;
  std::uint64_t vulkanSubmittedWorkgroupCount = 0u;
  std::size_t vulkanSemanticLayerBatchCallCount = 0u;
  std::size_t vulkanSemanticLayerBatchJobCount = 0u;
  std::size_t supportPreparationWindowCapacity = 0u;
  std::size_t supportPreparedLayerCount = 0u;
  std::size_t supportMaximumPreparationInflight = 0u;
  std::uint64_t supportPreparationLoadMicroseconds = 0u;
  std::uint64_t supportPreparationDescribeMicroseconds = 0u;
  std::uint64_t supportForwardSemanticMicroseconds = 0u;
  std::uint64_t supportReverseSemanticMicroseconds = 0u;
  std::uint64_t supportForwardClassificationMicroseconds = 0u;
  std::uint64_t supportForwardCommitMicroseconds = 0u;
  std::uint64_t supportForwardLineageMicroseconds = 0u;
  std::uint64_t supportForwardLineageCommitMicroseconds = 0u;
  std::uint64_t supportReversePreparationMicroseconds = 0u;
  std::uint64_t supportReverseCommitMicroseconds = 0u;
  std::uint64_t supportSemanticEvidenceMicroseconds = 0u;
  std::size_t supportSemanticEvidenceLotCount = 0u;
  std::size_t supportSemanticEvidenceLayerPairCount = 0u;
  std::size_t supportSemanticEvidenceEdgeCount = 0u;
};

struct SupportAnalysisResult {
  bool ok = false;
  bool cancelled = false;
  std::string error;
  SupportAnalysisSummary summary;
  std::vector<LayerSemanticIndex> layers;
  std::vector<SupportGraphNode> nodes;
  std::vector<SupportGraphEdge> edges;
  std::vector<SupportDecisionTrace> decisions;
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
