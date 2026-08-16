#include "domain/photons/BinaryMask.h"
#include "domain/photons/LayerMaskSource.h"
#include "render3d/analysis/SupportAnalysisDiagnostics.h"
#include "render3d/analysis/SupportAnalyzer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

class VectorSource final : public accloud::photons::LayerMaskSource {
public:
  explicit VectorSource(std::vector<accloud::photons::BinaryMask> layers)
      : layers_(std::move(layers)) {}

  std::size_t layerCount() const noexcept override { return layers_.size(); }
  std::uint32_t width() const noexcept override { return layers_.front().width(); }
  std::uint32_t height() const noexcept override { return layers_.front().height(); }

  std::optional<accloud::photons::BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) override {
    if (layerNumber >= layers_.size()) {
      error = "layer outside diagnostics test source";
      return std::nullopt;
    }
    return layers_[layerNumber];
  }

private:
  std::vector<accloud::photons::BinaryMask> layers_;
};

void fillRect(
    accloud::photons::BinaryMask& mask,
    std::uint32_t firstX,
    std::uint32_t firstY,
    std::uint32_t lastX,
    std::uint32_t lastY) {
  for (std::uint32_t y = firstY; y < lastY; ++y) {
    for (std::uint32_t x = firstX; x < lastX; ++x) {
      mask.set(x, y, true);
    }
  }
}

nlohmann::json readJson(const std::filesystem::path& path) {
  std::ifstream file(path);
  nlohmann::json value;
  file >> value;
  return value;
}

bool hasPngSignature(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  unsigned char signature[8] = {};
  file.read(reinterpret_cast<char*>(signature), sizeof(signature));
  const unsigned char expected[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
  return file.gcount() == static_cast<std::streamsize>(sizeof(signature))
         && std::equal(std::begin(signature), std::end(signature), std::begin(expected));
}

std::pair<std::uint32_t, std::uint32_t> readPngDimensions(
    const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::array<unsigned char, 24> header = {};
  file.read(reinterpret_cast<char*>(header.data()),
            static_cast<std::streamsize>(header.size()));
  if (file.gcount() != static_cast<std::streamsize>(header.size())) {
    return {0u, 0u};
  }
  const auto readBigEndian = [&](std::size_t offset) {
    return (static_cast<std::uint32_t>(header[offset]) << 24u)
           | (static_cast<std::uint32_t>(header[offset + 1u]) << 16u)
           | (static_cast<std::uint32_t>(header[offset + 2u]) << 8u)
           | static_cast<std::uint32_t>(header[offset + 3u]);
  };
  return {readBigEndian(16u), readBigEndian(20u)};
}

} // namespace

int main() {
  std::vector<accloud::photons::BinaryMask> layers;
  for (int index = 0; index < 7; ++index) {
    layers.emplace_back(40u, 32u);
  }
  fillRect(layers[0], 4u, 24u, 36u, 29u);
  fillRect(layers[1], 4u, 24u, 36u, 29u);
  fillRect(layers[2], 17u, 19u, 23u, 27u);
  fillRect(layers[3], 18u, 15u, 22u, 24u);
  fillRect(layers[4], 19u, 11u, 21u, 19u);
  fillRect(layers[5], 14u, 8u, 28u, 16u);
  fillRect(layers[6], 10u, 6u, 32u, 15u);

  VectorSource source(layers);
  accloud::render3d::SupportAnalysisOptions options;
  options.captureDecisionTrace = true;
  options.pitchXMillimetres = 0.05;
  options.pitchYMillimetres = 0.05;
  options.pitchZMillimetres = 0.05;

  accloud::render3d::SupportAnalyzer analyzer;
  const auto result = analyzer.analyze(source, options);
  if (!require(result.ok, result.error)
      || !require(!result.decisions.empty(), "decision trace is empty")) {
    return 1;
  }

  const auto output = std::filesystem::temp_directory_path()
                      / "accloud-support-analysis-diagnostics-test";
  std::error_code cleanupError;
  std::filesystem::remove_all(output, cleanupError);

  accloud::render3d::SupportAnalysisBundleWriter writer;
  accloud::render3d::SupportAnalysisBundleMetadata metadata;
  metadata.inputFileName = "synthetic.pwsz";
  metadata.pitchXMillimetres = options.pitchXMillimetres;
  metadata.pitchYMillimetres = options.pitchYMillimetres;
  metadata.pitchZMillimetres = options.pitchZMillimetres;
  accloud::render3d::SupportAnalysisBundleOptions bundleOptions;
  bundleOptions.downsample = 1u;
  std::string error;
  if (!require(writer.write(
          source, analyzer, result, options, metadata, output,
          bundleOptions, {}, error), error)) {
    return 1;
  }

  const auto manifest = readJson(output / "manifest.json");
  const auto summary = readJson(output / "summary.json");
  const auto analysis = readJson(output / "analysis.json");
  const auto decisions = readJson(output / "decisions.json");
  if (!require(manifest.at("layer_count") == layers.size(),
               "manifest layer count mismatch")
      || !require(manifest.at("images").size() == layers.size(),
                  "manifest image count mismatch")
      || !require(manifest.at("diagnostic_layout").at("orientation") == "vertical",
                  "diagnostic layout must be vertical")
      || !require(manifest.at("diagnostic_layout").at("separate_images") == true,
                  "diagnostic panels must be separate images")
      || !require(
          manifest.at("diagnostic_layout").at("pick_map_encoding")
              == "component_id_plus_one_rgb24",
          "diagnostic pick-map encoding mismatch")
      || !require(manifest.at("diagnostic_layout").at("node_id_labels") == true,
                  "diagnostic node labels must be enabled")
      || !require(summary.at("layer_count") == layers.size(),
                  "summary layer count mismatch")
      || !require(analysis.at("layers").size() == layers.size(),
                  "analysis layer count mismatch")
      || !require(!decisions.at("decisions").empty(),
                  "decisions JSON is empty")
      || !require(
          analysis.at("summary").contains("reverse_model_seeds")
              && analysis.at("summary").contains("reverse_model_continuations")
              && analysis.at("summary").contains("bidirectional_mixed_components"),
          "bidirectional summary counters are missing")) {
    return 1;
  }

  const auto& firstDecision = decisions.at("decisions").front();
  if (!require(firstDecision.contains("surface_comparison"),
               "surface comparison is missing")
      || !require(firstDecision.contains("choice"), "decision choice is missing")
      || !require(firstDecision.contains("reason_code"), "reason code is missing")
      || !require(firstDecision.contains("why"), "human reason is missing")) {
    return 1;
  }
  const auto parentedDecision = std::find_if(
      decisions.at("decisions").begin(), decisions.at("decisions").end(),
      [](const nlohmann::json& decision) {
        return !decision.at("parent_node_id").is_null();
      });
  if (!require(parentedDecision != decisions.at("decisions").end(),
               "parented diagnostic decision is missing")
      || !require(
          parentedDecision->at("surface_comparison").contains(
              "added_pixels_after_alignment"),
          "aligned added-pixel count is missing")
      || !require(
          parentedDecision->at("surface_comparison").contains(
              "removed_pixels_after_alignment"),
          "aligned removed-pixel count is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "aligned_overlap_ratio"),
          "aligned overlap ratio is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "predicted_motion_pixels"),
          "predicted support motion is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "motion_residual_pixels"),
          "support motion residual is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "support_motion_continuation"),
          "support motion decision flag is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "material_distance_pixels"),
          "material-to-material distance is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "matched_support_parent_node_ids"),
          "matched support-parent list is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "support_fusion_continuation"),
          "support fusion decision flag is missing")
      || !require(
          parentedDecision->at("surface_comparison").contains(
              "terminal_taper_decrease_steps"),
          "terminal taper step count is missing")
      || !require(
          parentedDecision->at("surface_comparison").contains(
              "support_fusion_coverage_ratio"),
          "support fusion coverage is missing")
      || !require(
          parentedDecision->at("surface_comparison").contains(
              "model_lineage_pixels"),
          "model-lineage pixel count is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "model_lineage_shift_pixels"),
          "model-lineage motion is missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "model_lineage_continued"),
          "model-lineage continuity flag is missing")
      || !require(
          parentedDecision->at("surface_comparison").contains(
              "reverse_model_evidence_pixels"),
          "reverse model-evidence pixel count is missing")
      || !require(
          parentedDecision->at("surface_comparison").contains(
              "reverse_support_core_pixels"),
          "reverse support-core pixel count is missing")
      || !require(
          parentedDecision->at("surface_comparison").contains(
              "final_support_pixels")
              && parentedDecision->at("surface_comparison").contains(
                  "final_model_pixels"),
          "final bidirectional semantic pixel counts are missing")
      || !require(
          parentedDecision->at("geometric_comparison").contains(
              "reverse_model_lineage_continued")
              && parentedDecision->at("geometric_comparison").contains(
                  "reverse_model_seed")
              && parentedDecision->at("geometric_comparison").contains(
                  "bidirectional_conflict"),
          "reverse-pass reconciliation flags are missing")) {
    return 1;
  }

  bool foundNodeLabel = false;
  bool foundSelectionId = false;
  for (std::size_t layer = 1; layer <= layers.size(); ++layer) {
    std::ostringstream baseName;
    baseName << "layer_" << std::setw(6) << std::setfill('0') << layer;
    const std::array<std::string, 4> panels = {
        "raw", "semantic", "nodes", "pick",
    };
    std::optional<std::pair<std::uint32_t, std::uint32_t>> expectedDimensions;
    for (const auto& panel : panels) {
      const auto fileName = baseName.str() + "_" + panel + ".png";
      const auto path = output / "images" / fileName;
      if (!require(hasPngSignature(path),
                   "diagnostic panel PNG is missing or invalid")) {
        return 1;
      }
      const auto dimensions = readPngDimensions(path);
      if (!require(dimensions.first > 0u && dimensions.first <= 40u,
                   "diagnostic panel width is invalid")
          || !require(dimensions.second > 0u && dimensions.second <= 32u,
                      "diagnostic panel height is invalid")) {
        return 1;
      }
      if (!expectedDimensions.has_value()) {
        expectedDimensions = dimensions;
      } else if (!require(dimensions == *expectedDimensions,
                          "diagnostic panels and pick map must align")) {
        return 1;
      }
    }

    const auto jsonName = baseName.str() + ".json";
    const auto layerJson = readJson(output / "layers" / jsonName);
    if (!require(layerJson.at("layer") == layer,
                 "per-layer JSON index mismatch")
        || !require(layerJson.contains("decisions"),
                    "per-layer decisions are missing")
        || !require(layerJson.at("diagnostic").at("orientation") == "vertical",
                    "per-layer diagnostic layout mismatch")
        || !require(layerJson.at("diagnostic").at("separate_images") == true,
                    "per-layer separate-image flag is missing")
        || !require(layerJson.at("diagnostic").at("images").contains("raw_mask"),
                    "raw diagnostic image reference is missing")
        || !require(
            layerJson.at("diagnostic").at("images").contains("semantic_result"),
            "semantic diagnostic image reference is missing")
        || !require(
            layerJson.at("diagnostic").at("images").contains("decision_nodes"),
            "node diagnostic image reference is missing")
        || !require(layerJson.at("diagnostic").at("images").contains("pick_map"),
                    "diagnostic pick-map reference is missing")
        || !require(layerJson.at("diagnostic").contains("source_bounds_pixels"),
                    "diagnostic source bounds are missing")
        || !require(layerJson.at("diagnostic").contains("image_size_pixels"),
                    "diagnostic image size is missing")
        || !require(layerJson.at("diagnostic").contains("node_id_labels"),
                    "per-layer node-label index is missing")) {
      return 1;
    }
    for (const auto& decision : layerJson.at("decisions")) {
      foundSelectionId = foundSelectionId || decision.contains("selection_id");
    }
    foundNodeLabel = foundNodeLabel
                     || !layerJson.at("diagnostic").at("node_id_labels").empty();
  }
  if (!require(foundNodeLabel, "diagnostic node labels were not indexed")
      || !require(foundSelectionId,
                  "diagnostic decisions do not expose selection IDs")) {
    return 1;
  }

  std::filesystem::remove_all(output, cleanupError);
  std::cout << "support analysis diagnostics tests passed\n";
  return 0;
}
