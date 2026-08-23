#include "render3d/meshing/LayerStackMesher.h"
#include "infra/photons/drivers/pwsz/PwszArchiveReader.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class VectorLayerSource final : public accloud::photons::LayerMaskSource {
public:
  VectorLayerSource(std::uint32_t width,
                    std::uint32_t height,
                    std::vector<accloud::photons::BinaryMask> layers)
      : width_(width), height_(height), layers_(std::move(layers)) {}

  std::size_t layerCount() const noexcept override { return layers_.size(); }
  std::uint32_t width() const noexcept override { return width_; }
  std::uint32_t height() const noexcept override { return height_; }

  std::optional<accloud::photons::BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) override {
    if (layerNumber >= layers_.size()) {
      error = "test layer outside source";
      return std::nullopt;
    }
    return layers_[layerNumber];
  }

private:
  std::uint32_t width_;
  std::uint32_t height_;
  std::vector<accloud::photons::BinaryMask> layers_;
};

accloud::photons::BinaryMask maskFromRows(const std::vector<std::string>& rows) {
  const auto height = static_cast<std::uint32_t>(rows.size());
  const auto width = rows.empty() ? 0u : static_cast<std::uint32_t>(rows.front().size());
  accloud::photons::BinaryMask mask(width, height);
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      if (rows[y][x] == '#') {
        mask.set(x, y);
      }
    }
  }
  return mask;
}

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

std::size_t totalTriangles(const std::vector<accloud::photons::MeshChunk>& chunks) {
  std::size_t count = 0;
  for (const auto& chunk : chunks) {
    count += chunk.triangleCount();
  }
  return count;
}

} // namespace

bool validateExternalMesh(
    const std::filesystem::path& path,
    std::size_t firstLayer,
    std::size_t lastLayer) {
  accloud::photons::pwsz::PwszArchiveReader reader;
  std::string error;
  if (!reader.open(path, error)) {
    std::cerr << path << ": " << error << '\n';
    return false;
  }
  const accloud::photons::LayerRange range{firstLayer, lastLayer};
  if (!range.validFor(reader.layerCount())) {
    std::cerr << path << ": external mesh range is invalid\n";
    return false;
  }

  const accloud::render3d::MeshBuildOptions options{
      reader.meta().pitchXMm.value_or(reader.meta().pitchXYMm.value_or(1.0)),
      reader.meta().pitchYMm.value_or(reader.meta().pitchXYMm.value_or(1.0)),
      reader.meta().pitchZMm.value_or(1.0),
      32,
      accloud::render3d::CutSurfaceMode::Closed,
  };
  accloud::render3d::LayerStackMesher mesher;
  const auto result = mesher.build(reader, range, options);
  if (!result.ok || result.chunks.empty()) {
    std::cerr << path << ": " << result.error << '\n';
    return false;
  }
  std::cout << path.filename().string() << ": mesh " << firstLayer << ".." << lastLayer
            << ", " << result.chunks.size() << " chunks, "
            << totalTriangles(result.chunks) << " triangles\n";
  return true;
}

int main(int argc, char** argv) {
  using accloud::photons::LayerRange;
  using accloud::render3d::CutSurfaceMode;
  using accloud::render3d::LayerStackMesher;
  using accloud::render3d::MeshBuildOptions;

  bool ok = true;
  LayerStackMesher mesher;

  VectorLayerSource single(1, 1, {maskFromRows({"#"})});
  auto singleMesh = mesher.build(
      single,
      LayerRange{0, 0},
      MeshBuildOptions{1.0, 1.0, 1.0, 32, CutSurfaceMode::Open});
  ok &= require(singleMesh.ok, "single voxel layer must mesh");
  ok &= require(singleMesh.chunks.size() == 1, "single layer must produce one chunk");
  ok &= require(totalTriangles(singleMesh.chunks) == 12,
                "single stacked pixel must produce six quads / twelve triangles");
  ok &= require(singleMesh.chunks[0].bounds.valid()
                    && singleMesh.chunks[0].bounds.minX == 0.0F
                    && singleMesh.chunks[0].bounds.maxX == 1.0F
                    && singleMesh.chunks[0].bounds.minZ == 0.0F
                    && singleMesh.chunks[0].bounds.maxZ == 1.0F,
                "single stacked pixel bounds must be exact");

  const auto hollow = maskFromRows({
      "###",
      "#.#",
      "###",
  });
  VectorLayerSource hollowSource(3, 3, {hollow});
  auto hollowMesh = mesher.build(
      hollowSource,
      LayerRange{0, 0},
      MeshBuildOptions{1.0, 1.0, 1.0, 32, CutSurfaceMode::Open});
  ok &= require(hollowMesh.ok, "hollow layer must mesh");
  ok &= require(totalTriangles(hollowMesh.chunks) > totalTriangles(singleMesh.chunks),
                "inner void walls must contribute geometry rather than being filled");

  const auto hollowFive = maskFromRows({
      "#####",
      "#...#",
      "#...#",
      "#...#",
      "#####",
  });
  VectorLayerSource hollowFiveSource(5, 5, {hollowFive});
  auto hollowFiveMesh = mesher.build(
      hollowFiveSource,
      LayerRange{0, 0},
      MeshBuildOptions{1.0, 1.0, 1.0, 32, CutSurfaceMode::Open});

  const auto hollowWithIsland = maskFromRows({
      "#####",
      "#...#",
      "#.#.#",
      "#...#",
      "#####",
  });
  VectorLayerSource islandSource(5, 5, {hollowWithIsland});
  auto islandMesh = mesher.build(
      islandSource,
      LayerRange{0, 0},
      MeshBuildOptions{1.0, 1.0, 1.0, 32, CutSurfaceMode::Open});
  ok &= require(islandMesh.ok, "material island inside a cavity must mesh");
  ok &= require(hollowFiveMesh.ok
                    && totalTriangles(islandMesh.chunks) > totalTriangles(hollowFiveMesh.chunks),
                "internal support/material island must not be discarded");

  const auto full = maskFromRows({"#"});
  VectorLayerSource column(1, 1, {full, full, full, full});
  auto openRange = mesher.build(
      column,
      LayerRange{1, 2},
      MeshBuildOptions{1.0, 1.0, 1.0, 32, CutSurfaceMode::Open});
  auto closedRange = mesher.build(
      column,
      LayerRange{1, 2},
      MeshBuildOptions{1.0, 1.0, 1.0, 32, CutSurfaceMode::Closed});
  ok &= require(openRange.ok && closedRange.ok, "partial layer ranges must mesh");
  ok &= require(totalTriangles(closedRange.chunks) == totalTriangles(openRange.chunks) + 4,
                "closed range must add two cut quads while open range exposes the interior");

  VectorLayerSource chunked(1, 1, {full, full, full, full, full});
  auto chunkedMesh = mesher.build(
      chunked,
      LayerRange{0, 4},
      MeshBuildOptions{1.0, 1.0, 0.05, 2, CutSurfaceMode::Open});
  ok &= require(chunkedMesh.ok && chunkedMesh.chunks.size() == 3,
                "mesh must be segmented by the configured layer chunk size");
  if (chunkedMesh.chunks.size() == 3) {
    ok &= require(chunkedMesh.chunks[0].layers.first == 0
                      && chunkedMesh.chunks[0].layers.last == 1
                      && chunkedMesh.chunks[2].layers.first == 4
                      && chunkedMesh.chunks[2].layers.last == 4,
                  "chunk layer ranges must remain explicit and inclusive");
  }

  if (argc == 4) {
    try {
      ok &= validateExternalMesh(
          argv[1],
          static_cast<std::size_t>(std::stoull(argv[2])),
          static_cast<std::size_t>(std::stoull(argv[3])));
    } catch (const std::exception& exception) {
      std::cerr << "invalid external mesh arguments: " << exception.what() << '\n';
      ok = false;
    }
  }

  return ok ? 0 : 1;
}
