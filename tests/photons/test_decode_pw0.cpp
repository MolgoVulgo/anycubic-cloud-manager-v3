#include "infra/photons/drivers/pwmb/codec/Pw0Decoder.h"

#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

} // namespace

int main() {
  using accloud::photons::pw0::DecodeOptions;
  using accloud::photons::pw0::decode;

  bool ok = true;

  const std::vector<std::uint8_t> mixed{
      0x00, 0x03, // black x3, two-byte run
      0x52,       // grey level 5 x2, one-byte run
      0xf0, 0x04, // white x4, two-byte run
      0x00, 0x03, // black x3
  };
  const auto decoded = decode(std::span<const std::uint8_t>(mixed), 4, 3, DecodeOptions{true});
  ok &= require(decoded.ok, "mixed pw0Img stream must decode");
  ok &= require(decoded.decodedPixels == 12, "mixed pw0Img must fill the exact raster");
  ok &= require(decoded.bytesConsumed == mixed.size(), "mixed pw0Img must consume the complete stream");
  ok &= require(decoded.layer.hasIntermediateGray, "grey runs must mark antialiasing as present");
  ok &= require(decoded.layer.decodedGray.has_value(), "retainGray must preserve exposure levels");
  ok &= require(decoded.layer.maskTruth.has_value(), "decoder must produce a material mask");
  if (decoded.layer.decodedGray) {
    ok &= require((*decoded.layer.decodedGray)[3] == 5u, "grey exposure level must be preserved");
    ok &= require((*decoded.layer.decodedGray)[5] == 15u, "white exposure level must be preserved");
  }
  if (decoded.layer.maskTruth) {
    ok &= require(decoded.layer.maskTruth->count() == 6, "every non-black pixel must be material");
  }
  if (decoded.layer.bbox) {
    ok &= require(decoded.layer.bbox->minX == 0 && decoded.layer.bbox->minY == 0
                      && decoded.layer.bbox->maxX == 3 && decoded.layer.bbox->maxY == 2,
                  "material bounding box must include grey and white runs");
  } else {
    ok &= require(false, "non-empty material must produce a bounding box");
  }

  const std::vector<std::uint8_t> binary{
      0x00, 0x06,
      0xf0, 0x06,
  };
  const auto binaryDecoded = decode(binary, 4, 3);
  ok &= require(binaryDecoded.ok, "binary pw0Img stream must decode without antialiasing");
  ok &= require(!binaryDecoded.layer.hasIntermediateGray,
                "absence of intermediate grey is valid and must not imply antialiasing");
  ok &= require(binaryDecoded.layer.maskTruth && binaryDecoded.layer.maskTruth->count() == 6,
                "binary material mask must be exact");

  const std::vector<std::uint8_t> crossRows{0xf0, 0x82}; // white x130
  const auto crossRowsDecoded = decode(crossRows, 65, 2);
  ok &= require(crossRowsDecoded.ok && crossRowsDecoded.layer.maskTruth
                    && crossRowsDecoded.layer.maskTruth->count() == 130,
                "material runs must cross non-word-aligned row boundaries without padding leaks");

  const std::vector<std::uint8_t> zeroRun{0x00, 0x00};
  const auto zeroDecoded = decode(zeroRun, 1, 1);
  ok &= require(!zeroDecoded.ok && zeroDecoded.error.find("zero-length") != std::string::npos,
                "zero-length runs must be rejected");

  const std::vector<std::uint8_t> truncated{0xf0};
  const auto truncatedDecoded = decode(truncated, 1, 1);
  ok &= require(!truncatedDecoded.ok && truncatedDecoded.error.find("truncated") != std::string::npos,
                "truncated two-byte runs must be rejected");

  const std::vector<std::uint8_t> trailing{0xf0, 0x01, 0x11};
  const auto trailingDecoded = decode(trailing, 1, 1);
  ok &= require(trailingDecoded.ok && !trailingDecoded.layer.diagnostics.empty(),
                "trailing bytes after raster completion must be diagnosed and ignored");

  return ok ? 0 : 1;
}
