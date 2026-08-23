#pragma once

#include "domain/photons/LayerRange.h"
#include "domain/photons/MeshChunk.h"
#include "render3d/core/LayerVisibility.h"
#include "render3d/gl/MeshlessVolume.h"

#include <cstddef>
#include <string>
#include <vector>

namespace accloud::render3d {

struct RenderFramePlan {
  photons::LayerRange layers;
  float minimumZ = 0.0F;
  float maximumZ = 0.0F;
  photons::MeshBounds bounds;
  std::vector<std::size_t> chunkIndices;
};

class Renderer {
public:
  void clear() noexcept;
  void setDocument(std::size_t totalLayers, double pitchZMm) noexcept;
  [[nodiscard]] bool setVisibleLayersOneBased(
      std::size_t firstLayer,
      std::size_t lastLayer,
      std::string& error) noexcept;
  void appendChunk(photons::MeshChunk chunk);
  void appendChunks(std::vector<photons::MeshChunk> chunks);

  [[nodiscard]] RenderFramePlan framePlan() const;
  [[nodiscard]] const MeshlessVolume& volume() const noexcept { return volume_; }
  [[nodiscard]] std::size_t totalLayers() const noexcept { return visibility_.totalLayers(); }
  [[nodiscard]] double pitchZMm() const noexcept { return pitchZMm_; }

private:
  MeshlessVolume volume_;
  LayerVisibility visibility_;
  double pitchZMm_ = 1.0;
};

} // namespace accloud::render3d
