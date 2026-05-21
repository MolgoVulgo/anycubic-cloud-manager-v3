#pragma once

#include "domain/photons/LayerIndex.h"
#include "domain/photons/LayerSlice.h"
#include "domain/photons/PhotonsFormat.h"
#include "domain/photons/PhotonsMeta.h"

#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <vector>

namespace accloud::photons {

struct DriverProbeResult {
  PhotonsFormat format = PhotonsFormat::UNKNOWN;
  std::uint8_t confidence = 0;
};

class IFormatDriver {
public:
  virtual ~IFormatDriver() = default;

  virtual DriverProbeResult probe(std::istream& stream) const = 0;
  virtual PhotonsMeta readMeta(std::istream& stream) const = 0;
  virtual std::vector<std::vector<std::uint8_t>> readPreviews(std::istream& stream) const = 0;
  virtual std::vector<LayerIndex> buildLayerIndex(std::istream& stream) const = 0;
  virtual std::optional<LayerSlice> decodeLayer(std::istream& stream, std::size_t layerNumber) const = 0;
};

} // namespace accloud::photons
