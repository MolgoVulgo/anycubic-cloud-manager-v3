#pragma once

#include "domain/photons/MeshChunk.h"
#include "render3d/meshing/LayerStackMesher.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace accloud::render3d {

// Exact XY rectangle used to reconstruct a horizontal section at any Z plane.
// The compact CPU cache keeps one orientation-independent copy per decoded
// mask layer. Upper/lower orientation and Z placement are applied only when a
// render batch is materialized.
struct LayerSectionRect {
  std::uint16_t x0 = 0;
  std::uint16_t x1 = 0;
  std::uint16_t y0 = 0;
  std::uint16_t y1 = 0;

  [[nodiscard]] bool operator==(const LayerSectionRect&) const noexcept = default;
};

static_assert(sizeof(LayerSectionRect) == 8,
              "LayerSectionRect must remain an eight-byte CPU cache record");

struct LayerSectionTemplate {
  std::uint32_t rasterWidth = 0;
  std::uint32_t rasterHeight = 0;
  std::vector<LayerSectionRect> rectangles;

  [[nodiscard]] std::size_t compactByteSize() const noexcept {
    return rectangles.size() * sizeof(LayerSectionRect);
  }
};

struct LayerSectionCacheStats {
  std::size_t hits = 0;
  std::size_t misses = 0;
  std::size_t evictions = 0;
  std::size_t entries = 0;
  std::size_t residentBytes = 0;
};

class LayerSectionCache {
public:
  explicit LayerSectionCache(
      std::size_t maximumBytes,
      std::size_t maximumEntries = 2048);

  [[nodiscard]] std::shared_ptr<const LayerSectionTemplate> find(
      std::size_t maskLayer);
  [[nodiscard]] std::shared_ptr<const LayerSectionTemplate> insert(
      std::size_t maskLayer,
      LayerSectionTemplate section);
  void clear() noexcept;

  [[nodiscard]] std::size_t maximumBytes() const noexcept { return maximumBytes_; }
  [[nodiscard]] LayerSectionCacheStats stats() const noexcept;

private:
  struct Entry {
    std::size_t maskLayer = 0;
    std::shared_ptr<const LayerSectionTemplate> section;
    std::size_t bytes = 0;
  };

  using EntryList = std::list<Entry>;
  using EntryIterator = EntryList::iterator;

  void evictToFit(std::size_t incomingBytes);

  std::size_t maximumBytes_ = 0;
  std::size_t maximumEntries_ = 0;
  std::size_t residentBytes_ = 0;
  std::size_t hits_ = 0;
  std::size_t misses_ = 0;
  std::size_t evictions_ = 0;
  EntryList entries_;
  std::unordered_map<std::size_t, EntryIterator> index_;
};

[[nodiscard]] bool makeLayerSectionTemplate(
    const photons::MeshChunk& source,
    LayerSectionTemplate& destination,
    std::string& error);

[[nodiscard]] photons::MeshChunk materializeLayerSection(
    const LayerSectionTemplate& section,
    std::size_t planeLayer,
    CutSurfaceBoundary boundary,
    float pitchXMm,
    float pitchYMm,
    float pitchZMm);

} // namespace accloud::render3d
