#pragma once

#include "domain/photons/LayerRange.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace accloud::photons {

struct MeshVertex {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  float nx = 0.0F;
  float ny = 0.0F;
  float nz = 0.0F;
};

struct MeshBounds {
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float minZ = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  float maxZ = std::numeric_limits<float>::lowest();

  [[nodiscard]] bool valid() const noexcept {
    return minX <= maxX && minY <= maxY && minZ <= maxZ;
  }

  void include(float x, float y, float z) noexcept {
    minX = x < minX ? x : minX;
    minY = y < minY ? y : minY;
    minZ = z < minZ ? z : minZ;
    maxX = x > maxX ? x : maxX;
    maxY = y > maxY ? y : maxY;
    maxZ = z > maxZ ? z : maxZ;
  }
};

struct MeshChunk {
  LayerRange layers;
  MeshBounds bounds;
  std::vector<MeshVertex> vertices;
  std::vector<std::uint32_t> indices;

  [[nodiscard]] bool empty() const noexcept { return indices.empty(); }
  [[nodiscard]] std::size_t triangleCount() const noexcept { return indices.size() / 3; }
};

} // namespace accloud::photons
