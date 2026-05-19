#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace accloud::photons {

struct BoundingBox {
  std::uint32_t minX = 0;
  std::uint32_t minY = 0;
  std::uint32_t maxX = 0;
  std::uint32_t maxY = 0;
};

struct LayerSlice {
  std::optional<std::vector<std::uint8_t>> decodedGray;
  std::optional<std::vector<std::uint8_t>> maskTruth;
  std::optional<BoundingBox> bbox;
};

} // namespace accloud::photons
