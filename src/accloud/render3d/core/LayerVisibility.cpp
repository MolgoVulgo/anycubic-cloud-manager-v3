#include "render3d/core/LayerVisibility.h"

namespace accloud::render3d {

LayerVisibility::LayerVisibility(std::size_t totalLayers) {
  setTotalLayers(totalLayers);
}

void LayerVisibility::setTotalLayers(std::size_t totalLayers) noexcept {
  totalLayers_ = totalLayers;
  range_ = totalLayers == 0
               ? photons::LayerRange{1, 0}
               : photons::LayerRange{0, totalLayers - 1};
}

bool LayerVisibility::setZeroBased(
    photons::LayerRange range,
    std::string& error) noexcept {
  if (!range.validFor(totalLayers_)) {
    error = "visible layer range is outside the document";
    return false;
  }
  range_ = range;
  return true;
}

bool LayerVisibility::setOneBased(
    std::size_t firstLayer,
    std::size_t lastLayer,
    std::string& error) noexcept {
  const auto converted = photons::LayerRange::fromOneBased(
      firstLayer,
      lastLayer,
      totalLayers_);
  if (!converted) {
    error = "visible layer range must use valid one-based layer numbers";
    return false;
  }
  range_ = *converted;
  return true;
}

VisibleChunkSelection LayerVisibility::select(
    const std::vector<photons::MeshChunk>& chunks,
    double pitchZMm) const {
  VisibleChunkSelection selection;
  selection.layers = range_;
  if (!range_.valid()) {
    return selection;
  }
  selection.minimumZ = static_cast<float>(range_.first * pitchZMm);
  selection.maximumZ = static_cast<float>((range_.last + 1) * pitchZMm);

  for (std::size_t index = 0; index < chunks.size(); ++index) {
    if (chunks[index].layers.intersects(range_)) {
      selection.chunkIndices.push_back(index);
    }
  }
  return selection;
}

} // namespace accloud::render3d
