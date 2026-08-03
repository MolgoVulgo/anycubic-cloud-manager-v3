#include "domain/photons/BinaryMask.h"
#include "domain/photons/LayerMaskSource.h"
#include "domain/photons/LayerRange.h"
#include "domain/photons/MeshChunk.h"
#include "infra/photons/drivers/pwsz/PwszArchiveReader.h"
#include "render3d/meshing/LayerStackMesher.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using accloud::photons::BinaryMask;
using accloud::photons::LayerMaskSource;
using accloud::photons::LayerRange;
using accloud::photons::MeshChunk;
using accloud::render3d::CutSurfaceMode;
using accloud::render3d::LayerStackMesher;
using accloud::render3d::MeshBuildCallbacks;
using accloud::render3d::MeshBuildOptions;
using accloud::render3d::MeshBuildResult;

struct BenchmarkOptions {
  std::filesystem::path inputPath;
  std::vector<std::size_t> workerCounts{4, 8, 16};
  std::size_t repeats = 1;
  std::size_t warmupRuns = 0;
  std::size_t layerStride = 2;
  std::vector<std::size_t> chunkLayerCounts{8, 16, 32};
  std::optional<std::pair<std::size_t, std::size_t>> oneBasedRange;
  std::optional<std::filesystem::path> outputPrefix;
  bool selfTest = false;
  bool showHelp = false;
};

struct GeometrySignature {
  std::size_t chunks = 0;
  std::size_t surfaceQuads = 0;
  std::size_t legacyVertices = 0;
  std::size_t triangles = 0;
  std::uint64_t compactBytes = 0;
  std::uint64_t legacyEquivalentBytes = 0;

  [[nodiscard]] bool operator==(const GeometrySignature&) const noexcept = default;
};

struct BenchmarkRun {
  std::size_t requestedWorkers = 0;
  std::size_t effectiveWorkers = 0;
  std::size_t repeatIndex = 0;
  std::size_t chunkLayerCount = 0;
  std::size_t decodedLayers = 0;
  std::size_t workerTasks = 0;
  std::uint64_t largestChunkBytes = 0;
  std::uint64_t minimumWorkerDurationMs = 0;
  std::uint64_t maximumWorkerDurationMs = 0;
  double durationMs = 0.0;
  double firstChunkMs = 0.0;
  GeometrySignature geometry;
  bool ok = false;
  std::string error;
};

struct WorkerSummary {
  std::size_t chunkLayerCount = 0;
  std::size_t requestedWorkers = 0;
  std::size_t effectiveWorkers = 0;
  std::size_t sampleCount = 0;
  double minimumMs = 0.0;
  double medianMs = 0.0;
  double averageMs = 0.0;
  double maximumMs = 0.0;
  double averageFirstChunkMs = 0.0;
  double speedup = 0.0;
  double efficiency = 0.0;
};

void printUsage(std::ostream& output) {
  output
      << "Usage:\n"
      << "  accloud_render3d_worker_benchmark --input <file.pwsz> [options]\n\n"
      << "Options:\n"
      << "  --workers <list>          Worker counts, default: 4,8,16\n"
      << "  --repeats <n>             Measured runs per matrix combination, default: 1\n"
      << "  --warmup-runs <n>         Unreported warm-up runs per matrix combination, default: 0\n"
      << "  --layer-stride <n>        Z sampling stride, default: 2\n"
      << "  --chunk-layers <list>     Chunk sizes, default: 8,16,32\n"
      << "  --range <first:last>      Inclusive one-based layer range, default: all\n"
      << "  --output-prefix <path>    Write <path>.csv and <path>.jsonl\n"
      << "  --self-test               Run a small synthetic benchmark\n"
      << "  --help                    Show this help\n\n"
      << "The benchmark executes every chunk-size/worker-count combination. It measures\n"
      << "PWSZ decoding plus CPU mesh generation and discards each completed chunk\n"
      << "immediately, so GPU upload and rendering are excluded.\n";
}

std::optional<std::size_t> parseSize(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  std::size_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::vector<std::size_t>> parseWorkers(std::string_view text) {
  std::vector<std::size_t> values;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t separator = text.find(',', start);
    const std::size_t end = separator == std::string_view::npos ? text.size() : separator;
    const auto value = parseSize(text.substr(start, end - start));
    if (!value || *value < accloud::render3d::kMinimumMeshWorkerCount
        || *value > accloud::render3d::kMaximumMeshWorkerCount
        || std::find(values.begin(), values.end(), *value) != values.end()) {
      return std::nullopt;
    }
    values.push_back(*value);
    if (separator == std::string_view::npos) {
      break;
    }
    start = separator + 1;
  }
  if (values.empty()) {
    return std::nullopt;
  }
  return values;
}

std::optional<std::vector<std::size_t>> parseChunkLayers(std::string_view text) {
  std::vector<std::size_t> values;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t separator = text.find(',', start);
    const std::size_t end = separator == std::string_view::npos ? text.size() : separator;
    const auto value = parseSize(text.substr(start, end - start));
    if (!value || *value == 0
        || *value > accloud::photons::kPackedSurfaceMaximumRelativeZ
        || std::find(values.begin(), values.end(), *value) != values.end()) {
      return std::nullopt;
    }
    values.push_back(*value);
    if (separator == std::string_view::npos) {
      break;
    }
    start = separator + 1;
  }
  if (values.empty()) {
    return std::nullopt;
  }
  return values;
}

std::optional<std::pair<std::size_t, std::size_t>> parseRange(std::string_view text) {
  const std::size_t separator = text.find(':');
  if (separator == std::string_view::npos || text.find(':', separator + 1) != std::string_view::npos) {
    return std::nullopt;
  }
  const auto first = parseSize(text.substr(0, separator));
  const auto last = parseSize(text.substr(separator + 1));
  if (!first || !last || *first == 0 || *last == 0 || *first > *last) {
    return std::nullopt;
  }
  return std::pair{*first, *last};
}

bool parseArguments(
    std::span<const std::string_view> arguments,
    BenchmarkOptions& options,
    std::string& error) {
  const auto requireValue = [&](std::size_t& index, std::string_view option)
      -> std::optional<std::string_view> {
    if (index + 1 >= arguments.size()) {
      error = std::string(option) + " requires a value";
      return std::nullopt;
    }
    ++index;
    return arguments[index];
  };

  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::string_view argument = arguments[index];
    if (argument == "--help" || argument == "-h") {
      options.showHelp = true;
    } else if (argument == "--self-test") {
      options.selfTest = true;
    } else if (argument == "--input") {
      const auto value = requireValue(index, argument);
      if (!value) {
        return false;
      }
      options.inputPath = std::filesystem::path(*value);
    } else if (argument == "--workers") {
      const auto value = requireValue(index, argument);
      if (!value) {
        return false;
      }
      const auto workers = parseWorkers(*value);
      if (!workers) {
        error = "--workers must contain unique values in the 1..16 range";
        return false;
      }
      options.workerCounts = *workers;
    } else if (argument == "--repeats") {
      const auto value = requireValue(index, argument);
      if (!value) {
        return false;
      }
      const auto parsed = parseSize(*value);
      if (!parsed || *parsed < 1 || *parsed > 20) {
        error = "--repeats must be between 1 and 20";
        return false;
      }
      options.repeats = *parsed;
    } else if (argument == "--warmup-runs") {
      const auto value = requireValue(index, argument);
      if (!value) {
        return false;
      }
      const auto parsed = parseSize(*value);
      if (!parsed || *parsed > 5) {
        error = "--warmup-runs must be between 0 and 5";
        return false;
      }
      options.warmupRuns = *parsed;
    } else if (argument == "--layer-stride") {
      const auto value = requireValue(index, argument);
      if (!value) {
        return false;
      }
      const auto parsed = parseSize(*value);
      if (!parsed || *parsed == 0) {
        error = "--layer-stride must be positive";
        return false;
      }
      options.layerStride = *parsed;
    } else if (argument == "--chunk-layers") {
      const auto value = requireValue(index, argument);
      if (!value) {
        return false;
      }
      const auto parsed = parseChunkLayers(*value);
      if (!parsed) {
        error = "--chunk-layers must contain unique values in the packed 1..63 range";
        return false;
      }
      options.chunkLayerCounts = *parsed;
    } else if (argument == "--range") {
      const auto value = requireValue(index, argument);
      if (!value) {
        return false;
      }
      const auto parsed = parseRange(*value);
      if (!parsed) {
        error = "--range must use the inclusive one-based first:last format";
        return false;
      }
      options.oneBasedRange = *parsed;
    } else if (argument == "--output-prefix") {
      const auto value = requireValue(index, argument);
      if (!value || value->empty()) {
        error = "--output-prefix requires a non-empty path";
        return false;
      }
      options.outputPrefix = std::filesystem::path(*value);
    } else {
      error = "unknown option: " + std::string(argument);
      return false;
    }
  }

  if (!options.showHelp && !options.selfTest && options.inputPath.empty()) {
    error = "--input is required unless --self-test is used";
    return false;
  }
  return true;
}

std::uint64_t chunkBytes(const MeshChunk& chunk) {
  return static_cast<std::uint64_t>(chunk.compactByteSize());
}

BenchmarkRun runBenchmark(
    LayerMaskSource& source,
    LayerRange range,
    const MeshBuildOptions& commonOptions,
    std::size_t workerCount,
    std::size_t repeatIndex) {
  BenchmarkRun run;
  run.requestedWorkers = workerCount;
  run.repeatIndex = repeatIndex;
  run.chunkLayerCount = commonOptions.chunkLayerCount;

  MeshBuildOptions options = commonOptions;
  options.workerCount = workerCount;

  bool firstChunkSeen = false;
  const auto started = std::chrono::steady_clock::now();
  MeshBuildCallbacks callbacks;
  callbacks.consumeChunk = [&](MeshChunk&& chunk) {
    const auto now = std::chrono::steady_clock::now();
    if (!firstChunkSeen) {
      firstChunkSeen = true;
      run.firstChunkMs = std::chrono::duration<double, std::milli>(now - started).count();
    }
    ++run.geometry.chunks;
    run.geometry.surfaceQuads += chunk.surfaceQuadCount();
    run.geometry.legacyVertices += chunk.legacyVertexCount();
    run.geometry.triangles += chunk.triangleCount();
    const std::uint64_t bytes = chunkBytes(chunk);
    run.geometry.compactBytes += bytes;
    run.geometry.legacyEquivalentBytes += chunk.legacyEquivalentByteSize();
    run.largestChunkBytes = std::max(run.largestChunkBytes, bytes);
    return true;
  };

  LayerStackMesher mesher;
  const MeshBuildResult result = mesher.build(source, range, options, callbacks);
  run.durationMs = std::chrono::duration<double, std::milli>(
                       std::chrono::steady_clock::now() - started)
                       .count();
  run.ok = result.ok;
  run.error = result.error;
  run.effectiveWorkers = result.effectiveWorkerCount;
  run.decodedLayers = result.decodedLayerCount;
  if (!result.workerStats.empty()) {
    run.minimumWorkerDurationMs = result.workerStats.front().durationMs;
    for (const auto& stats : result.workerStats) {
      run.workerTasks += stats.taskCount;
      run.minimumWorkerDurationMs = std::min(run.minimumWorkerDurationMs, stats.durationMs);
      run.maximumWorkerDurationMs = std::max(run.maximumWorkerDurationMs, stats.durationMs);
    }
  }
  return run;
}

std::vector<WorkerSummary> summarize(const std::vector<BenchmarkRun>& runs) {
  using GroupKey = std::pair<std::size_t, std::size_t>;
  std::map<GroupKey, std::vector<const BenchmarkRun*>> grouped;
  for (const auto& run : runs) {
    if (run.ok) {
      grouped[{run.chunkLayerCount, run.requestedWorkers}].push_back(&run);
    }
  }

  std::vector<WorkerSummary> summaries;
  for (const auto& [key, samples] : grouped) {
    std::vector<double> durations;
    durations.reserve(samples.size());
    double firstChunkTotal = 0.0;
    for (const auto* sample : samples) {
      durations.push_back(sample->durationMs);
      firstChunkTotal += sample->firstChunkMs;
    }
    std::sort(durations.begin(), durations.end());
    WorkerSummary summary;
    summary.chunkLayerCount = key.first;
    summary.requestedWorkers = key.second;
    summary.effectiveWorkers = samples.front()->effectiveWorkers;
    summary.sampleCount = samples.size();
    summary.minimumMs = durations.front();
    summary.maximumMs = durations.back();
    summary.averageMs = std::accumulate(durations.begin(), durations.end(), 0.0)
                        / static_cast<double>(durations.size());
    const std::size_t middle = durations.size() / 2;
    summary.medianMs = durations.size() % 2 == 0
                           ? (durations[middle - 1] + durations[middle]) / 2.0
                           : durations[middle];
    summary.averageFirstChunkMs = firstChunkTotal / static_cast<double>(samples.size());
    summaries.push_back(summary);
  }

  std::map<std::size_t, std::pair<double, std::size_t>> baselines;
  for (const auto& summary : summaries) {
    if (!baselines.contains(summary.chunkLayerCount)) {
      baselines.emplace(
          summary.chunkLayerCount,
          std::pair{summary.averageMs, summary.requestedWorkers});
    }
  }
  for (auto& summary : summaries) {
    const auto baseline = baselines.at(summary.chunkLayerCount);
    summary.speedup = summary.averageMs > 0.0 ? baseline.first / summary.averageMs : 0.0;
    const double relativeWorkers = static_cast<double>(summary.requestedWorkers)
                                   / static_cast<double>(baseline.second);
    summary.efficiency = relativeWorkers > 0.0 ? summary.speedup / relativeWorkers : 0.0;
  }
  return summaries;
}

std::string csvEscape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('"');
  for (const char character : value) {
    if (character == '"') {
      escaped.push_back('"');
    }
    escaped.push_back(character);
  }
  escaped.push_back('"');
  return escaped;
}

std::string jsonEscape(std::string_view value) {
  std::ostringstream output;
  for (const unsigned char character : value) {
    switch (character) {
    case '"': output << "\\\""; break;
    case '\\': output << "\\\\"; break;
    case '\b': output << "\\b"; break;
    case '\f': output << "\\f"; break;
    case '\n': output << "\\n"; break;
    case '\r': output << "\\r"; break;
    case '\t': output << "\\t"; break;
    default:
      if (character < 0x20) {
        output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
               << static_cast<int>(character) << std::dec << std::setfill(' ');
      } else {
        output << static_cast<char>(character);
      }
    }
  }
  return output.str();
}

bool writeReports(
    const std::filesystem::path& prefix,
    std::string_view inputName,
    double archiveOpenMs,
    LayerRange range,
    const BenchmarkOptions& command,
    const std::vector<BenchmarkRun>& runs,
    const std::vector<WorkerSummary>& summaries,
    std::string& error) {
  std::error_code directoryError;
  const auto parent = prefix.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, directoryError);
    if (directoryError) {
      error = "cannot create benchmark output directory: " + directoryError.message();
      return false;
    }
  }

  const std::filesystem::path csvPath = prefix.string() + ".csv";
  const std::filesystem::path jsonlPath = prefix.string() + ".jsonl";
  std::ofstream csv(csvPath, std::ios::trunc);
  std::ofstream jsonl(jsonlPath, std::ios::trunc);
  if (!csv || !jsonl) {
    error = "cannot create benchmark report files";
    return false;
  }

  csv << "input,archive_open_ms,range_first,range_last,layer_stride,chunk_layers,repeat,"
         "requested_workers,effective_workers,duration_ms,first_chunk_ms,decoded_layers,chunks,"
         "surface_quads,legacy_vertices,triangles,compact_bytes,legacy_equivalent_bytes,compression_ratio,largest_chunk_bytes,worker_tasks,min_worker_ms,"
         "max_worker_ms,status,error\n";
  csv << std::fixed << std::setprecision(3);
  for (const auto& run : runs) {
    csv << csvEscape(inputName) << ',' << archiveOpenMs << ',' << (range.first + 1) << ','
        << (range.last + 1) << ',' << command.layerStride << ',' << run.chunkLayerCount << ','
        << run.repeatIndex << ',' << run.requestedWorkers << ',' << run.effectiveWorkers << ','
        << run.durationMs << ',' << run.firstChunkMs << ',' << run.decodedLayers << ','
        << run.geometry.chunks << ',' << run.geometry.surfaceQuads << ','
        << run.geometry.legacyVertices << ',' << run.geometry.triangles << ','
        << run.geometry.compactBytes << ',' << run.geometry.legacyEquivalentBytes << ','
        << (run.geometry.compactBytes == 0 ? 0.0
                                          : static_cast<double>(run.geometry.legacyEquivalentBytes)
                                                / static_cast<double>(run.geometry.compactBytes))
        << ',' << run.largestChunkBytes << ','
        << run.workerTasks << ',' << run.minimumWorkerDurationMs << ','
        << run.maximumWorkerDurationMs << ',' << (run.ok ? "ok" : "error") << ','
        << csvEscape(run.error) << '\n';
  }

  const auto writeArray = [](std::ostream& output, const std::vector<std::size_t>& values) {
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (index != 0) {
        output << ',';
      }
      output << values[index];
    }
    output << ']';
  };

  jsonl << std::fixed << std::setprecision(3);
  jsonl << "{\"type\":\"metadata\",\"input\":\"" << jsonEscape(inputName)
        << "\",\"archive_open_ms\":" << archiveOpenMs
        << ",\"range_first\":" << (range.first + 1)
        << ",\"range_last\":" << (range.last + 1)
        << ",\"layer_stride\":" << command.layerStride
        << ",\"chunk_layers\":";
  writeArray(jsonl, command.chunkLayerCounts);
  jsonl << ",\"workers\":";
  writeArray(jsonl, command.workerCounts);
  jsonl << "}\n";

  for (const auto& run : runs) {
    jsonl << "{\"type\":\"run\",\"input\":\"" << jsonEscape(inputName)
          << "\",\"chunk_layers\":" << run.chunkLayerCount
          << ",\"repeat\":" << run.repeatIndex
          << ",\"requested_workers\":" << run.requestedWorkers
          << ",\"effective_workers\":" << run.effectiveWorkers
          << ",\"duration_ms\":" << run.durationMs
          << ",\"first_chunk_ms\":" << run.firstChunkMs
          << ",\"decoded_layers\":" << run.decodedLayers
          << ",\"chunks\":" << run.geometry.chunks
          << ",\"surface_quads\":" << run.geometry.surfaceQuads
          << ",\"legacy_vertices\":" << run.geometry.legacyVertices
          << ",\"triangles\":" << run.geometry.triangles
          << ",\"compact_bytes\":" << run.geometry.compactBytes
          << ",\"legacy_equivalent_bytes\":" << run.geometry.legacyEquivalentBytes
          << ",\"compression_ratio\":"
          << (run.geometry.compactBytes == 0 ? 0.0
                                             : static_cast<double>(run.geometry.legacyEquivalentBytes)
                                                   / static_cast<double>(run.geometry.compactBytes))
          << ",\"largest_chunk_bytes\":" << run.largestChunkBytes
          << ",\"worker_tasks\":" << run.workerTasks
          << ",\"min_worker_ms\":" << run.minimumWorkerDurationMs
          << ",\"max_worker_ms\":" << run.maximumWorkerDurationMs
          << ",\"ok\":" << (run.ok ? "true" : "false")
          << ",\"error\":\"" << jsonEscape(run.error) << "\"}\n";
  }
  for (const auto& summary : summaries) {
    jsonl << "{\"type\":\"summary\",\"chunk_layers\":"
          << summary.chunkLayerCount << ",\"requested_workers\":"
          << summary.requestedWorkers << ",\"effective_workers\":"
          << summary.effectiveWorkers << ",\"samples\":" << summary.sampleCount
          << ",\"min_ms\":" << summary.minimumMs << ",\"median_ms\":"
          << summary.medianMs << ",\"average_ms\":" << summary.averageMs
          << ",\"max_ms\":" << summary.maximumMs
          << ",\"average_first_chunk_ms\":" << summary.averageFirstChunkMs
          << ",\"speedup\":" << summary.speedup
          << ",\"efficiency\":" << summary.efficiency << "}\n";
  }
  return true;
}

void printRun(const BenchmarkRun& run) {
  std::cout << std::fixed << std::setprecision(1)
            << "chunk_layers=" << run.chunkLayerCount
            << " workers=" << run.requestedWorkers
            << " effective=" << run.effectiveWorkers
            << " repeat=" << run.repeatIndex
            << " duration_ms=" << run.durationMs
            << " first_chunk_ms=" << run.firstChunkMs
            << " triangles=" << run.geometry.triangles
            << " compact_mib=" << (static_cast<double>(run.geometry.compactBytes) / (1024.0 * 1024.0))
            << " compression="
            << (run.geometry.compactBytes == 0 ? 0.0
                                               : static_cast<double>(run.geometry.legacyEquivalentBytes)
                                                     / static_cast<double>(run.geometry.compactBytes))
            << " status=" << (run.ok ? "ok" : "error") << '\n';
  std::cout.flush();
}

void printSummary(const std::vector<WorkerSummary>& summaries) {
  std::cout << "\nSummary (baseline = lowest requested worker count for each chunk size):\n";
  std::cout << "chunks workers effective samples average_ms median_ms first_chunk_ms speedup efficiency\n";
  std::cout << std::fixed << std::setprecision(2);
  for (const auto& summary : summaries) {
    std::cout << std::setw(6) << summary.chunkLayerCount << ' '
              << std::setw(7) << summary.requestedWorkers << ' '
              << std::setw(9) << summary.effectiveWorkers << ' '
              << std::setw(7) << summary.sampleCount << ' '
              << std::setw(10) << summary.averageMs << ' '
              << std::setw(9) << summary.medianMs << ' '
              << std::setw(14) << summary.averageFirstChunkMs << ' '
              << std::setw(7) << summary.speedup << ' '
              << std::setw(10) << summary.efficiency << '\n';
  }
}

class SyntheticSource final : public LayerMaskSource {
public:
  SyntheticSource(std::uint32_t width, std::uint32_t height, std::size_t layerCount)
      : width_(width), height_(height) {
    layers_.reserve(layerCount);
    for (std::size_t layer = 0; layer < layerCount; ++layer) {
      BinaryMask mask(width, height);
      const std::uint32_t inset = static_cast<std::uint32_t>(layer % 4);
      for (std::uint32_t y = 8 + inset; y < height - 8 - inset; ++y) {
        for (std::uint32_t x = 8 + inset; x < width - 8 - inset; ++x) {
          if (x < width / 2 - 3 || x > width / 2 + 3 || y < height / 2 - 3
              || y > height / 2 + 3) {
            mask.setUnchecked(x, y, true);
          }
        }
      }
      layers_.push_back(std::move(mask));
    }
  }

  [[nodiscard]] std::size_t layerCount() const noexcept override { return layers_.size(); }
  [[nodiscard]] std::uint32_t width() const noexcept override { return width_; }
  [[nodiscard]] std::uint32_t height() const noexcept override { return height_; }
  [[nodiscard]] bool supportsConcurrentMaskLoads() const noexcept override { return true; }
  [[nodiscard]] std::optional<BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) override {
    if (layerNumber >= layers_.size()) {
      error = "synthetic layer outside source";
      return std::nullopt;
    }
    return layers_[layerNumber];
  }

private:
  std::uint32_t width_ = 0;
  std::uint32_t height_ = 0;
  std::vector<BinaryMask> layers_;
};

int runSelfTest() {
  const BenchmarkOptions defaults;
  if (defaults.workerCounts != std::vector<std::size_t>{4, 8, 16}
      || defaults.chunkLayerCounts != std::vector<std::size_t>{8, 16, 32}
      || MeshBuildOptions{}.chunkLayerCount != 8) {
    std::cerr << "self-test benchmark defaults are inconsistent\n";
    return 1;
  }

  SyntheticSource source(64, 64, 64);
  MeshBuildOptions options;
  options.pitchXMm = 0.05;
  options.pitchYMm = 0.05;
  options.pitchZMm = 0.05;
  options.layerStride = 1;
  options.cutSurfaceMode = CutSurfaceMode::Open;

  std::vector<BenchmarkRun> runs;
  std::map<std::size_t, GeometrySignature> expectedGeometryByChunkSize;
  for (const std::size_t chunkLayers : defaults.chunkLayerCounts) {
    options.chunkLayerCount = chunkLayers;
    for (const std::size_t workers : defaults.workerCounts) {
      auto run = runBenchmark(source, {0, 63}, options, workers, 1);
      printRun(run);
      if (!run.ok) {
        std::cerr << "self-test benchmark failed: " << run.error << '\n';
        return 1;
      }
      const auto [expected, inserted] = expectedGeometryByChunkSize.emplace(
          chunkLayers, run.geometry);
      if (!inserted && run.geometry != expected->second) {
        std::cerr << "self-test geometry differs between worker counts for chunk size "
                  << chunkLayers << '\n';
        return 1;
      }
      runs.push_back(std::move(run));
    }
  }

  if (runs.size() != 9 || expectedGeometryByChunkSize.size() != 3
      || expectedGeometryByChunkSize.at(8).chunks != 8
      || expectedGeometryByChunkSize.at(16).chunks != 4
      || expectedGeometryByChunkSize.at(32).chunks != 2) {
    std::cerr << "self-test benchmark matrix is incomplete\n";
    return 1;
  }
  for (const auto& [chunkLayers, signature] : expectedGeometryByChunkSize) {
    if (signature.triangles == 0) {
      std::cerr << "self-test generated no triangles for chunk size " << chunkLayers << '\n';
      return 1;
    }
    if (signature.compactBytes == 0
        || signature.legacyEquivalentBytes != signature.compactBytes * 15u) {
      std::cerr << "self-test compact ratio differs from 15:1 for chunk size "
                << chunkLayers << '\n';
      return 1;
    }
  }

  const auto summaries = summarize(runs);
  if (summaries.size() != 9) {
    std::cerr << "self-test summary does not cover the full benchmark matrix\n";
    return 1;
  }
  printSummary(summaries);
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  std::vector<std::string_view> arguments;
  arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);
  for (int index = 1; index < argc; ++index) {
    arguments.emplace_back(argv[index]);
  }

  BenchmarkOptions command;
  std::string error;
  if (!parseArguments(arguments, command, error)) {
    std::cerr << "error: " << error << "\n\n";
    printUsage(std::cerr);
    return 2;
  }
  if (command.showHelp) {
    printUsage(std::cout);
    return 0;
  }
  if (command.selfTest) {
    return runSelfTest();
  }

  accloud::photons::pwsz::PwszArchiveReader reader;
  const auto archiveOpenStarted = std::chrono::steady_clock::now();
  if (!reader.open(command.inputPath, error)) {
    std::cerr << "error: cannot open PWSZ: " << error << '\n';
    return 1;
  }
  const double archiveOpenMs = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - archiveOpenStarted)
                                   .count();

  if (reader.layerCount() == 0) {
    std::cerr << "error: PWSZ contains no layers\n";
    return 1;
  }

  LayerRange range{0, reader.layerCount() - 1};
  if (command.oneBasedRange) {
    const auto converted = LayerRange::fromOneBased(
        command.oneBasedRange->first,
        command.oneBasedRange->second,
        reader.layerCount());
    if (!converted) {
      std::cerr << "error: selected layer range is outside the PWSZ\n";
      return 2;
    }
    range = *converted;
  }

  const auto& meta = reader.meta();
  const double pitchX = meta.pitchXMm.value_or(meta.pitchXYMm.value_or(1.0));
  const double pitchY = meta.pitchYMm.value_or(meta.pitchXYMm.value_or(pitchX));
  const double pitchZ = meta.pitchZMm.value_or(1.0);

  MeshBuildOptions meshOptions;
  meshOptions.pitchXMm = pitchX;
  meshOptions.pitchYMm = pitchY;
  meshOptions.pitchZMm = pitchZ;
  meshOptions.layerStride = command.layerStride;
  meshOptions.cutSurfaceMode = CutSurfaceMode::Open;

  const auto formatList = [](const std::vector<std::size_t>& values) {
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (index != 0) {
        output << ',';
      }
      output << values[index];
    }
    return output.str();
  };

  const std::string inputName = command.inputPath.filename().string();
  std::cout << "input=" << inputName
            << " layers=" << reader.layerCount()
            << " range=" << (range.first + 1) << ':' << (range.last + 1)
            << " resolution=" << reader.width() << 'x' << reader.height()
            << " stride=" << meshOptions.layerStride
            << " chunk_layers=" << formatList(command.chunkLayerCounts)
            << " workers=" << formatList(command.workerCounts)
            << " archive_open_ms=" << std::fixed << std::setprecision(1) << archiveOpenMs
            << '\n';

  using Combination = std::pair<std::size_t, std::size_t>;
  std::vector<Combination> matrix;
  matrix.reserve(command.chunkLayerCounts.size() * command.workerCounts.size());
  for (const std::size_t chunkLayers : command.chunkLayerCounts) {
    for (const std::size_t workers : command.workerCounts) {
      matrix.emplace_back(chunkLayers, workers);
    }
  }

  for (const auto [chunkLayers, workers] : matrix) {
    MeshBuildOptions runOptions = meshOptions;
    runOptions.chunkLayerCount = chunkLayers;
    for (std::size_t warmup = 0; warmup < command.warmupRuns; ++warmup) {
      std::cout << "warmup chunk_layers=" << chunkLayers << " workers=" << workers
                << " run=" << (warmup + 1) << '\n';
      const auto warmupRun = runBenchmark(reader, range, runOptions, workers, 0);
      if (!warmupRun.ok) {
        std::cerr << "error: warm-up failed for chunk_layers=" << chunkLayers
                  << " workers=" << workers << ": " << warmupRun.error << '\n';
        return 1;
      }
    }
  }

  std::vector<BenchmarkRun> runs;
  runs.reserve(matrix.size() * command.repeats);
  std::map<std::size_t, GeometrySignature> expectedGeometryByChunkSize;
  for (std::size_t repeat = 1; repeat <= command.repeats; ++repeat) {
    std::vector<Combination> executionOrder = matrix;
    if (repeat % 2 == 0) {
      std::reverse(executionOrder.begin(), executionOrder.end());
    }
    for (const auto [chunkLayers, workers] : executionOrder) {
      MeshBuildOptions runOptions = meshOptions;
      runOptions.chunkLayerCount = chunkLayers;
      auto run = runBenchmark(reader, range, runOptions, workers, repeat);
      printRun(run);
      if (!run.ok) {
        std::cerr << "error: benchmark failed for chunk_layers=" << chunkLayers
                  << " workers=" << workers << ": " << run.error << '\n';
        return 1;
      }
      const auto [expected, inserted] = expectedGeometryByChunkSize.emplace(
          chunkLayers, run.geometry);
      if (!inserted && run.geometry != expected->second) {
        std::cerr << "error: generated geometry differs between worker runs for chunk size "
                  << chunkLayers << '\n';
        return 1;
      }
      runs.push_back(std::move(run));
    }
  }

  const auto summaries = summarize(runs);
  printSummary(summaries);

  if (command.outputPrefix) {
    if (!writeReports(
            *command.outputPrefix,
            inputName,
            archiveOpenMs,
            range,
            command,
            runs,
            summaries,
            error)) {
      std::cerr << "error: " << error << '\n';
      return 1;
    }
    std::cout << "reports=" << command.outputPrefix->string() << ".csv,"
              << command.outputPrefix->string() << ".jsonl\n";
  }
  return 0;
}
