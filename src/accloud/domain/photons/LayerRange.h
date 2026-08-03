#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace accloud::photons {

struct LayerRange {
  std::size_t first = 0;
  std::size_t last = 0;

  [[nodiscard]] constexpr bool valid() const noexcept { return first <= last; }
  [[nodiscard]] constexpr std::size_t count() const noexcept {
    return valid() ? (last - first + 1) : 0;
  }
  [[nodiscard]] constexpr bool contains(std::size_t layer) const noexcept {
    return valid() && layer >= first && layer <= last;
  }
  [[nodiscard]] constexpr bool intersects(const LayerRange& other) const noexcept {
    return valid() && other.valid() && first <= other.last && other.first <= last;
  }
  [[nodiscard]] constexpr bool validFor(std::size_t layerCount) const noexcept {
    return valid() && layerCount > 0 && last < layerCount;
  }

  [[nodiscard]] static constexpr std::optional<LayerRange> fromOneBased(
      std::size_t firstLayer,
      std::size_t lastLayer,
      std::size_t layerCount) noexcept {
    if (firstLayer == 0 || lastLayer == 0 || firstLayer > lastLayer
        || lastLayer > layerCount) {
      return std::nullopt;
    }
    return LayerRange{firstLayer - 1, lastLayer - 1};
  }
};

} // namespace accloud::photons
