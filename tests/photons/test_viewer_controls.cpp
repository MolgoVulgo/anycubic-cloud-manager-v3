#include "domain/photons/MeshChunk.h"
#include "render3d/core/LayerVisibility.h"
#include "render3d/core/OrbitCamera.h"

#include <cmath>
#include <iostream>
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
  bool ok = true;

  accloud::render3d::LayerVisibility emptyVisibility;
  const auto emptySelection = emptyVisibility.select({}, 0.05);
  ok &= require(emptySelection.chunkIndices.empty()
                    && emptySelection.minimumZ == 0.0F
                    && emptySelection.maximumZ == 0.0F,
                "empty documents must expose an empty layer selection");

  accloud::render3d::LayerVisibility visibility(1247);
  std::string error;
  ok &= require(visibility.setOneBased(415, 1021, error),
                "one-based user layer range must be accepted: " + error);
  ok &= require(visibility.range().first == 414 && visibility.range().last == 1020,
                "user range 415..1021 must map to zero-based 414..1020");
  ok &= require(visibility.range().count() == 607,
                "inclusive visible layer count must be exact");

  error.clear();
  ok &= require(visibility.setOneBased(1, 254, error)
                    && visibility.range().first == 0
                    && visibility.range().last == 253,
                "user range 1..254 must map to zero-based 0..253");
  error.clear();
  ok &= require(visibility.setOneBased(415, 1021, error),
                "second user range must remain selectable: " + error);

  std::vector<accloud::photons::MeshChunk> chunks;
  for (std::size_t first = 0; first < 1247; first += 32) {
    accloud::photons::MeshChunk chunk;
    chunk.layers = {first, std::min<std::size_t>(first + 31, 1246)};
    chunks.push_back(std::move(chunk));
  }

  const auto selection = visibility.select(chunks, 0.05);
  ok &= require(!selection.chunkIndices.empty(), "visible range must select intersecting chunks");
  if (!selection.chunkIndices.empty()) {
    ok &= require(chunks[selection.chunkIndices.front()].layers.contains(414),
                  "first selected chunk must contain the lower visible layer");
    ok &= require(chunks[selection.chunkIndices.back()].layers.contains(1020),
                  "last selected chunk must contain the upper visible layer");
  }
  ok &= require(std::abs(selection.minimumZ - 20.7F) < 0.0001F
                    && std::abs(selection.maximumZ - 51.05F) < 0.0001F,
                "visible Z clip planes must derive from the selected layer range");

  error.clear();
  ok &= require(!visibility.setOneBased(0, 1, error),
                "zero is not a valid user-facing layer number");

  accloud::photons::MeshBounds bounds;
  bounds.include(0.0F, 0.0F, 0.0F);
  bounds.include(10.0F, 20.0F, 30.0F);
  accloud::render3d::OrbitCamera camera;
  camera.fit(bounds);
  ok &= require(std::abs(camera.target().x - 5.0) < 0.0001
                    && std::abs(camera.target().y - 10.0) < 0.0001
                    && std::abs(camera.target().z - 15.0) < 0.0001,
                "camera fit must target the mesh bounding-box centre");
  const double fittedDistance = camera.distance();
  camera.zoom(1.0);
  ok &= require(camera.distance() < fittedDistance,
                "positive wheel zoom must move the camera closer");
  camera.orbit(0.25, 100.0);
  ok &= require(camera.pitch() < 1.5707963267948966,
                "camera pitch must remain below the pole singularity");
  const auto beforePan = camera.target();
  camera.pan(2.0, 3.0);
  const auto afterPan = camera.target();
  ok &= require(beforePan.x != afterPan.x || beforePan.y != afterPan.y || beforePan.z != afterPan.z,
                "camera pan must move the orbit target");

  return ok ? 0 : 1;
}
