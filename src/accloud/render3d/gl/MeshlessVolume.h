#pragma once

#include "domain/photons/LayerRange.h"
#include "domain/photons/MeshChunk.h"

#include <cstddef>
#include <vector>

namespace accloud::render3d {

class MeshlessVolume {
public:
  void clear() noexcept;
  void append(photons::MeshChunk chunk);
  void append(std::vector<photons::MeshChunk> chunks);

  [[nodiscard]] const std::vector<photons::MeshChunk>& chunks() const noexcept {
    return chunks_;
  }
  [[nodiscard]] const photons::MeshBounds& bounds() const noexcept { return bounds_; }
  [[nodiscard]] std::size_t triangleCount() const noexcept { return triangleCount_; }
  [[nodiscard]] std::vector<std::size_t> intersecting(
      photons::LayerRange range) const;

private:
  void include(const photons::MeshBounds& bounds) noexcept;

  std::vector<photons::MeshChunk> chunks_;
  photons::MeshBounds bounds_;
  std::size_t triangleCount_ = 0;
};

} // namespace accloud::render3d
