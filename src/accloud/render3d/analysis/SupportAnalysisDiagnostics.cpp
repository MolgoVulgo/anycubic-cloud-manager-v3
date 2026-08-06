#include "render3d/analysis/SupportAnalysisDiagnostics.h"

#include "domain/photons/BinaryMask.h"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace accloud::render3d {
namespace {

using photons::BinaryMask;

const char* phaseName(PrintPhase phase) {
  switch (phase) {
  case PrintPhase::Raft: return "raft";
  case PrintPhase::SupportsOnly: return "supports_only";
  case PrintPhase::ModelAndSupports: return "model_and_supports";
  case PrintPhase::ModelMostly: return "model_mostly";
  }
  return "unknown";
}

const char* semanticName(MaterialSemantic semantic) {
  switch (semantic) {
  case MaterialSemantic::Model: return "model";
  case MaterialSemantic::Support: return "support";
  case MaterialSemantic::Raft: return "raft";
  }
  return "unknown";
}

const char* nodeKindName(SupportNodeKind kind) {
  switch (kind) {
  case SupportNodeKind::RaftRoot: return "raft_root";
  case SupportNodeKind::Pillar: return "pillar";
  case SupportNodeKind::Branch: return "branch";
  case SupportNodeKind::Brace: return "brace";
  case SupportNodeKind::Head: return "head";
  case SupportNodeKind::Rejected: return "rejected";
  }
  return "unknown";
}

const char* edgeKindName(SupportEdgeKind kind) {
  switch (kind) {
  case SupportEdgeKind::Continuation: return "continuation";
  case SupportEdgeKind::Split: return "split";
  case SupportEdgeKind::Brace: return "brace";
  case SupportEdgeKind::ModelContact: return "model_contact";
  }
  return "unknown";
}

struct Bounds {
  std::uint32_t minX = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t minY = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxX = 0;
  std::uint32_t maxY = 0;

  [[nodiscard]] bool empty() const noexcept {
    return minX == std::numeric_limits<std::uint32_t>::max();
  }
};

Bounds materialBounds(const BinaryMask& material) {
  Bounds bounds;
  for (std::uint32_t y = 0; y < material.height(); ++y) {
    for (std::size_t wordIndex = 0; wordIndex < material.wordsPerRow(); ++wordIndex) {
      std::uint64_t word = material.rowWord(y, wordIndex);
      if (wordIndex + 1u == material.wordsPerRow()
          && (material.width() % 64u) != 0u) {
        word &= (std::uint64_t{1} << (material.width() % 64u)) - 1u;
      }
      if (word == 0u) {
        continue;
      }
      const auto first = static_cast<std::uint32_t>(wordIndex * 64u)
                         + static_cast<std::uint32_t>(std::countr_zero(word));
      const auto last = static_cast<std::uint32_t>(wordIndex * 64u)
                        + 64u - static_cast<std::uint32_t>(std::countl_zero(word));
      bounds.minX = std::min(bounds.minX, first);
      bounds.minY = std::min(bounds.minY, y);
      bounds.maxX = std::max(bounds.maxX, std::min(material.width(), last));
      bounds.maxY = std::max(bounds.maxY, y + 1u);
    }
  }
  return bounds;
}

void appendBigEndian(std::vector<std::uint8_t>& output, std::uint32_t value) {
  output.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
  output.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
  output.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
  output.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendChunk(
    std::vector<std::uint8_t>& png,
    const std::array<char, 4>& type,
    const std::vector<std::uint8_t>& payload) {
  appendBigEndian(png, static_cast<std::uint32_t>(payload.size()));
  const auto typeOffset = png.size();
  png.insert(png.end(), type.begin(), type.end());
  png.insert(png.end(), payload.begin(), payload.end());
  const auto crc = crc32(
      0u,
      reinterpret_cast<const Bytef*>(png.data() + typeOffset),
      static_cast<uInt>(4u + payload.size()));
  appendBigEndian(png, static_cast<std::uint32_t>(crc));
}

bool writeRgbPng(
    const std::filesystem::path& path,
    std::uint32_t width,
    std::uint32_t height,
    const std::vector<std::uint8_t>& rgb,
    std::string& error) {
  if (width == 0u || height == 0u
      || rgb.size() != static_cast<std::size_t>(width) * height * 3u) {
    error = "invalid RGB image buffer";
    return false;
  }

  std::vector<std::uint8_t> scanlines;
  scanlines.resize(static_cast<std::size_t>(height) * (1u + width * 3u));
  for (std::uint32_t y = 0; y < height; ++y) {
    const auto destinationOffset = static_cast<std::size_t>(y) * (1u + width * 3u);
    scanlines[destinationOffset] = 0u;
    std::copy_n(
        rgb.data() + static_cast<std::size_t>(y) * width * 3u,
        static_cast<std::size_t>(width) * 3u,
        scanlines.data() + destinationOffset + 1u);
  }

  uLongf compressedSize = compressBound(static_cast<uLong>(scanlines.size()));
  std::vector<std::uint8_t> compressed(compressedSize);
  const auto status = compress2(
      compressed.data(), &compressedSize,
      scanlines.data(), static_cast<uLong>(scanlines.size()), Z_BEST_SPEED);
  if (status != Z_OK) {
    error = "zlib could not compress diagnostic PNG";
    return false;
  }
  compressed.resize(compressedSize);

  std::vector<std::uint8_t> png = {
      137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u,
  };
  std::vector<std::uint8_t> ihdr;
  appendBigEndian(ihdr, width);
  appendBigEndian(ihdr, height);
  ihdr.insert(ihdr.end(), {8u, 2u, 0u, 0u, 0u});
  appendChunk(png, {'I', 'H', 'D', 'R'}, ihdr);
  appendChunk(png, {'I', 'D', 'A', 'T'}, compressed);
  appendChunk(png, {'I', 'E', 'N', 'D'}, {});

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open diagnostic PNG output";
    return false;
  }
  file.write(reinterpret_cast<const char*>(png.data()),
             static_cast<std::streamsize>(png.size()));
  if (!file) {
    error = "cannot write diagnostic PNG output";
    return false;
  }
  return true;
}

void setPixel(
    std::vector<std::uint8_t>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t x,
    std::uint32_t y,
    const std::array<std::uint8_t, 3>& color) {
  if (x >= width || y >= height) {
    return;
  }
  const auto offset = (static_cast<std::size_t>(y) * width + x) * 3u;
  pixels[offset] = color[0];
  pixels[offset + 1u] = color[1];
  pixels[offset + 2u] = color[2];
}

void drawRect(
    std::vector<std::uint8_t>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t minX,
    std::uint32_t minY,
    std::uint32_t maxX,
    std::uint32_t maxY,
    const std::array<std::uint8_t, 3>& color) {
  if (minX >= maxX || minY >= maxY || minX >= width || minY >= height) {
    return;
  }
  maxX = std::min(maxX, width);
  maxY = std::min(maxY, height);
  for (std::uint32_t x = minX; x < maxX; ++x) {
    setPixel(pixels, width, height, x, minY, color);
    setPixel(pixels, width, height, x, maxY - 1u, color);
  }
  for (std::uint32_t y = minY; y < maxY; ++y) {
    setPixel(pixels, width, height, minX, y, color);
    setPixel(pixels, width, height, maxX - 1u, y, color);
  }
}

bool writeLayerDiagnostic(
    photons::LayerMaskSource& source,
    const SupportAnalyzer& analyzer,
    const SupportAnalysisResult& result,
    std::size_t layer,
    std::uint32_t downsample,
    const std::vector<const SupportDecisionTrace*>& decisions,
    const std::filesystem::path& path,
    std::string& error) {
  auto material = source.loadMask(layer, error);
  if (!material) {
    return false;
  }
  std::vector<SemanticRun> semanticRuns;
  if (!analyzer.materializeLayerSemantics(
          *material, result.layers[layer], semanticRuns, error)) {
    return false;
  }

  auto bounds = materialBounds(*material);
  if (bounds.empty()) {
    bounds = Bounds{0u, 0u, std::max(1u, material->width()),
                    std::max(1u, material->height())};
  }
  const std::uint32_t margin = 8u * downsample;
  bounds.minX = bounds.minX > margin ? bounds.minX - margin : 0u;
  bounds.minY = bounds.minY > margin ? bounds.minY - margin : 0u;
  bounds.maxX = std::min(material->width(), bounds.maxX + margin);
  bounds.maxY = std::min(material->height(), bounds.maxY + margin);
  const auto panelWidth = std::max(
      1u, (bounds.maxX - bounds.minX + downsample - 1u) / downsample);
  const auto outputHeight = std::max(
      1u, (bounds.maxY - bounds.minY + downsample - 1u) / downsample);
  const auto separator = 2u;
  const auto outputWidth = panelWidth * 3u + separator * 2u;
  std::vector<std::uint8_t> pixels(
      static_cast<std::size_t>(outputWidth) * outputHeight * 3u, 10u);

  constexpr std::array<std::uint8_t, 3> rawColor = {205u, 210u, 216u};
  constexpr std::array<std::uint8_t, 3> modelColor = {220u, 96u, 79u};
  constexpr std::array<std::uint8_t, 3> supportColor = {80u, 184u, 198u};
  constexpr std::array<std::uint8_t, 3> raftColor = {239u, 166u, 55u};
  constexpr std::array<std::uint8_t, 3> separatorColor = {70u, 76u, 86u};

  const auto paintBlock = [&](std::uint32_t sourceY,
                              std::uint32_t firstX,
                              std::uint32_t lastX,
                              std::uint32_t panel,
                              const std::array<std::uint8_t, 3>& color) {
    if (sourceY < bounds.minY || sourceY >= bounds.maxY
        || lastX <= bounds.minX || firstX >= bounds.maxX) {
      return;
    }
    const auto clippedFirst = std::max(firstX, bounds.minX);
    const auto clippedLast = std::min(lastX, bounds.maxX);
    if (clippedFirst >= clippedLast) {
      return;
    }
    const auto firstBlock = (clippedFirst - bounds.minX) / downsample;
    const auto lastBlock = (clippedLast - 1u - bounds.minX) / downsample;
    const auto outputY = (sourceY - bounds.minY) / downsample;
    const auto panelOffset = panel * (panelWidth + separator);
    for (std::uint32_t x = firstBlock; x <= lastBlock; ++x) {
      setPixel(pixels, outputWidth, outputHeight,
               panelOffset + x, outputY, color);
    }
  };

  for (std::uint32_t y = bounds.minY; y < bounds.maxY; ++y) {
    for (std::size_t wordIndex = 0; wordIndex < material->wordsPerRow(); ++wordIndex) {
      std::uint64_t word = material->rowWord(y, wordIndex);
      if (wordIndex + 1u == material->wordsPerRow()
          && (material->width() % 64u) != 0u) {
        word &= (std::uint64_t{1} << (material->width() % 64u)) - 1u;
      }
      while (word != 0u) {
        const auto firstBit = static_cast<std::uint32_t>(std::countr_zero(word));
        const auto shifted = word >> firstBit;
        const auto runLength = static_cast<std::uint32_t>(std::countr_one(shifted));
        const auto firstX = static_cast<std::uint32_t>(wordIndex * 64u) + firstBit;
        const auto lastX = firstX + runLength;
        paintBlock(y, firstX, lastX, 0u, rawColor);
        paintBlock(y, firstX, lastX, 1u, modelColor);
        paintBlock(y, firstX, lastX, 2u, modelColor);
        if (runLength >= 64u - firstBit) {
          word = 0u;
        } else {
          word &= ~(((std::uint64_t{1} << runLength) - 1u) << firstBit);
        }
      }
    }
  }

  for (const auto& run : semanticRuns) {
    const auto color = run.semantic == MaterialSemantic::Raft
                           ? raftColor
                           : supportColor;
    paintBlock(run.y, run.firstX, run.lastX, 1u, color);
    paintBlock(run.y, run.firstX, run.lastX, 2u, color);
  }

  for (std::uint32_t x = panelWidth; x < panelWidth + separator; ++x) {
    for (std::uint32_t y = 0; y < outputHeight; ++y) {
      setPixel(pixels, outputWidth, outputHeight, x, y, separatorColor);
    }
  }
  const auto secondSeparator = panelWidth * 2u + separator;
  for (std::uint32_t x = secondSeparator; x < secondSeparator + separator; ++x) {
    for (std::uint32_t y = 0; y < outputHeight; ++y) {
      setPixel(pixels, outputWidth, outputHeight, x, y, separatorColor);
    }
  }

  for (const auto* decision : decisions) {
    if (decision == nullptr) {
      continue;
    }
    std::array<std::uint8_t, 3> color = decision->decision == MaterialSemantic::Model
        ? std::array<std::uint8_t, 3>{255u, 80u, 80u}
        : decision->decision == MaterialSemantic::Raft
              ? std::array<std::uint8_t, 3>{255u, 210u, 60u}
              : std::array<std::uint8_t, 3>{80u, 230u, 245u};
    if (decision->contactConfirmed) {
      color = {255u, 70u, 230u};
    } else if (decision->contactCandidate) {
      color = {255u, 235u, 80u};
    } else if (decision->mixedSemanticProjection) {
      color = {180u, 100u, 255u};
    }
    const auto convertX = [&](std::uint32_t sourceX) {
      if (sourceX <= bounds.minX) {
        return 0u;
      }
      return std::min(panelWidth, (sourceX - bounds.minX + downsample - 1u)
                                      / downsample);
    };
    const auto convertY = [&](std::uint32_t sourceY) {
      if (sourceY <= bounds.minY) {
        return 0u;
      }
      return std::min(outputHeight, (sourceY - bounds.minY + downsample - 1u)
                                        / downsample);
    };
    const auto panelOffset = 2u * (panelWidth + separator);
    drawRect(pixels, outputWidth, outputHeight,
             panelOffset + convertX(decision->minX), convertY(decision->minY),
             panelOffset + std::max(convertX(decision->maxX),
                                    convertX(decision->minX) + 1u),
             std::max(convertY(decision->maxY), convertY(decision->minY) + 1u),
             color);
  }

  return writeRgbPng(path, outputWidth, outputHeight, pixels, error);
}

nlohmann::json decisionJson(const SupportDecisionTrace& decision) {
  return {
      {"layer", decision.layer + 1u},
      {"component_id", decision.componentId},
      {"node_id", decision.nodeId == std::numeric_limits<std::size_t>::max()
                      ? nlohmann::json(nullptr)
                      : nlohmann::json(decision.nodeId)},
      {"parent_node_id",
       decision.parentNodeId == std::numeric_limits<std::size_t>::max()
           ? nlohmann::json(nullptr)
           : nlohmann::json(decision.parentNodeId)},
      {"bounds_pixels",
       {{"min_x", decision.minX}, {"min_y", decision.minY},
        {"max_x", decision.maxX}, {"max_y", decision.maxY}}},
      {"surface_comparison",
       {{"current_pixels", decision.currentAreaPixels},
        {"parent_pixels", decision.parentAreaPixels},
        {"recent_support_maximum_pixels",
         decision.recentSupportMaximumAreaPixels},
        {"parent_ratio", decision.parentAreaRatio},
        {"primary_parent_coverage_ratio",
         decision.primaryParentCoverageRatio},
        {"support_fusion_coverage_ratio",
         decision.supportFusionCoverageRatio},
        {"terminal_taper_decrease_steps",
         decision.terminalTaperDecreaseSteps},
        {"pending_start_ratio", decision.pendingStartAreaRatio},
        {"added_pixels_after_alignment", decision.addedPixelsAfterAlignment},
        {"removed_pixels_after_alignment", decision.removedPixelsAfterAlignment}}},
      {"geometric_comparison",
       {{"overlap_pixels", decision.overlapPixels},
        {"aligned_overlap_pixels", decision.alignedOverlapPixels},
        {"aligned_overlap_ratio", decision.alignedOverlapRatio},
        {"aligned_intersection_over_union",
         decision.alignedIntersectionOverUnion},
        {"centre_distance_pixels", decision.centreDistancePixels},
        {"material_distance_pixels", decision.materialDistancePixels},
        {"matched_support_parent_count",
         decision.matchedSupportParentCount},
        {"preserved_support_parent_count",
         decision.preservedSupportParentCount},
        {"matched_support_parent_node_ids",
         decision.matchedSupportParentNodeIds},
        {"matched_support_parent_overlap_pixels",
         decision.matchedSupportParentOverlapPixels},
        {"predicted_motion_pixels",
         {{"x", decision.predictedMotionXPixels},
          {"y", decision.predictedMotionYPixels}}},
        {"motion_residual_pixels", decision.motionResidualPixels},
        {"support_motion_continuation", decision.supportMotionContinuation},
        {"support_fusion_continuation", decision.supportFusionContinuation},
        {"overlaps_previous_model", decision.overlapsPreviousModel},
        {"near_previous_model", decision.nearPreviousModel},
        {"terminal_taper_on_parent", decision.terminalTaperOnParent},
        {"immediate_terminal_taper_on_parent",
         decision.immediateTerminalTaperOnParent},
        {"terminal_taper_rebound_on_parent",
         decision.terminalTaperReboundOnParent}}},
      {"state",
       {{"contact_candidate", decision.contactCandidate},
        {"contact_confirmed", decision.contactConfirmed},
        {"pending_contact_length", decision.pendingContactLength},
        {"rooted_in_raft", decision.rootedInRaft},
        {"rooted_in_model", decision.rootedInModel},
        {"accepted", decision.accepted},
        {"mixed_semantic_projection", decision.mixedSemanticProjection}}},
      {"choice", semanticName(decision.decision)},
      {"reason_code", supportDecisionReasonCode(decision.reason)},
      {"why", supportDecisionReasonText(decision.reason)},
  };
}

nlohmann::json summaryJson(const SupportAnalysisSummary& summary) {
  return {
      {"raft_last_layer", summary.raftLastLayer + 1u},
      {"first_model_layer", summary.firstModelLayer + 1u},
      {"last_support_layer", summary.lastSupportLayer + 1u},
      {"components", summary.componentCount},
      {"candidate_nodes", summary.candidateNodeCount},
      {"accepted_nodes", summary.acceptedNodeCount},
      {"raft_runs", summary.raftRunCount},
      {"support_runs", summary.supportRunCount},
      {"free_support_runs", summary.freeSupportRunCount},
      {"projected_support_runs", summary.projectedSupportRunCount},
      {"projected_contact_pixels", summary.projectedContactPixelCount},
      {"rejected_projection_runs", summary.rejectedProjectionRunCount},
      {"rejected_growth_pixels", summary.rejectedGrowthPixelCount},
      {"untapered_model_contacts", summary.untaperedModelContactCount},
      {"contacts_without_valid_projection",
       summary.contactsWithoutValidProjectionCount},
      {"maximum_contact_growth_ratio", summary.maximumContactGrowthRatio},
      {"terminal_support_stops", summary.terminalSupportStopCount},
      {"expanding_model_contacts", summary.expandingModelContactCount},
      {"maximum_model_expansion_ratio", summary.maximumModelExpansionRatio},
      {"continuations", summary.continuationEdgeCount},
      {"splits", summary.splitEdgeCount},
      {"braces", summary.braceEdgeCount},
      {"model_contacts", summary.modelContactEdgeCount},
      {"forced_semantic_samples", summary.forcedSemanticSampleCount},
  };
}

std::string layerFileName(std::size_t layerOneBased) {
  std::ostringstream stream;
  stream << "layer_" << std::setw(6) << std::setfill('0') << layerOneBased
         << ".png";
  return stream.str();
}

std::string layerJsonFileName(std::size_t layerOneBased) {
  std::ostringstream stream;
  stream << "layer_" << std::setw(6) << std::setfill('0') << layerOneBased
         << ".json";
  return stream.str();
}

bool writeJsonFile(
    const std::filesystem::path& path,
    const nlohmann::json& value,
    std::string& error) {
  std::ofstream file(path);
  if (!file) {
    error = "cannot open JSON output: " + path.string();
    return false;
  }
  file << value.dump(2) << '\n';
  if (!file) {
    error = "cannot write JSON output: " + path.string();
    return false;
  }
  return true;
}

} // namespace

const char* supportDecisionReasonCode(SupportDecisionReason reason) noexcept {
  switch (reason) {
  case SupportDecisionReason::RaftPrefix: return "raft_prefix";
  case SupportDecisionReason::FirstSupportLayer: return "first_support_layer";
  case SupportDecisionReason::SupportContinuation: return "support_continuation";
  case SupportDecisionReason::SupportMotionContinuation:
    return "support_motion_continuation";
  case SupportDecisionReason::SupportFusionContinuation:
    return "support_fusion_continuation";
  case SupportDecisionReason::SupportBornBeforeModel: return "support_born_before_model";
  case SupportDecisionReason::ModelRootCandidate: return "model_root_candidate";
  case SupportDecisionReason::ModelDominantMerge: return "model_dominant_merge";
  case SupportDecisionReason::RelativeExpansionBeforeFirstModel:
    return "relative_expansion_before_first_model";
  case SupportDecisionReason::UnrelatedAfterModel: return "unrelated_after_model";
  case SupportDecisionReason::ContactCandidateOpened: return "contact_candidate_opened";
  case SupportDecisionReason::ContactCandidateContinued:
    return "contact_candidate_continued";
  case SupportDecisionReason::ContactConfirmedAbrupt: return "contact_confirmed_abrupt";
  case SupportDecisionReason::ContactConfirmedProgressive:
    return "contact_confirmed_progressive";
  case SupportDecisionReason::MixedSemanticProjection:
    return "mixed_semantic_projection";
  case SupportDecisionReason::RejectedSupportPath: return "rejected_support_path";
  }
  return "unknown";
}

const char* supportDecisionReasonText(SupportDecisionReason reason) noexcept {
  switch (reason) {
  case SupportDecisionReason::RaftPrefix:
    return "The component belongs to the repeated raft prefix starting at layer 1.";
  case SupportDecisionReason::FirstSupportLayer:
    return "The first layer after the raft starts a raft-rooted support branch.";
  case SupportDecisionReason::SupportContinuation:
    return "A previous support parent was found and semantic continuity was preserved.";
  case SupportDecisionReason::SupportMotionContinuation:
    return "The translated section keeps the support shape and predicted trajectory, so local growth remains support matter.";
  case SupportDecisionReason::SupportFusionContinuation:
    return "Several previous support sections are preserved in the current component, so the local enlargement remains a support fusion.";
  case SupportDecisionReason::SupportBornBeforeModel:
    return "No model exists yet; the disconnected component remains part of the support network.";
  case SupportDecisionReason::ModelRootCandidate:
    return "The component starts beside established model matter and is tracked as a model-rooted support candidate.";
  case SupportDecisionReason::ModelDominantMerge:
    return "Previous model overlap dominates the recent support profile.";
  case SupportDecisionReason::RelativeExpansionBeforeFirstModel:
    return "The component expands beyond the complete previous support profile before the first model contact.";
  case SupportDecisionReason::UnrelatedAfterModel:
    return "No support parent exists after model matter has been established.";
  case SupportDecisionReason::ContactCandidateOpened:
    return "Growth after a terminal taper opened a provisional support-to-model contact.";
  case SupportDecisionReason::ContactCandidateContinued:
    return "The provisional contact persists on the following native layer.";
  case SupportDecisionReason::ContactConfirmedAbrupt:
    return "The local contact was confirmed by an abrupt expansion above the terminal support tip.";
  case SupportDecisionReason::ContactConfirmedProgressive:
    return "The local contact was confirmed by persistent cumulative growth above the terminal support tip.";
  case SupportDecisionReason::MixedSemanticProjection:
    return "Stable model matter and a raft-rooted support share one raster component; support runs were partitioned explicitly.";
  case SupportDecisionReason::RejectedSupportPath:
    return "The candidate path was not accepted as support, or a provisional contact did not persist.";
  }
  return "Unknown support-analysis decision.";
}

bool SupportAnalysisBundleWriter::write(
    photons::LayerMaskSource& source,
    const SupportAnalyzer& analyzer,
    const SupportAnalysisResult& result,
    const SupportAnalysisOptions& analysisOptions,
    const SupportAnalysisBundleMetadata& metadata,
    const std::filesystem::path& outputDirectory,
    const SupportAnalysisBundleOptions& options,
    const SupportAnalysisBundleCallbacks& callbacks,
    std::string& error) const {
  if (!result.ok) {
    error = "cannot export an unsuccessful support analysis";
    return false;
  }
  if (result.layers.size() != source.layerCount()) {
    error = "analysis layer count does not match source";
    return false;
  }
  if (options.downsample == 0u) {
    error = "diagnostic downsample must be positive";
    return false;
  }

  std::error_code filesystemError;
  std::filesystem::create_directories(outputDirectory, filesystemError);
  if (filesystemError) {
    error = "cannot create analysis bundle directory: " + filesystemError.message();
    return false;
  }
  const auto imagesDirectory = outputDirectory / "images";
  const auto layersDirectory = outputDirectory / "layers";
  if (options.writeImages) {
    std::filesystem::create_directories(imagesDirectory, filesystemError);
    if (filesystemError) {
      error = "cannot create diagnostic image directory: " + filesystemError.message();
      return false;
    }
  }
  std::filesystem::create_directories(layersDirectory, filesystemError);
  if (filesystemError) {
    error = "cannot create per-layer JSON directory: " + filesystemError.message();
    return false;
  }

  std::vector<std::vector<const SupportDecisionTrace*>> decisionsByLayer(
      source.layerCount());
  for (const auto& decision : result.decisions) {
    if (decision.layer < decisionsByLayer.size()) {
      decisionsByLayer[decision.layer].push_back(&decision);
    }
  }

  nlohmann::json analysis;
  analysis["schema"] = "accloud.support-analysis.v1";
  analysis["input"] = metadata.inputFileName;
  analysis["resolution"] = {source.width(), source.height()};
  analysis["layer_count"] = source.layerCount();
  analysis["pitch_mm"] = {
      metadata.pitchXMillimetres,
      metadata.pitchYMillimetres,
      metadata.pitchZMillimetres,
  };
  analysis["summary"] = summaryJson(result.summary);
  analysis["options"] = {
      {"minimum_track_layers", analysisOptions.minimumTrackLayers},
      {"taper_lookback_layers", analysisOptions.taperLookbackLayers},
      {"model_contact_confirmation_layers",
       analysisOptions.modelContactConfirmationLayers},
      {"maximum_layer_motion_pixels", analysisOptions.maximumLayerMotionPixels},
      {"minimum_terminal_taper_steps",
       analysisOptions.minimumTerminalTaperSteps},
      {"terminal_taper_step_ratio",
       analysisOptions.terminalTaperStepRatio},
      {"minimum_support_parent_coverage_ratio",
       analysisOptions.minimumSupportParentCoverageRatio},
      {"minimum_support_fusion_coverage_ratio",
       analysisOptions.minimumSupportFusionCoverageRatio},
      {"minimum_model_expansion_ratio",
       analysisOptions.minimumModelExpansionRatio},
      {"abrupt_model_expansion_ratio",
       analysisOptions.abruptModelExpansionRatio},
      {"terminal_taper_ratio", analysisOptions.terminalTaperRatio},
      {"model_root_taper_ratio", analysisOptions.modelRootTaperRatio},
  };
  analysis["forced_sample_layers"] = nlohmann::json::array();
  for (const auto forcedLayer : result.forcedSampleLayers) {
    analysis["forced_sample_layers"].push_back(forcedLayer + 1u);
  }
  analysis["layers"] = nlohmann::json::array();
  for (const auto& layer : result.layers) {
    analysis["layers"].push_back({
        {"layer", layer.layer + 1u},
        {"phase", phaseName(layer.phase)},
        {"support_components", layer.supportComponentIds.size()},
        {"projected_support_runs", layer.projectedSupportRuns.size()},
        {"decision_count", decisionsByLayer[layer.layer].size()},
    });
  }
  analysis["nodes"] = nlohmann::json::array();
  for (const auto& node : result.nodes) {
    analysis["nodes"].push_back({
        {"id", node.id},
        {"layer", node.layer + 1u},
        {"area_pixels", node.areaPixels},
        {"center_mm", {node.centerXMillimetres, node.centerYMillimetres}},
        {"equivalent_diameter_mm", node.equivalentDiameterMillimetres},
        {"kind", nodeKindName(node.kind)},
        {"rooted_in_raft", node.rootedInRaft},
        {"rooted_in_model", node.rootedInModel},
        {"terminal_taper", node.terminalTaper},
    });
  }
  analysis["edges"] = nlohmann::json::array();
  for (const auto& edge : result.edges) {
    analysis["edges"].push_back({
        {"lower_node", edge.lowerNode},
        {"upper_node", edge.upperNode},
        {"kind", edgeKindName(edge.kind)},
    });
  }

  nlohmann::json decisions;
  decisions["schema"] = "accloud.support-decisions.v1";
  decisions["input"] = metadata.inputFileName;
  decisions["decisions"] = nlohmann::json::array();
  for (const auto& decision : result.decisions) {
    decisions["decisions"].push_back(decisionJson(decision));
  }


  nlohmann::json overview = {
      {"schema", "accloud.support-analysis-summary.v1"},
      {"input", metadata.inputFileName},
      {"resolution", {source.width(), source.height()}},
      {"layer_count", source.layerCount()},
      {"pitch_mm",
       {metadata.pitchXMillimetres,
        metadata.pitchYMillimetres,
        metadata.pitchZMillimetres}},
      {"summary", analysis["summary"]},
      {"options", analysis["options"]},
      {"analysis_json", "analysis.json"},
      {"decisions_json", "decisions.json"},
  };

  if (!writeJsonFile(outputDirectory / "analysis.json", analysis, error)
      || !writeJsonFile(outputDirectory / "decisions.json", decisions, error)
      || !writeJsonFile(outputDirectory / "summary.json", overview, error)) {
    return false;
  }

  nlohmann::json manifest;
  manifest["schema"] = "accloud.support-analysis-bundle.v1";
  manifest["input"] = metadata.inputFileName;
  manifest["source_path"] = metadata.inputFileName;
  manifest["summary_json"] = "summary.json";
  manifest["analysis_json"] = "analysis.json";
  manifest["decisions_json"] = "decisions.json";
  manifest["image_downsample"] = options.downsample;
  manifest["layer_count"] = source.layerCount();
  manifest["images"] = nlohmann::json::array();

  for (std::size_t layer = 0; layer < source.layerCount(); ++layer) {
    if (callbacks.isCancelled && callbacks.isCancelled()) {
      error = "analysis bundle export cancelled";
      return false;
    }
    const auto fileName = layerFileName(layer + 1u);
    const auto jsonFileName = layerJsonFileName(layer + 1u);
    if (options.writeImages
        && !writeLayerDiagnostic(
            source, analyzer, result, layer, options.downsample,
            decisionsByLayer[layer], imagesDirectory / fileName, error)) {
      return false;
    }

    nlohmann::json layerJson = {
        {"schema", "accloud.support-analysis-layer.v1"},
        {"layer", layer + 1u},
        {"phase", phaseName(result.layers[layer].phase)},
        {"image", options.writeImages ? "images/" + fileName : ""},
        {"support_components", result.layers[layer].supportComponentIds.size()},
        {"projected_support_runs", result.layers[layer].projectedSupportRuns.size()},
        {"decisions", nlohmann::json::array()},
    };
    for (const auto* decision : decisionsByLayer[layer]) {
      if (decision != nullptr) {
        layerJson["decisions"].push_back(decisionJson(*decision));
      }
    }
    if (!writeJsonFile(layersDirectory / jsonFileName, layerJson, error)) {
      return false;
    }

    manifest["images"].push_back({
        {"layer", layer + 1u},
        {"path", options.writeImages ? "images/" + fileName : ""},
        {"layer_json", "layers/" + jsonFileName},
        {"decision_count", decisionsByLayer[layer].size()},
    });
    if (callbacks.progress) {
      callbacks.progress(layer + 1u, source.layerCount());
    }
  }

  return writeJsonFile(outputDirectory / "manifest.json", manifest, error);
}

} // namespace accloud::render3d
