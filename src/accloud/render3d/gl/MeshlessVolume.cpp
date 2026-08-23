#include "render3d/gl/MeshlessVolume.h"

#include <utility>

namespace accloud::render3d {

void MeshlessVolume::clear() noexcept {
  chunks_.clear();
  bounds_ = {};
  triangleCount_ = 0;
}

void MeshlessVolume::include(const photons::MeshBounds& bounds) noexcept {
  if (!bounds.valid()) {
    return;
  }
  bounds_.include(bounds.minX, bounds.minY, bounds.minZ);
  bounds_.include(bounds.maxX, bounds.maxY, bounds.maxZ);
}

void MeshlessVolume::append(photons::MeshChunk chunk) {
  include(chunk.bounds);
  triangleCount_ += chunk.triangleCount();
  chunks_.push_back(std::move(chunk));
}

void MeshlessVolume::append(std::vector<photons::MeshChunk> chunks) {
  for (auto& chunk : chunks) {
    append(std::move(chunk));
  }
}

std::vector<std::size_t> MeshlessVolume::intersecting(
    photons::LayerRange range) const {
  std::vector<std::size_t> selected;
  if (!range.valid()) {
    return selected;
  }
  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    if (chunks_[index].layers.intersects(range)) {
      selected.push_back(index);
    }
  }
  return selected;
}

} // namespace accloud::render3d
