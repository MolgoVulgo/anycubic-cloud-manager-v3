#include "render3d/core/LayerSectionCache.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace accloud::render3d {

LayerSectionCache::LayerSectionCache(
    std::size_t maximumBytes,
    std::size_t maximumEntries)
    : maximumBytes_(maximumBytes),
      maximumEntries_(std::max<std::size_t>(1, maximumEntries)) {}

std::shared_ptr<const LayerSectionTemplate> LayerSectionCache::find(
    std::size_t maskLayer) {
  const auto found = index_.find(maskLayer);
  if (found == index_.end()) {
    ++misses_;
    return {};
  }
  ++hits_;
  entries_.splice(entries_.begin(), entries_, found->second);
  return found->second->section;
}

void LayerSectionCache::evictToFit(std::size_t incomingBytes) {
  while (!entries_.empty()
         && (entries_.size() >= maximumEntries_
             || incomingBytes > maximumBytes_ - std::min(maximumBytes_, residentBytes_))) {
    auto last = std::prev(entries_.end());
    residentBytes_ -= last->bytes;
    index_.erase(last->maskLayer);
    entries_.erase(last);
    ++evictions_;
  }
}

std::shared_ptr<const LayerSectionTemplate> LayerSectionCache::insert(
    std::size_t maskLayer,
    LayerSectionTemplate section) {
  const std::size_t bytes = section.compactByteSize();
  if (bytes > maximumBytes_) {
    return std::make_shared<const LayerSectionTemplate>(std::move(section));
  }

  if (const auto found = index_.find(maskLayer); found != index_.end()) {
    residentBytes_ -= found->second->bytes;
    entries_.erase(found->second);
    index_.erase(found);
  }

  evictToFit(bytes);
  auto shared = std::make_shared<const LayerSectionTemplate>(std::move(section));
  entries_.push_front(Entry{maskLayer, std::move(shared), bytes});
  index_[maskLayer] = entries_.begin();
  residentBytes_ += bytes;
  return entries_.begin()->section;
}

void LayerSectionCache::clear() noexcept {
  entries_.clear();
  index_.clear();
  residentBytes_ = 0;
}

LayerSectionCacheStats LayerSectionCache::stats() const noexcept {
  return LayerSectionCacheStats{
      hits_, misses_, evictions_, entries_.size(), residentBytes_};
}

bool makeLayerSectionTemplate(
    const photons::MeshChunk& source,
    LayerSectionTemplate& destination,
    std::string& error) {
  destination = {};
  destination.rasterWidth = source.rasterWidth;
  destination.rasterHeight = source.rasterHeight;
  destination.rectangles.reserve(source.surfaceQuadCount());

  for (const auto& quad : source.surfaces) {
    const auto face = photons::packedSurfaceFace(quad);
    if (face != photons::PackedSurfaceFace::NegativeZ
        && face != photons::PackedSurfaceFace::PositiveZ) {
      error = "layer section cache accepts horizontal surfaces only";
      destination = {};
      return false;
    }
    const auto x0 = photons::packedSurfaceField(quad, 0u, 14u);
    const auto x1 = photons::packedSurfaceField(quad, 14u, 14u);
    const auto y0 = photons::packedSurfaceField(quad, 28u, 13u);
    const auto y1 = photons::packedSurfaceField(quad, 41u, 13u);
    if (x0 > std::numeric_limits<std::uint16_t>::max()
        || x1 > std::numeric_limits<std::uint16_t>::max()
        || y0 > std::numeric_limits<std::uint16_t>::max()
        || y1 > std::numeric_limits<std::uint16_t>::max()) {
      error = "layer section coordinates exceed the compact CPU cache format";
      destination = {};
      return false;
    }
    destination.rectangles.push_back(LayerSectionRect{
        static_cast<std::uint16_t>(x0),
        static_cast<std::uint16_t>(x1),
        static_cast<std::uint16_t>(y0),
        static_cast<std::uint16_t>(y1),
    });
  }
  return true;
}

photons::MeshChunk materializeLayerSection(
    const LayerSectionTemplate& section,
    std::size_t planeLayer,
    CutSurfaceBoundary boundary,
    float pitchXMm,
    float pitchYMm,
    float pitchZMm) {
  photons::MeshChunk chunk;
  chunk.layers = {planeLayer, planeLayer};
  chunk.rasterWidth = section.rasterWidth;
  chunk.rasterHeight = section.rasterHeight;
  chunk.pitchXMm = pitchXMm;
  chunk.pitchYMm = pitchYMm;
  chunk.pitchZMm = pitchZMm;
  chunk.surfaces.reserve(section.rectangles.size());

  const auto face = boundary == CutSurfaceBoundary::Upper
                        ? photons::PackedSurfaceFace::PositiveZ
                        : photons::PackedSurfaceFace::NegativeZ;
  const float z = static_cast<float>(planeLayer) * pitchZMm;
  for (const auto& rectangle : section.rectangles) {
    chunk.surfaces.push_back(photons::packZSurface(
        face,
        0,
        rectangle.x0,
        rectangle.x1,
        rectangle.y0,
        rectangle.y1));
    chunk.bounds.include(
        static_cast<float>(rectangle.x0) * pitchXMm,
        static_cast<float>(rectangle.y0) * pitchYMm,
        z);
    chunk.bounds.include(
        static_cast<float>(rectangle.x1) * pitchXMm,
        static_cast<float>(rectangle.y1) * pitchYMm,
        z);
  }
  return chunk;
}

} // namespace accloud::render3d
