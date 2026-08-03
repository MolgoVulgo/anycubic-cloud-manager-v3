#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace accloud::photons {

struct LayerIndex {
  std::size_t layerNumber = 0;
  std::string storageMember;
  std::uint64_t fileOffset = 0;
  std::uint32_t byteLength = 0;
  std::uint32_t uncompressedByteLength = 0;
  double zHeightMm = 0.0;
  double thicknessMm = 0.0;
  std::uint32_t flags = 0;
};

} // namespace accloud::photons
