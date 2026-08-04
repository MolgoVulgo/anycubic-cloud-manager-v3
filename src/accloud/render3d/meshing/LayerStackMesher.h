#pragma once

#include "domain/photons/LayerMaskSource.h"
#include "domain/photons/LayerRange.h"
#include "domain/photons/MeshChunk.h"

#include <cstddef>
#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace accloud::render3d {

enum class CutSurfaceMode {
  Open,
  Closed,
};

enum class CutSurfaceBoundary {
  Lower,
  Upper,
};

inline constexpr std::size_t kMinimumMeshWorkerCount = 1;
inline constexpr std::size_t kMaximumMeshWorkerCount = 16;

struct MeshBuildOptions {
  double pitchXMm = 1.0;
  double pitchYMm = 1.0;
  double pitchZMm = 1.0;
  std::size_t chunkLayerCount = 8;
  CutSurfaceMode cutSurfaceMode = CutSurfaceMode::Open;
  // Preview sampling along Z. A value of 2 decodes one source layer out of
  // two while preserving the first and last selected layers. The sampled mask
  // is extruded until the next sampled layer, so the model keeps its original
  // Z extent at reduced geometric fidelity.
  std::size_t layerStride = 1;
  // Number of chunk workers. Four is the application and core default; callers
  // may persist any value in the supported 1..16 range.
  std::size_t workerCount = 4;
  // PWSZ masks contain no exact model/support labels. When enabled, the mesher
  // tags only conservative support-like components; ambiguous material remains
  // Model. The tag uses the otherwise free bit 60 of PackedSurfaceQuad.
  bool classifySupports = false;
  double supportMaximumSpanMm = 3.0;
  double supportMaximumAreaMm2 = 7.0;
  double supportRaftMaximumHeightMm = 0.75;
};

struct MeshWorkerStats {
  std::size_t workerIndex = 0;
  std::size_t taskCount = 0;
  std::size_t decodedLayerCount = 0;
  std::size_t chunkCount = 0;
  std::uint64_t durationMs = 0;
};

struct CutSurfaceBuildResult {
  bool ok = false;
  photons::MeshChunk chunk;
  std::size_t decodedLayerCount = 0;
  std::string error;
};

struct MeshBuildResult {
  bool ok = false;
  bool cancelled = false;
  std::vector<photons::MeshChunk> chunks;
  std::size_t decodedLayerCount = 0;
  std::size_t effectiveWorkerCount = 1;
  std::vector<MeshWorkerStats> workerStats;
  std::string error;
};

struct MeshBuildCallbacks {
  std::function<bool(photons::MeshChunk&&)> consumeChunk;
  std::function<bool()> isCancelled;
  std::function<void(std::size_t completedLayers, std::size_t totalLayers)> progress;
};

class LayerStackMesher {
public:
  [[nodiscard]] CutSurfaceBuildResult buildCutSurface(
      photons::LayerMaskSource& source,
      std::size_t maskLayer,
      std::size_t planeLayer,
      CutSurfaceBoundary boundary,
      const MeshBuildOptions& options) const;

  [[nodiscard]] MeshBuildResult build(
      photons::LayerMaskSource& source,
      photons::LayerRange range,
      const MeshBuildOptions& options,
      const MeshBuildCallbacks& callbacks = {}) const;
};

} // namespace accloud::render3d
