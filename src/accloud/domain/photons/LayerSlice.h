#pragma once

#include "domain/photons/BinaryMask.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace accloud::photons {

struct BoundingBox {
  std::uint32_t minX = 0;
  std::uint32_t minY = 0;
  std::uint32_t maxX = 0;
  std::uint32_t maxY = 0;
};

struct LayerSlice {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::optional<std::vector<std::uint8_t>> decodedGray;
  std::optional<BinaryMask> maskTruth;
  std::optional<BoundingBox> bbox;
  bool hasIntermediateGray = false;
  std::vector<std::string> diagnostics;
};

} // namespace accloud::photons
