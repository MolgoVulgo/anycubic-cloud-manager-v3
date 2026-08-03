#include "render3d/gl/Renderer.h"

#include <utility>

namespace accloud::render3d {

void Renderer::clear() noexcept {
  volume_.clear();
  visibility_.setTotalLayers(0);
  pitchZMm_ = 1.0;
}

void Renderer::setDocument(std::size_t totalLayers, double pitchZMm) noexcept {
  visibility_.setTotalLayers(totalLayers);
  pitchZMm_ = pitchZMm > 0.0 ? pitchZMm : 1.0;
}

bool Renderer::setVisibleLayersOneBased(
    std::size_t firstLayer,
    std::size_t lastLayer,
    std::string& error) noexcept {
  return visibility_.setOneBased(firstLayer, lastLayer, error);
}

void Renderer::appendChunk(photons::MeshChunk chunk) {
  volume_.append(std::move(chunk));
}

void Renderer::appendChunks(std::vector<photons::MeshChunk> chunks) {
  volume_.append(std::move(chunks));
}

RenderFramePlan Renderer::framePlan() const {
  const auto selected = visibility_.select(volume_.chunks(), pitchZMm_);
  RenderFramePlan plan;
  plan.layers = selected.layers;
  plan.minimumZ = selected.minimumZ;
  plan.maximumZ = selected.maximumZ;
  plan.bounds = volume_.bounds();
  plan.chunkIndices = selected.chunkIndices;
  return plan;
}

} // namespace accloud::render3d
