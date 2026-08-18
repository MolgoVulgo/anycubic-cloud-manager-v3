#include "render3d/compute/SupportComputeBackend.h"
#include "render3d/compute/VulkanSupportComputeBackend.h"

#include <cstddef>
#include <cstdint>
#include <barrier>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

void setPixel(
    std::vector<std::uint32_t>& words,
    std::uint32_t wordsPerRow,
    std::uint32_t x,
    std::uint32_t y) {
  words[static_cast<std::size_t>(y) * wordsPerRow + x / 32u]
      |= std::uint32_t{1} << (x % 32u);
}

bool pixel(
    const std::vector<std::uint32_t>& words,
    std::uint32_t wordsPerRow,
    std::uint32_t width,
    std::uint32_t height,
    std::int64_t x,
    std::int64_t y) {
  if (x < 0 || y < 0
      || x >= static_cast<std::int64_t>(width)
      || y >= static_cast<std::int64_t>(height)) {
    return false;
  }
  const auto ux = static_cast<std::uint32_t>(x);
  const auto uy = static_cast<std::uint32_t>(y);
  return (words[static_cast<std::size_t>(uy) * wordsPerRow + ux / 32u]
          & (std::uint32_t{1} << (ux % 32u))) != 0u;
}

std::vector<std::uint32_t> referenceOverlaps(
    const std::vector<std::uint32_t>& source,
    const std::vector<std::uint32_t>& reference,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t radius) {
  const auto wordsPerRow = (width + 31u) / 32u;
  const auto diameter = radius * 2u + 1u;
  std::vector<std::uint32_t> result(
      static_cast<std::size_t>(diameter) * diameter, 0u);
  std::size_t candidate = 0u;
  for (std::int64_t shiftY = -static_cast<std::int64_t>(radius);
       shiftY <= static_cast<std::int64_t>(radius); ++shiftY) {
    for (std::int64_t shiftX = -static_cast<std::int64_t>(radius);
         shiftX <= static_cast<std::int64_t>(radius); ++shiftX, ++candidate) {
      std::uint32_t overlap = 0u;
      for (std::uint32_t y = 0u; y < height; ++y) {
        for (std::uint32_t x = 0u; x < width; ++x) {
          if (pixel(source, wordsPerRow, width, height, x, y)
              && pixel(reference, wordsPerRow, width, height,
                       static_cast<std::int64_t>(x) - shiftX,
                       static_cast<std::int64_t>(y) - shiftY)) {
            ++overlap;
          }
        }
      }
      result[candidate] = overlap;
    }
  }
  return result;
}

std::vector<accloud::render3d::compute::SupportComputeRun> compactRuns(
    const std::vector<std::uint32_t>& source,
    std::uint32_t width,
    std::uint32_t height) {
  const auto wordsPerRow = (width + 31u) / 32u;
  std::vector<accloud::render3d::compute::SupportComputeRun> runs;
  for (std::uint32_t y = 0u; y < height; ++y) {
    std::uint32_t x = 0u;
    while (x < width) {
      while (x < width && !pixel(source, wordsPerRow, width, height, x, y)) {
        ++x;
      }
      if (x >= width) {
        break;
      }
      const auto first = x;
      while (x < width && pixel(source, wordsPerRow, width, height, x, y)) {
        ++x;
      }
      runs.push_back(accloud::render3d::compute::SupportComputeRun{
          y, first, x, 0u});
    }
  }
  return runs;
}

} // namespace

int main() {
  using namespace accloud::render3d::compute;
  std::string diagnostic;
  auto backend = createVulkanSupportComputeBackend(diagnostic);
  if (!backend) {
    std::cerr << "SKIP: " << diagnostic << '\n';
    return 77;
  }

  constexpr std::uint32_t width = 67u;
  constexpr std::uint32_t height = 9u;
  constexpr std::uint32_t radius = 4u;
  constexpr std::uint32_t wordsPerRow = (width + 31u) / 32u;
  std::vector<std::uint32_t> source(
      static_cast<std::size_t>(wordsPerRow) * height, 0u);
  std::vector<std::uint32_t> reference(source.size(), 0u);

  for (std::uint32_t y = 1u; y < 8u; ++y) {
    for (std::uint32_t x = 3u; x < 64u; x += 5u) {
      if (((x + y) % 3u) != 0u) {
        setPixel(source, wordsPerRow, x, y);
      }
      if (((x + 2u * y) % 4u) != 0u && x + 1u < width) {
        setPixel(reference, wordsPerRow, x + 1u, y);
      }
    }
  }
  // Exercise 32-bit word-boundary shifts explicitly.
  setPixel(source, wordsPerRow, 31u, 4u);
  setPixel(source, wordsPerRow, 32u, 4u);
  setPixel(reference, wordsPerRow, 27u, 4u);
  setPixel(reference, wordsPerRow, 36u, 4u);

  TranslatedOverlapBatch batch;
  batch.wordsPerRow = wordsPerRow;
  batch.height = height;
  batch.radius = radius;
  batch.sourceWords = source;
  batch.referenceWords = reference;
  const auto expected = referenceOverlaps(source, reference, width, height, radius);
  std::vector<std::uint32_t> actual(expected.size(), 0u);
  std::string error;
  if (!backend->translatedOverlaps(batch, actual, error)) {
    std::cerr << "Vulkan dispatch failed: " << error << '\n';
    return 1;
  }
  if (actual != expected) {
    std::cerr << "Vulkan translated-overlap result differs from CPU reference\n";
    for (std::size_t index = 0u; index < actual.size(); ++index) {
      if (actual[index] != expected[index]) {
        std::cerr << "candidate " << index << ": expected " << expected[index]
                  << ", got " << actual[index] << '\n';
      }
    }
    return 1;
  }
  if (backend->successfulDispatchCount() != 1u
      || backend->failedDispatchCount() != 0u) {
    std::cerr << "unexpected Vulkan dispatch counters\n";
    return 1;
  }

  // P6.2 keeps the full reference resident and submits the source as compact
  // runs. Run the same reference twice so the second request must reuse the
  // device-local reference instead of uploading it again.
  const auto runs = compactRuns(source, width, height);
  TranslatedRunOverlapBatch runBatch;
  runBatch.referenceKey = 0x1234u;
  runBatch.width = width;
  runBatch.height = height;
  runBatch.wordsPerRow = wordsPerRow;
  runBatch.radius = radius;
  runBatch.sourceRuns = runs;
  runBatch.referenceWords = reference;
  std::vector<std::uint32_t> runActual(expected.size(), 0u);
  if (!backend->translatedRunOverlaps(runBatch, runActual, error)) {
    std::cerr << "Vulkan run dispatch failed: " << error << '\n';
    return 1;
  }
  if (runActual != expected) {
    std::cerr << "Vulkan run-overlap result differs from CPU reference\n";
    return 1;
  }
  std::fill(runActual.begin(), runActual.end(), 0u);
  if (!backend->translatedRunOverlaps(runBatch, runActual, error)
      || runActual != expected) {
    std::cerr << "Vulkan resident-reference reuse differs from CPU reference: "
              << error << '\n';
    return 1;
  }
  const auto runTelemetry = backend->telemetry();
  if (runTelemetry.runSourceJobs != 2u
      || runTelemetry.residentReferenceUploads != 1u
      || runTelemetry.residentReferenceReuses < 1u
      || runTelemetry.submittedWorkgroups == 0u) {
    std::cerr << "unexpected P6.2 run/resident telemetry: run_jobs="
              << runTelemetry.runSourceJobs
              << " ref_uploads=" << runTelemetry.residentReferenceUploads
              << " ref_reuses=" << runTelemetry.residentReferenceReuses
              << " workgroups=" << runTelemetry.submittedWorkgroups << '\n';
    return 1;
  }

  // P6.4 submits exact zero-shift semantic-mask overlaps for a whole layer in
  // one API call. Use more than the backend's 64-job coalescing limit so the
  // test also covers deterministic splitting into several Vulkan submissions.
  constexpr std::size_t semanticBatchJobs = 96u;
  std::vector<SupportComputeRun> semanticRuns;
  std::vector<SupportComputeRunRange> semanticQueries;
  semanticRuns.reserve(runs.size() * semanticBatchJobs);
  semanticQueries.reserve(semanticBatchJobs);
  for (std::size_t job = 0u; job < semanticBatchJobs; ++job) {
    const auto firstRun = semanticRuns.size();
    semanticRuns.insert(semanticRuns.end(), runs.begin(), runs.end());
    semanticQueries.push_back(SupportComputeRunRange{
        static_cast<std::uint32_t>(firstRun),
        static_cast<std::uint32_t>(runs.size())});
  }
  RunMaskOverlapBatch semanticBatch;
  semanticBatch.referenceKey = 0x5678u;
  semanticBatch.width = width;
  semanticBatch.height = height;
  semanticBatch.wordsPerRow = wordsPerRow;
  semanticBatch.sourceRuns = semanticRuns;
  semanticBatch.queries = semanticQueries;
  semanticBatch.referenceWords = reference;
  const auto expectedZeroShift = referenceOverlaps(
      source, reference, width, height, 0u).front();
  std::vector<std::uint32_t> semanticActual(semanticBatchJobs, 0u);
  if (!backend->runMaskOverlaps(semanticBatch, semanticActual, error)) {
    std::cerr << "Vulkan semantic layer batch failed: " << error << '\n';
    return 1;
  }
  for (std::size_t job = 0u; job < semanticActual.size(); ++job) {
    if (semanticActual[job] != expectedZeroShift) {
      std::cerr << "semantic layer batch job " << job
                << " differs from CPU zero-shift reference\n";
      return 1;
    }
  }
  const auto semanticTelemetry = backend->telemetry();
  if (semanticTelemetry.semanticLayerBatchCalls != 1u
      || semanticTelemetry.semanticLayerBatchJobs != semanticBatchJobs
      || semanticTelemetry.runSourceJobs < 2u + semanticBatchJobs
      || semanticTelemetry.completedGpuJobs < 3u + semanticBatchJobs) {
    std::cerr << "unexpected P6.4 semantic layer-batch telemetry: calls="
              << semanticTelemetry.semanticLayerBatchCalls
              << " jobs=" << semanticTelemetry.semanticLayerBatchJobs
              << " run_jobs=" << semanticTelemetry.runSourceJobs
              << " gpu_jobs=" << semanticTelemetry.completedGpuJobs << '\n';
    return 1;
  }

  // P6.1/P6.2 must coalesce concurrent component jobs and execute more than one
  // job in a single Vulkan submission. The public API remains synchronous to
  // callers, but the backend dispatcher batches requests arriving from the
  // support-analysis worker pool.
  constexpr std::size_t concurrentJobs = 16u;
  std::barrier startLine(static_cast<std::ptrdiff_t>(concurrentJobs));
  std::vector<std::vector<std::uint32_t>> concurrentResults(
      concurrentJobs, std::vector<std::uint32_t>(expected.size(), 0u));
  std::vector<std::uint8_t> concurrentSuccess(concurrentJobs, 0u);
  std::vector<std::string> concurrentErrors(concurrentJobs);
  std::vector<std::jthread> workers;
  workers.reserve(concurrentJobs);
  for (std::size_t job = 0u; job < concurrentJobs; ++job) {
    workers.emplace_back([&, job] {
      startLine.arrive_and_wait();
      concurrentSuccess[job] = backend->translatedOverlaps(
          batch, concurrentResults[job], concurrentErrors[job]);
    });
  }
  workers.clear();
  for (std::size_t job = 0u; job < concurrentJobs; ++job) {
    if (!concurrentSuccess[job]) {
      std::cerr << "concurrent Vulkan job " << job
                << " failed: " << concurrentErrors[job] << '\n';
      return 1;
    }
    if (concurrentResults[job] != expected) {
      std::cerr << "concurrent Vulkan job " << job
                << " differs from CPU reference\n";
      return 1;
    }
  }
  const auto telemetry = backend->telemetry();
  const auto expectedJobs = concurrentJobs + 3u + semanticBatchJobs;
  if (telemetry.submittedJobs != expectedJobs
      || telemetry.completedGpuJobs != expectedJobs
      || telemetry.cpuFallbackJobs != 0u
      || telemetry.failedDispatches != 0u
      || telemetry.maximumBatchJobs < 2u
      || telemetry.successfulDispatches >= expectedJobs
      || telemetry.runSourceJobs < 2u + semanticBatchJobs
      || telemetry.residentReferenceUploads < 2u
      || telemetry.residentReferenceReuses < 1u
      || telemetry.submittedWorkgroups <= telemetry.completedGpuJobs
      || telemetry.semanticLayerBatchCalls != 1u
      || telemetry.semanticLayerBatchJobs != semanticBatchJobs) {
    std::cerr << "unexpected Vulkan coalescing telemetry: submitted="
              << telemetry.submittedJobs
              << " gpu_jobs=" << telemetry.completedGpuJobs
              << " fallbacks=" << telemetry.cpuFallbackJobs
              << " dispatches=" << telemetry.successfulDispatches
              << " max_batch=" << telemetry.maximumBatchJobs << '\n';
    return 1;
  }
  std::cout << "Vulkan tiled/run-resident support-compute matches CPU reference\n";
  return 0;
}
