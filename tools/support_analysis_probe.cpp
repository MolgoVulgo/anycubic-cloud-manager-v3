#include "domain/photons/BinaryMask.h"
#include "infra/photons/drivers/pwsz/PwszArchiveReader.h"
#include "render3d/analysis/SupportAnalyzer.h"
#include "render3d/analysis/SupportAnalysisDiagnostics.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

const char* phaseName(accloud::render3d::PrintPhase phase) {
  using accloud::render3d::PrintPhase;
  switch (phase) {
  case PrintPhase::Raft: return "raft";
  case PrintPhase::SupportsOnly: return "supports_only";
  case PrintPhase::ModelAndSupports: return "model_and_supports";
  case PrintPhase::ModelMostly: return "model_mostly";
  }
  return "unknown";
}

const char* nodeKindName(accloud::render3d::SupportNodeKind kind) {
  using accloud::render3d::SupportNodeKind;
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

struct DumpRequest {
  std::size_t layerOneBased = 0;
  std::filesystem::path outputPpm;
};

struct Arguments {
  std::filesystem::path input;
  std::optional<std::filesystem::path> outputJson;
  std::optional<std::filesystem::path> bundleDirectory;
  std::vector<DumpRequest> dumps;
  std::uint32_t downsample = 16;
  std::size_t workerCount = 1u;
  bool enableBitsetAcceleration = true;
  bool verifyMaterialization = false;
};

bool parseUnsigned(const char* value, std::size_t& output) {
  try {
    std::size_t consumed = 0;
    const auto parsed = std::stoull(value, &consumed);
    if (consumed != std::string(value).size()) {
      return false;
    }
    output = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

std::optional<Arguments> parseArguments(int argc, char** argv) {
  if (argc < 2) {
    return std::nullopt;
  }
  Arguments args;
  args.input = argv[1];
  std::size_t pendingDumpLayer = 0;
  for (int index = 2; index < argc; ++index) {
    const std::string option = argv[index];
    if (option == "--output" && index + 1 < argc) {
      args.outputJson = argv[++index];
    } else if (option == "--bundle" && index + 1 < argc) {
      args.bundleDirectory = argv[++index];
    } else if (option == "--dump-layer" && index + 1 < argc) {
      if (pendingDumpLayer != 0
          || !parseUnsigned(argv[++index], pendingDumpLayer)
          || pendingDumpLayer == 0) {
        return std::nullopt;
      }
    } else if (option == "--dump-ppm" && index + 1 < argc) {
      if (pendingDumpLayer == 0) {
        return std::nullopt;
      }
      args.dumps.push_back(DumpRequest{pendingDumpLayer, argv[++index]});
      pendingDumpLayer = 0;
    } else if (option == "--downsample" && index + 1 < argc) {
      std::size_t parsed = 0;
      if (!parseUnsigned(argv[++index], parsed) || parsed == 0
          || parsed > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
      }
      args.downsample = static_cast<std::uint32_t>(parsed);
    } else if (option == "--workers" && index + 1 < argc) {
      if (!parseUnsigned(argv[++index], args.workerCount)
          || args.workerCount < accloud::render3d::kMinimumSupportAnalysisWorkerCount
          || args.workerCount > accloud::render3d::kMaximumSupportAnalysisWorkerCount) {
        return std::nullopt;
      }
    } else if (option == "--no-bitsets") {
      args.enableBitsetAcceleration = false;
    } else if (option == "--verify-materialization") {
      args.verifyMaterialization = true;
    } else if (!args.outputJson && option.rfind("--", 0) != 0) {
      // Backward-compatible positional JSON output.
      args.outputJson = option;
    } else {
      return std::nullopt;
    }
  }
  if (pendingDumpLayer != 0) {
    return std::nullopt;
  }
  return args;
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

Bounds materialBounds(const accloud::photons::BinaryMask& material) {
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

bool writeLayerPpm(
    accloud::photons::pwsz::PwszArchiveReader& reader,
    const accloud::render3d::SupportAnalyzer& analyzer,
    const accloud::render3d::LayerSemanticIndex& semantics,
    std::size_t layer,
    std::uint32_t downsample,
    const std::filesystem::path& path,
    std::string& error) {
  auto material = reader.loadMask(layer, error);
  if (!material) {
    return false;
  }
  std::vector<accloud::render3d::SemanticRun> semanticRuns;
  if (!analyzer.materializeLayerSemantics(*material, semantics, semanticRuns, error)) {
    return false;
  }

  Bounds bounds = materialBounds(*material);
  if (bounds.empty()) {
    error = "selected layer is empty";
    return false;
  }
  const std::uint32_t margin = 16u * downsample;
  bounds.minX = bounds.minX > margin ? bounds.minX - margin : 0u;
  bounds.minY = bounds.minY > margin ? bounds.minY - margin : 0u;
  bounds.maxX = std::min(material->width(), bounds.maxX + margin);
  bounds.maxY = std::min(material->height(), bounds.maxY + margin);
  const auto outputWidth = (bounds.maxX - bounds.minX + downsample - 1u) / downsample;
  const auto outputHeight = (bounds.maxY - bounds.minY + downsample - 1u) / downsample;
  std::vector<std::uint8_t> pixels(
      static_cast<std::size_t>(outputWidth) * outputHeight, 0u);

  const auto markRange = [&](std::uint32_t y,
                             std::uint32_t firstX,
                             std::uint32_t lastX,
                             std::uint8_t value) {
    if (y < bounds.minY || y >= bounds.maxY
        || lastX <= bounds.minX || firstX >= bounds.maxX) {
      return;
    }
    const auto first = std::max(firstX, bounds.minX);
    const auto last = std::min(lastX, bounds.maxX);
    if (first >= last) {
      return;
    }
    const auto firstBlock = (first - bounds.minX) / downsample;
    const auto lastBlock = (last - 1u - bounds.minX) / downsample;
    const auto outputY = (y - bounds.minY) / downsample;
    for (std::uint32_t outputX = firstBlock; outputX <= lastBlock; ++outputX) {
      auto& destination = pixels[static_cast<std::size_t>(outputY) * outputWidth + outputX];
      destination = std::max(destination, value);
    }
  };

  // Rasterise contiguous material runs directly into downsampled output cells.
  // This avoids one mark operation per exposed pixel on dense real-world layers.
  for (std::uint32_t y = bounds.minY; y < bounds.maxY; ++y) {
    const auto firstWord = bounds.minX / 64u;
    const auto lastWord = (bounds.maxX + 63u) / 64u;
    for (std::size_t wordIndex = firstWord; wordIndex < lastWord; ++wordIndex) {
      const auto wordFirstX = static_cast<std::uint32_t>(wordIndex * 64u);
      const auto clippedFirstX = std::max(bounds.minX, wordFirstX);
      const auto clippedLastX = std::min(bounds.maxX, wordFirstX + 64u);
      if (clippedFirstX >= clippedLastX) {
        continue;
      }

      std::uint64_t word = material->rowWord(y, wordIndex);
      const auto firstBit = clippedFirstX - wordFirstX;
      const auto lastBit = clippedLastX - wordFirstX;
      const auto lowerMask = firstBit == 0u
                                 ? std::numeric_limits<std::uint64_t>::max()
                                 : std::numeric_limits<std::uint64_t>::max() << firstBit;
      const auto upperMask = lastBit == 64u
                                 ? std::numeric_limits<std::uint64_t>::max()
                                 : (std::uint64_t{1} << lastBit) - 1u;
      word &= lowerMask & upperMask;

      while (word != 0u) {
        const auto firstSet = static_cast<std::uint32_t>(std::countr_zero(word));
        const auto shifted = word >> firstSet;
        const auto runLength = static_cast<std::uint32_t>(std::countr_one(shifted));
        markRange(y, wordFirstX + firstSet,
                  wordFirstX + firstSet + runLength, 1u);
        if (runLength == 64u - firstSet) {
          word = 0u;
        } else {
          const auto runMask = ((std::uint64_t{1} << runLength) - 1u) << firstSet;
          word &= ~runMask;
        }
      }
    }
  }

  for (const auto& run : semanticRuns) {
    const std::uint8_t value = run.semantic == accloud::render3d::MaterialSemantic::Raft
                                   ? 2u
                                   : 3u;
    if (run.y < bounds.minY || run.y >= bounds.maxY
        || run.lastX <= bounds.minX || run.firstX >= bounds.maxX) {
      continue;
    }
    markRange(run.y, run.firstX, run.lastX, value);
  }

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open PPM output";
    return false;
  }
  file << "P6\n" << outputWidth << ' ' << outputHeight << "\n255\n";
  for (const auto value : pixels) {
    const unsigned char pixel[3] = {
        static_cast<unsigned char>(
            value == 3u ? 235 : value == 2u ? 245 : value == 1u ? 70 : 10),
        static_cast<unsigned char>(
            value == 3u ? 95 : value == 2u ? 190 : value == 1u ? 190 : 10),
        static_cast<unsigned char>(
            value == 3u ? 70 : value == 2u ? 45 : value == 1u ? 220 : 10),
    };
    file.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
  }
  return static_cast<bool>(file);
}

} // namespace

int main(int argc, char** argv) {
  const auto arguments = parseArguments(argc, argv);
  if (!arguments) {
    std::cerr << "usage: accloud_support_analysis_probe input.pwsz [output.json] "
                 "[--output file.json] [--bundle output-directory] "
                 "[--dump-layer N --dump-ppm file.ppm [--downsample N]] "
                 "[--workers N] [--no-bitsets] [--verify-materialization]\n";
    return 2;
  }

  accloud::photons::pwsz::PwszArchiveReader reader;
  std::string error;
  if (!reader.open(arguments->input, error)) {
    std::cerr << error << '\n';
    return 1;
  }

  const auto& meta = reader.meta();
  accloud::render3d::SupportAnalysisOptions options;
  options.pitchXMillimetres = meta.pitchXMm.value_or(meta.pitchXYMm.value_or(1.0));
  options.pitchYMillimetres = meta.pitchYMm.value_or(meta.pitchXYMm.value_or(
      options.pitchXMillimetres));
  options.pitchZMillimetres = meta.pitchZMm.value_or(1.0);
  options.workerCount = arguments->workerCount;
  options.enableBitsetAcceleration = arguments->enableBitsetAcceleration;
  options.captureDecisionTrace = arguments->bundleDirectory.has_value();

  accloud::render3d::SupportAnalyzer analyzer;
  accloud::render3d::SupportAnalysisCallbacks analysisCallbacks;
  analysisCallbacks.progress = [](std::size_t completed, std::size_t total) {
    std::cerr << "ANALYZE_PROGRESS " << completed << ' ' << total << '\n';
  };
  const auto result = analyzer.analyze(reader, options, analysisCallbacks);
  if (!result.ok) {
    std::cerr << (result.error.empty() ? "support analysis failed" : result.error) << '\n';
    return result.cancelled ? 3 : 1;
  }

  bool materializationVerified = false;
  if (arguments->verifyMaterialization) {
    std::size_t supportRuns = 0;
    std::size_t raftRuns = 0;
    for (std::size_t layer = 0; layer < result.layers.size(); ++layer) {
      if ((layer % 100u) == 0u) {
        std::cerr << "verify layer " << (layer + 1u) << '/' << result.layers.size() << '\n';
      }
      auto mask = reader.loadMask(layer, error);
      if (!mask) {
        std::cerr << (error.empty() ? "cannot reload verification layer" : error) << '\n';
        return 1;
      }
      std::vector<accloud::render3d::SemanticRun> runs;
      if (!analyzer.materializeLayerSemantics(
              *mask, result.layers[layer], runs, error)) {
        std::cerr << error << '\n';
        return 1;
      }
      for (const auto& run : runs) {
        supportRuns += run.semantic == accloud::render3d::MaterialSemantic::Support;
        raftRuns += run.semantic == accloud::render3d::MaterialSemantic::Raft;
      }
    }
    materializationVerified = supportRuns == result.summary.supportRunCount
                              && raftRuns == result.summary.raftRunCount;
    if (!materializationVerified) {
      std::cerr << "semantic materialization count mismatch" << '\n';
      return 1;
    }
  }

  if (arguments->bundleDirectory) {
    accloud::render3d::SupportAnalysisBundleWriter bundleWriter;
    accloud::render3d::SupportAnalysisBundleMetadata bundleMetadata;
    bundleMetadata.inputFileName = arguments->input.string();
    bundleMetadata.pitchXMillimetres = options.pitchXMillimetres;
    bundleMetadata.pitchYMillimetres = options.pitchYMillimetres;
    bundleMetadata.pitchZMillimetres = options.pitchZMillimetres;
    accloud::render3d::SupportAnalysisBundleOptions bundleOptions;
    bundleOptions.downsample = arguments->downsample;
    bundleOptions.writeImages = true;
    accloud::render3d::SupportAnalysisBundleCallbacks bundleCallbacks;
    bundleCallbacks.progress = [](std::size_t completed, std::size_t total) {
      std::cerr << "IMAGE_PROGRESS " << completed << ' ' << total << '\n';
    };
    if (!bundleWriter.write(
            reader, analyzer, result, options, bundleMetadata,
            *arguments->bundleDirectory, bundleOptions, bundleCallbacks, error)) {
      std::cerr << error << '\n';
      return 1;
    }
  }

  nlohmann::json output;
  output["input"] = arguments->input.filename().string();
  output["materialization_verified"] = materializationVerified;
  output["resolution"] = {reader.width(), reader.height()};
  output["layer_count"] = reader.layerCount();
  output["analysis_workers"] = arguments->workerCount;
  output["bitset_acceleration"] = arguments->enableBitsetAcceleration;
  output["pitch_mm"] = {
      options.pitchXMillimetres,
      options.pitchYMillimetres,
      options.pitchZMillimetres,
  };
  output["summary"] = {
      {"raft_last_layer", result.summary.raftLastLayer + 1u},
      {"first_model_layer", result.summary.firstModelLayer + 1u},
      {"last_support_layer", result.summary.lastSupportLayer + 1u},
      {"components", result.summary.componentCount},
      {"candidate_nodes", result.summary.candidateNodeCount},
      {"accepted_nodes", result.summary.acceptedNodeCount},
      {"raft_runs", result.summary.raftRunCount},
      {"support_runs", result.summary.supportRunCount},
      {"free_support_runs", result.summary.freeSupportRunCount},
      {"projected_support_runs", result.summary.projectedSupportRunCount},
      {"projected_contact_pixels", result.summary.projectedContactPixelCount},
      {"rejected_projection_runs", result.summary.rejectedProjectionRunCount},
      {"rejected_growth_pixels", result.summary.rejectedGrowthPixelCount},
      {"untapered_model_contacts", result.summary.untaperedModelContactCount},
      {"contacts_without_valid_projection",
       result.summary.contactsWithoutValidProjectionCount},
      {"maximum_contact_growth_ratio", result.summary.maximumContactGrowthRatio},
      {"terminal_support_stops", result.summary.terminalSupportStopCount},
      {"expanding_model_contacts", result.summary.expandingModelContactCount},
      {"maximum_model_expansion_ratio", result.summary.maximumModelExpansionRatio},
      {"continuations", result.summary.continuationEdgeCount},
      {"splits", result.summary.splitEdgeCount},
      {"braces", result.summary.braceEdgeCount},
      {"model_contacts", result.summary.modelContactEdgeCount},
      {"forced_semantic_samples", result.summary.forcedSemanticSampleCount},
      {"reverse_model_seeds", result.summary.reverseModelSeedCount},
      {"reverse_model_continuations", result.summary.reverseModelContinuationCount},
      {"bidirectional_mixed_components",
       result.summary.bidirectionalMixedComponentCount},
  };

  output["forced_sample_layers"] = nlohmann::json::array();
  for (const auto layer : result.forcedSampleLayers) {
    output["forced_sample_layers"].push_back(layer + 1u);
  }

  output["node_kinds"] = nlohmann::json::object();
  for (const auto& node : result.nodes) {
    const auto* name = nodeKindName(node.kind);
    output["node_kinds"][name] = output["node_kinds"].value(name, 0u) + 1u;
  }

  output["layers"] = nlohmann::json::array();
  for (const auto& layer : result.layers) {
    output["layers"].push_back({
        {"layer", layer.layer + 1u},
        {"phase", phaseName(layer.phase)},
        {"support_components", layer.supportComponentIds.size()},
        {"projected_support_runs", layer.projectedSupportRuns.size()},
    });
  }

  if (arguments->outputJson) {
    std::ofstream file(*arguments->outputJson);
    if (!file) {
      std::cerr << "cannot open output JSON\n";
      return 1;
    }
    file << output.dump(2) << '\n';
  }
  for (const auto& dump : arguments->dumps) {
    if (dump.layerOneBased < 1 || dump.layerOneBased > result.layers.size()) {
      std::cerr << "dump layer is outside the archive\n";
      return 2;
    }
    const auto layer = dump.layerOneBased - 1u;
    if (!writeLayerPpm(reader, analyzer, result.layers[layer], layer, arguments->downsample,
                       dump.outputPpm, error)) {
      std::cerr << error << '\n';
      return 1;
    }
  }
  std::cout << output["summary"].dump() << '\n';
  return 0;
}
