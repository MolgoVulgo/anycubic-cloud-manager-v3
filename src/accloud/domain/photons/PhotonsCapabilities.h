#pragma once

#include <cstdint>

namespace accloud::photons {

enum class PhotonsCapability : std::uint32_t {
  None = 0,
  BitmapSlices = 1u << 0,
  VectorOrMeta = 1u << 1,
  HasPreviews = 1u << 2,
};

using PhotonsCapabilities = std::uint32_t;

constexpr PhotonsCapabilities toMask(PhotonsCapability capability) {
  return static_cast<PhotonsCapabilities>(capability);
}

constexpr bool hasCapability(PhotonsCapabilities capabilities, PhotonsCapability capability) {
  return (capabilities & toMask(capability)) != 0u;
}

} // namespace accloud::photons
