#pragma once

#include <cstddef>
#include <cstdint>

namespace accloud::photons {

struct LayerIndex {
  std::size_t layerNumber = 0;
  std::uint64_t fileOffset = 0;
  std::uint32_t byteLength = 0;
  double zHeightMm = 0.0;
  std::uint32_t flags = 0;
};

} // namespace accloud::photons
