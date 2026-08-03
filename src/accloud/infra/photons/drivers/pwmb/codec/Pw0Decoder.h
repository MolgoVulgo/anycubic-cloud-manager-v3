#pragma once

#include "domain/photons/LayerSlice.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace accloud::photons::pw0 {

struct DecodeOptions {
  bool retainGray = false;
};

struct DecodeResult {
  bool ok = false;
  LayerSlice layer;
  std::size_t bytesConsumed = 0;
  std::size_t decodedPixels = 0;
  std::string error;
};

[[nodiscard]] DecodeResult decode(
    std::span<const std::uint8_t> encoded,
    std::uint32_t width,
    std::uint32_t height,
    DecodeOptions options = {});

} // namespace accloud::photons::pw0
