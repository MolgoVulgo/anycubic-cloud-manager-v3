#include "domain/photons/PhotonsCapabilities.h"
#include "domain/photons/PhotonsDocument.h"
#include "domain/photons/PhotonsFormat.h"
#include "render3d/gl/Renderer.h"

#include <iostream>

int main() {
  using namespace accloud::photons;

  PhotonsDocument document;
  if (document.format != PhotonsFormat::UNKNOWN) {
    std::cerr << "new Photon documents must default to UNKNOWN format\n";
    return 1;
  }
  if (document.capabilities != 0u) {
    std::cerr << "new Photon documents must start without capabilities\n";
    return 1;
  }

  const auto capabilities = toMask(PhotonsCapability::BitmapSlices)
      | toMask(PhotonsCapability::HasPreviews);
  if (!hasCapability(capabilities, PhotonsCapability::BitmapSlices)
      || !hasCapability(capabilities, PhotonsCapability::HasPreviews)
      || hasCapability(capabilities, PhotonsCapability::VectorOrMeta)) {
    std::cerr << "Photon capability bitmask contract is inconsistent\n";
    return 1;
  }

  // The renderer is an opt-in CPU scene planner. Production presets keep it disabled.
  accloud::render3d::Renderer renderer;
  (void)renderer;

  return 0;
}
