#pragma once

#include "domain/photons/LayerRange.h"
#include "domain/photons/MeshChunk.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace accloud::render3d {

struct VisibleChunkSelection {
  photons::LayerRange layers;
  float minimumZ = 0.0F;
  float maximumZ = 0.0F;
  std::vector<std::size_t> chunkIndices;
};

class LayerVisibility {
public:
  explicit LayerVisibility(std::size_t totalLayers = 0);

  void setTotalLayers(std::size_t totalLayers) noexcept;
  [[nodiscard]] std::size_t totalLayers() const noexcept { return totalLayers_; }
  [[nodiscard]] photons::LayerRange range() const noexcept { return range_; }

  [[nodiscard]] bool setZeroBased(
      photons::LayerRange range,
      std::string& error) noexcept;
  [[nodiscard]] bool setOneBased(
      std::size_t firstLayer,
      std::size_t lastLayer,
      std::string& error) noexcept;

  [[nodiscard]] VisibleChunkSelection select(
      const std::vector<photons::MeshChunk>& chunks,
      double pitchZMm) const;

private:
  std::size_t totalLayers_ = 0;
  photons::LayerRange range_{};
};

} // namespace accloud::render3d
