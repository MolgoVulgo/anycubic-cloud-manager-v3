#pragma once

#include "domain/photons/BinaryMask.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace accloud::photons {

class LayerMaskSource {
public:
  virtual ~LayerMaskSource() = default;

  [[nodiscard]] virtual std::size_t layerCount() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t width() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t height() const noexcept = 0;
  // Implementations returning true guarantee that loadMask() may be called
  // concurrently from several worker threads after the source has been opened.
  // Sources that do not opt in are serialized by the mesher.
  [[nodiscard]] virtual bool supportsConcurrentMaskLoads() const noexcept {
    return false;
  }

  [[nodiscard]] virtual std::optional<BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) = 0;
};

} // namespace accloud::photons
