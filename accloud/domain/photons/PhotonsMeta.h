#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace accloud::photons {

struct PhotonsMeta {
  std::optional<std::uint32_t> resolutionX;
  std::optional<std::uint32_t> resolutionY;
  std::optional<double> pitchXYMm;
  std::optional<double> pitchZMm;
  std::optional<std::uint32_t> layerCount;
  std::optional<double> exposureSeconds;
  std::optional<double> bottomExposureSeconds;
  std::optional<std::uint32_t> antiAliasingLevel;
  std::string machineName;
};

} // namespace accloud::photons
