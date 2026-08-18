#include "render3d/compute/SupportComputeBackend.h"

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

} // namespace

int main() {
  using namespace accloud::render3d::compute;
  std::string diagnostic;
  auto backend = createSupportComputeBackend(
      SupportComputePreference::Vulkan, diagnostic);
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

  // P6.1 must coalesce concurrent component jobs and execute more than one
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
  if (telemetry.submittedJobs != concurrentJobs + 1u
      || telemetry.completedGpuJobs != concurrentJobs + 1u
      || telemetry.cpuFallbackJobs != 0u
      || telemetry.failedDispatches != 0u
      || telemetry.maximumBatchJobs < 2u
      || telemetry.successfulDispatches >= concurrentJobs + 1u) {
    std::cerr << "unexpected Vulkan coalescing telemetry: submitted="
              << telemetry.submittedJobs
              << " gpu_jobs=" << telemetry.completedGpuJobs
              << " fallbacks=" << telemetry.cpuFallbackJobs
              << " dispatches=" << telemetry.successfulDispatches
              << " max_batch=" << telemetry.maximumBatchJobs << '\n';
    return 1;
  }
  std::cout << "Vulkan support-compute overlap batch matches CPU reference\n";
  return 0;
}
