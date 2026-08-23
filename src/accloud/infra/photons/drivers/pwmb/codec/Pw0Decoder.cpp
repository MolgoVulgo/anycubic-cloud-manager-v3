#include "infra/photons/drivers/pwmb/codec/Pw0Decoder.h"

#include <algorithm>
#include <limits>
#include <optional>

namespace accloud::photons::pw0 {
namespace {

void includeRunInBoundingBox(
    BoundingBox& box,
    bool& hasBox,
    std::size_t first,
    std::size_t length,
    std::uint32_t width) {
  std::size_t cursor = first;
  std::size_t remaining = length;
  while (remaining > 0) {
    const auto y = static_cast<std::uint32_t>(cursor / width);
    const auto x = static_cast<std::uint32_t>(cursor % width);
    const std::size_t rowLength = std::min<std::size_t>(remaining, width - x);
    const auto maxX = static_cast<std::uint32_t>(x + rowLength - 1);

    if (!hasBox) {
      box = BoundingBox{x, y, maxX, y};
      hasBox = true;
    } else {
      box.minX = std::min(box.minX, x);
      box.minY = std::min(box.minY, y);
      box.maxX = std::max(box.maxX, maxX);
      box.maxY = std::max(box.maxY, y);
    }

    cursor += rowLength;
    remaining -= rowLength;
  }
}

} // namespace

DecodeResult decode(
    std::span<const std::uint8_t> encoded,
    std::uint32_t width,
    std::uint32_t height,
    DecodeOptions options) {
  DecodeResult result;
  result.layer.width = width;
  result.layer.height = height;

  if (width == 0 || height == 0) {
    result.error = "pw0Img dimensions must be non-zero";
    return result;
  }

  const std::uint64_t expected64 = static_cast<std::uint64_t>(width) * height;
  if (expected64 > std::numeric_limits<std::size_t>::max()) {
    result.error = "pw0Img raster exceeds addressable memory";
    return result;
  }
  const std::size_t expected = static_cast<std::size_t>(expected64);

  result.layer.maskTruth.emplace(width, height);
  if (options.retainGray) {
    result.layer.decodedGray.emplace(expected, 0u);
  }

  BoundingBox box;
  bool hasBox = false;
  std::size_t cursor = 0;
  std::size_t output = 0;

  while (output < expected) {
    if (cursor >= encoded.size()) {
      result.error = "pw0Img stream ended before the raster was complete";
      result.bytesConsumed = cursor;
      result.decodedPixels = output;
      return result;
    }

    const std::uint8_t first = encoded[cursor++];
    const std::uint8_t color = static_cast<std::uint8_t>(first >> 4u);
    std::size_t runLength = 0;

    if (color == 0u || color == 15u) {
      if (cursor >= encoded.size()) {
        result.error = "pw0Img two-byte run is truncated";
        result.bytesConsumed = cursor;
        result.decodedPixels = output;
        return result;
      }
      runLength = (static_cast<std::size_t>(first & 0x0fu) << 8u)
                  | encoded[cursor++];
    } else {
      runLength = first & 0x0fu;
      result.layer.hasIntermediateGray = true;
    }

    if (runLength == 0) {
      result.error = "pw0Img contains a zero-length run";
      result.bytesConsumed = cursor;
      result.decodedPixels = output;
      return result;
    }

    if (runLength > expected - output) {
      runLength = expected - output;
      result.layer.diagnostics.emplace_back(
          "final pw0Img run exceeded the raster and was clamped");
    }

    if (color != 0u) {
      result.layer.maskTruth->setRun(output, runLength);
      includeRunInBoundingBox(box, hasBox, output, runLength, width);
    }
    if (result.layer.decodedGray) {
      std::fill_n(result.layer.decodedGray->begin() + static_cast<std::ptrdiff_t>(output),
                  static_cast<std::ptrdiff_t>(runLength),
                  color);
    }

    output += runLength;
  }

  if (cursor < encoded.size()) {
    result.layer.diagnostics.emplace_back(
        "trailing pw0Img data was ignored after raster completion");
  }
  if (hasBox) {
    result.layer.bbox = box;
  }

  result.ok = true;
  result.bytesConsumed = cursor;
  result.decodedPixels = output;
  return result;
}

} // namespace accloud::photons::pw0
