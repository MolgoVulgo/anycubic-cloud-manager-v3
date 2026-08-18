#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace accloud::render3d::compute {

enum class SupportComputePreference : std::uint8_t {
  Auto,
  Cpu,
  Vulkan,
};

struct TranslatedOverlapBatch {
  std::uint32_t wordsPerRow = 0u;
  std::uint32_t height = 0u;
  std::uint32_t radius = 0u;
  std::span<const std::uint32_t> sourceWords;
  std::span<const std::uint32_t> referenceWords;
};

struct SupportComputeTelemetry {
  std::size_t eligibleJobs = 0u;
  std::size_t submittedJobs = 0u;
  std::size_t completedGpuJobs = 0u;
  std::size_t cpuFallbackJobs = 0u;
  std::size_t successfulDispatches = 0u;
  std::size_t failedDispatches = 0u;
  std::size_t maximumBatchJobs = 0u;
  std::uint64_t uploadBytes = 0u;
  std::uint64_t readbackBytes = 0u;
  std::uint64_t hostPreparationNanoseconds = 0u;
  std::uint64_t queueWaitNanoseconds = 0u;
  std::uint64_t batchExecutionNanoseconds = 0u;
};

class SupportComputeBackend {
public:
  virtual ~SupportComputeBackend() = default;

  [[nodiscard]] virtual const char* name() const noexcept = 0;
  [[nodiscard]] virtual const char* deviceName() const noexcept {
    return name();
  }

  // Computes one overlap count for every integer translation in the square
  // [-radius, +radius]^2, in row-major shift order (Y outer, X inner).
  // Implementations must be thread-safe. A false return is non-fatal for the
  // analyser: the caller falls back to the canonical CPU path.
  virtual bool translatedOverlaps(
      const TranslatedOverlapBatch& batch,
      std::span<std::uint32_t> overlaps,
      std::string& error) = 0;

  [[nodiscard]] std::size_t successfulDispatchCount() const noexcept {
    return successfulDispatchCount_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] std::size_t failedDispatchCount() const noexcept {
    return failedDispatchCount_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] SupportComputeTelemetry telemetry() const noexcept {
    return SupportComputeTelemetry{
        eligibleJobCount_.load(std::memory_order_relaxed),
        submittedJobCount_.load(std::memory_order_relaxed),
        completedGpuJobCount_.load(std::memory_order_relaxed),
        cpuFallbackJobCount_.load(std::memory_order_relaxed),
        successfulDispatchCount_.load(std::memory_order_relaxed),
        failedDispatchCount_.load(std::memory_order_relaxed),
        maximumBatchJobCount_.load(std::memory_order_relaxed),
        uploadBytes_.load(std::memory_order_relaxed),
        readbackBytes_.load(std::memory_order_relaxed),
        hostPreparationNanoseconds_.load(std::memory_order_relaxed),
        queueWaitNanoseconds_.load(std::memory_order_relaxed),
        batchExecutionNanoseconds_.load(std::memory_order_relaxed),
    };
  }

  void recordEligibleJob() noexcept {
    eligibleJobCount_.fetch_add(1u, std::memory_order_relaxed);
  }

  void recordHostPreparation(std::uint64_t nanoseconds) noexcept {
    hostPreparationNanoseconds_.fetch_add(nanoseconds, std::memory_order_relaxed);
  }

  void recordCpuFallbackJob() noexcept {
    cpuFallbackJobCount_.fetch_add(1u, std::memory_order_relaxed);
  }

protected:
  void recordSubmittedJobs(std::size_t count) noexcept {
    submittedJobCount_.fetch_add(count, std::memory_order_relaxed);
  }

  void recordSuccessfulDispatch(std::size_t batchJobs = 1u) noexcept {
    successfulDispatchCount_.fetch_add(1u, std::memory_order_relaxed);
    completedGpuJobCount_.fetch_add(batchJobs, std::memory_order_relaxed);
    auto observed = maximumBatchJobCount_.load(std::memory_order_relaxed);
    while (observed < batchJobs
           && !maximumBatchJobCount_.compare_exchange_weak(
               observed, batchJobs, std::memory_order_relaxed)) {
    }
  }

  void recordFailedDispatch(std::size_t batchJobs = 1u) noexcept {
    failedDispatchCount_.fetch_add(1u, std::memory_order_relaxed);
    cpuFallbackJobCount_.fetch_add(batchJobs, std::memory_order_relaxed);
  }

  void recordTransferBytes(std::uint64_t upload, std::uint64_t readback) noexcept {
    uploadBytes_.fetch_add(upload, std::memory_order_relaxed);
    readbackBytes_.fetch_add(readback, std::memory_order_relaxed);
  }

  void recordQueueWait(std::uint64_t nanoseconds) noexcept {
    queueWaitNanoseconds_.fetch_add(nanoseconds, std::memory_order_relaxed);
  }

  void recordBatchExecution(std::uint64_t nanoseconds) noexcept {
    batchExecutionNanoseconds_.fetch_add(nanoseconds, std::memory_order_relaxed);
  }

private:
  std::atomic<std::size_t> eligibleJobCount_{0u};
  std::atomic<std::size_t> submittedJobCount_{0u};
  std::atomic<std::size_t> completedGpuJobCount_{0u};
  std::atomic<std::size_t> cpuFallbackJobCount_{0u};
  std::atomic<std::size_t> successfulDispatchCount_{0u};
  std::atomic<std::size_t> failedDispatchCount_{0u};
  std::atomic<std::size_t> maximumBatchJobCount_{0u};
  std::atomic<std::uint64_t> uploadBytes_{0u};
  std::atomic<std::uint64_t> readbackBytes_{0u};
  std::atomic<std::uint64_t> hostPreparationNanoseconds_{0u};
  std::atomic<std::uint64_t> queueWaitNanoseconds_{0u};
  std::atomic<std::uint64_t> batchExecutionNanoseconds_{0u};
};

[[nodiscard]] bool vulkanSupportComputeCompiled() noexcept;

// Returns a Vulkan backend when it is compiled in and a usable compute device
// can be initialised. CPU preference always returns nullptr. Auto callers treat
// a nullptr as the normal CPU fallback; Vulkan callers may surface diagnostic
// as an explicit availability error.
[[nodiscard]] std::unique_ptr<SupportComputeBackend> createSupportComputeBackend(
    SupportComputePreference preference,
    std::string& diagnostic);

} // namespace accloud::render3d::compute
