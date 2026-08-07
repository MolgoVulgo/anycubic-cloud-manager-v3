#pragma once

#include "domain/photons/LayerRange.h"

#include <array>
#include <cstddef>
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

// Axis-aligned surface orientation encoded in the upper three bits of a
// PackedSurfaceQuad. The ordering is shared with the OpenGL vertex shader.
enum class PackedSurfaceFace : std::uint32_t {
  NegativeX = 0,
  PositiveX = 1,
  NegativeY = 2,
  PositiveY = 3,
  NegativeZ = 4,
  PositiveZ = 5,
};

// Estimated semantic class attached to one surface. PWSZ layer masks merge the
// model, supports and raft, so Support means "classified as support-like by the
// conservative Render3D heuristic", never an exact slicer label.
enum class PackedSurfaceSemantic : std::uint32_t {
  Model = 0,
  Support = 1,
};

// One exact axis-aligned surface rectangle. Grid coordinates are encoded as
// integers and converted to millimetres by the renderer using the chunk pitch.
// No vertex, normal or index duplication is stored in the runtime mesh.
struct PackedSurfaceQuad {
  std::uint32_t low = 0;
  std::uint32_t high = 0;

  [[nodiscard]] bool operator==(const PackedSurfaceQuad&) const noexcept = default;
};

static_assert(sizeof(PackedSurfaceQuad) == 8,
              "PackedSurfaceQuad must remain an eight-byte GPU instance");

inline constexpr std::uint32_t kPackedSurfaceMaximumX = (1u << 14u) - 1u;
inline constexpr std::uint32_t kPackedSurfaceMaximumY = (1u << 13u) - 1u;
inline constexpr std::uint32_t kPackedSurfaceMaximumRelativeZ = (1u << 6u) - 1u;
inline constexpr std::uint32_t kPackedSurfaceSemanticOffset = 60u;
inline constexpr std::uint32_t kPackedSurfaceFaceOffset = 61u;
inline constexpr std::size_t kLegacyBytesPerSurfaceQuad =
    4u * sizeof(MeshVertex) + 6u * sizeof(std::uint32_t);

[[nodiscard]] constexpr std::uint64_t packedSurfaceBits(
    const PackedSurfaceQuad& quad) noexcept {
  return static_cast<std::uint64_t>(quad.low)
         | (static_cast<std::uint64_t>(quad.high) << 32u);
}

[[nodiscard]] constexpr std::uint32_t packedSurfaceField(
    const PackedSurfaceQuad& quad,
    std::uint32_t offset,
    std::uint32_t width) noexcept {
  return static_cast<std::uint32_t>(
      (packedSurfaceBits(quad) >> offset) & ((std::uint64_t{1} << width) - 1u));
}

[[nodiscard]] constexpr PackedSurfaceFace packedSurfaceFace(
    const PackedSurfaceQuad& quad) noexcept {
  return static_cast<PackedSurfaceFace>(packedSurfaceField(quad, kPackedSurfaceFaceOffset, 3u));
}

[[nodiscard]] constexpr PackedSurfaceSemantic packedSurfaceSemantic(
    const PackedSurfaceQuad& quad) noexcept {
  return static_cast<PackedSurfaceSemantic>(
      packedSurfaceField(quad, kPackedSurfaceSemanticOffset, 1u));
}

[[nodiscard]] constexpr PackedSurfaceQuad makePackedSurfaceQuad(
    std::uint64_t payload,
    PackedSurfaceFace face,
    PackedSurfaceSemantic semantic = PackedSurfaceSemantic::Model) noexcept {
  const std::uint64_t bits = payload
      | (static_cast<std::uint64_t>(semantic) << kPackedSurfaceSemanticOffset)
      | (static_cast<std::uint64_t>(face) << kPackedSurfaceFaceOffset);
  return PackedSurfaceQuad{
      static_cast<std::uint32_t>(bits),
      static_cast<std::uint32_t>(bits >> 32u),
  };
}

[[nodiscard]] constexpr PackedSurfaceQuad packXSurface(
    PackedSurfaceFace face,
    std::uint32_t fixedX,
    std::uint32_t y0,
    std::uint32_t y1,
    std::uint32_t z0,
    std::uint32_t z1,
    PackedSurfaceSemantic semantic = PackedSurfaceSemantic::Model) noexcept {
  const std::uint64_t payload = static_cast<std::uint64_t>(fixedX)
      | (static_cast<std::uint64_t>(y0) << 14u)
      | (static_cast<std::uint64_t>(y1) << 27u)
      | (static_cast<std::uint64_t>(z0) << 40u)
      | (static_cast<std::uint64_t>(z1) << 46u);
  return makePackedSurfaceQuad(payload, face, semantic);
}

[[nodiscard]] constexpr PackedSurfaceQuad packYSurface(
    PackedSurfaceFace face,
    std::uint32_t fixedY,
    std::uint32_t x0,
    std::uint32_t x1,
    std::uint32_t z0,
    std::uint32_t z1,
    PackedSurfaceSemantic semantic = PackedSurfaceSemantic::Model) noexcept {
  const std::uint64_t payload = static_cast<std::uint64_t>(fixedY)
      | (static_cast<std::uint64_t>(x0) << 13u)
      | (static_cast<std::uint64_t>(x1) << 27u)
      | (static_cast<std::uint64_t>(z0) << 41u)
      | (static_cast<std::uint64_t>(z1) << 47u);
  return makePackedSurfaceQuad(payload, face, semantic);
}

[[nodiscard]] constexpr PackedSurfaceQuad packZSurface(
    PackedSurfaceFace face,
    std::uint32_t relativeZ,
    std::uint32_t x0,
    std::uint32_t x1,
    std::uint32_t y0,
    std::uint32_t y1,
    PackedSurfaceSemantic semantic = PackedSurfaceSemantic::Model) noexcept {
  const std::uint64_t payload = static_cast<std::uint64_t>(x0)
      | (static_cast<std::uint64_t>(x1) << 14u)
      | (static_cast<std::uint64_t>(y0) << 28u)
      | (static_cast<std::uint64_t>(y1) << 41u)
      | (static_cast<std::uint64_t>(relativeZ) << 54u);
  return makePackedSurfaceQuad(payload, face, semantic);
}

struct MeshChunk {
  LayerRange layers;
  MeshBounds bounds;
  std::uint32_t rasterWidth = 0;
  std::uint32_t rasterHeight = 0;
  float pitchXMm = 1.0F;
  float pitchYMm = 1.0F;
  float pitchZMm = 1.0F;
  std::vector<PackedSurfaceQuad> surfaces;

  [[nodiscard]] bool empty() const noexcept { return surfaces.empty(); }
  [[nodiscard]] std::size_t surfaceQuadCount() const noexcept { return surfaces.size(); }
  [[nodiscard]] std::size_t triangleCount() const noexcept { return surfaces.size() * 2u; }
  [[nodiscard]] std::size_t legacyVertexCount() const noexcept { return surfaces.size() * 4u; }
  [[nodiscard]] std::size_t compactByteSize() const noexcept {
    return surfaces.size() * sizeof(PackedSurfaceQuad);
  }
  [[nodiscard]] std::size_t legacyEquivalentByteSize() const noexcept {
    return surfaces.size() * kLegacyBytesPerSurfaceQuad;
  }
};

[[nodiscard]] inline std::array<MeshVertex, 4> surfaceQuadCorners(
    const MeshChunk& chunk,
    const PackedSurfaceQuad& quad) noexcept {
  const auto face = packedSurfaceFace(quad);
  const float baseZ = static_cast<float>(chunk.layers.first) * chunk.pitchZMm;
  std::array<MeshVertex, 4> corners{};

  const auto vertex = [](float x, float y, float z, float nx, float ny, float nz) {
    return MeshVertex{x, y, z, nx, ny, nz};
  };

  if (face == PackedSurfaceFace::NegativeX || face == PackedSurfaceFace::PositiveX) {
    const float x = packedSurfaceField(quad, 0u, 14u) * chunk.pitchXMm;
    const float y0 = packedSurfaceField(quad, 14u, 13u) * chunk.pitchYMm;
    const float y1 = packedSurfaceField(quad, 27u, 13u) * chunk.pitchYMm;
    const float z0 = baseZ + packedSurfaceField(quad, 40u, 6u) * chunk.pitchZMm;
    const float z1 = baseZ + packedSurfaceField(quad, 46u, 6u) * chunk.pitchZMm;
    if (face == PackedSurfaceFace::PositiveX) {
      corners = {vertex(x, y0, z0, 1, 0, 0), vertex(x, y1, z0, 1, 0, 0),
                 vertex(x, y1, z1, 1, 0, 0), vertex(x, y0, z1, 1, 0, 0)};
    } else {
      corners = {vertex(x, y0, z0, -1, 0, 0), vertex(x, y0, z1, -1, 0, 0),
                 vertex(x, y1, z1, -1, 0, 0), vertex(x, y1, z0, -1, 0, 0)};
    }
    return corners;
  }

  if (face == PackedSurfaceFace::NegativeY || face == PackedSurfaceFace::PositiveY) {
    const float y = packedSurfaceField(quad, 0u, 13u) * chunk.pitchYMm;
    const float x0 = packedSurfaceField(quad, 13u, 14u) * chunk.pitchXMm;
    const float x1 = packedSurfaceField(quad, 27u, 14u) * chunk.pitchXMm;
    const float z0 = baseZ + packedSurfaceField(quad, 41u, 6u) * chunk.pitchZMm;
    const float z1 = baseZ + packedSurfaceField(quad, 47u, 6u) * chunk.pitchZMm;
    if (face == PackedSurfaceFace::PositiveY) {
      corners = {vertex(x0, y, z0, 0, 1, 0), vertex(x0, y, z1, 0, 1, 0),
                 vertex(x1, y, z1, 0, 1, 0), vertex(x1, y, z0, 0, 1, 0)};
    } else {
      corners = {vertex(x0, y, z0, 0, -1, 0), vertex(x1, y, z0, 0, -1, 0),
                 vertex(x1, y, z1, 0, -1, 0), vertex(x0, y, z1, 0, -1, 0)};
    }
    return corners;
  }

  const float x0 = packedSurfaceField(quad, 0u, 14u) * chunk.pitchXMm;
  const float x1 = packedSurfaceField(quad, 14u, 14u) * chunk.pitchXMm;
  const float y0 = packedSurfaceField(quad, 28u, 13u) * chunk.pitchYMm;
  const float y1 = packedSurfaceField(quad, 41u, 13u) * chunk.pitchYMm;
  const float z = baseZ + packedSurfaceField(quad, 54u, 6u) * chunk.pitchZMm;
  if (face == PackedSurfaceFace::PositiveZ) {
    corners = {vertex(x0, y0, z, 0, 0, 1), vertex(x1, y0, z, 0, 0, 1),
               vertex(x1, y1, z, 0, 0, 1), vertex(x0, y1, z, 0, 0, 1)};
  } else {
    corners = {vertex(x0, y0, z, 0, 0, -1), vertex(x0, y1, z, 0, 0, -1),
               vertex(x1, y1, z, 0, 0, -1), vertex(x1, y0, z, 0, 0, -1)};
  }
  return corners;
}

[[nodiscard]] inline std::array<MeshVertex, 6> expandSurfaceQuad(
    const MeshChunk& chunk,
    const PackedSurfaceQuad& quad) noexcept {
  const auto corners = surfaceQuadCorners(chunk, quad);
  return {corners[0], corners[1], corners[2], corners[0], corners[2], corners[3]};
}

} // namespace accloud::photons
