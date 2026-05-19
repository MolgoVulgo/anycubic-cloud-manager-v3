#pragma once

#include "PhotonsCapabilities.h"
#include "PhotonsFormat.h"
#include "PhotonsMeta.h"
#include "LayerIndex.h"

#include <cstdint>
#include <vector>

namespace accloud::photons {

using PreviewImage = std::vector<std::uint8_t>;

struct PhotonsDocument {
  PhotonsFormat format = PhotonsFormat::UNKNOWN;
  PhotonsCapabilities capabilities = 0;
  PhotonsMeta meta;
  std::vector<PreviewImage> previews;
  std::vector<LayerIndex> layerIndex;
};

} // namespace accloud::photons
