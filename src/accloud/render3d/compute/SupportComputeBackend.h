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
};

struct TranslatedOverlapBatch {
  std::uint32_t wordsPerRow = 0u;
  std::uint32_t height = 0u;
  std::uint32_t radius = 0u;
  std::span<const std::uint32_t> sourceWords;
  std::span<const std::uint32_t> referenceWords;
};

// Compact source representation used by P6.2. The semantic analyser already
// owns canonical non-overlapping horizontal runs, so uploading those runs is
// substantially cheaper than rebuilding a dense source bitmap for every large
// component. Coordinates are in the native full-layer raster domain.
struct SupportComputeRun {
  std::uint32_t y = 0u;
  std::uint32_t firstX = 0u;
  std::uint32_t lastX = 0u;
  std::uint32_t reserved = 0u;
};
static_assert(sizeof(SupportComputeRun) == 16u);

struct TranslatedRunOverlapBatch {
  // Changes whenever the reference semantic mask changes. Vulkan backends may
  // keep this full-layer reference resident across several component jobs.
  std::uint64_t referenceKey = 0u;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t wordsPerRow = 0u;
  std::uint32_t radius = 0u;
  std::span<const SupportComputeRun> sourceRuns;
  std::span<const std::uint32_t> referenceWords;
};

// P6.4 introduced zero-shift semantic-mask queries at layer granularity.
// The runtime analyzer no longer routes through this experimental bulk path:
// benchmarks showed that materialising one synchronized host request per
// component regressed both performance and hybrid semantic stability. The
// contract remains only as a low-level Vulkan backend self-test until a true
// single-request layer implementation exists. Runs from all current components
// are flattened once; each range selects one component.
struct SupportComputeRunRange {
  std::uint32_t firstRun = 0u;
  std::uint32_t runCount = 0u;
};

struct RunMaskOverlapBatch {
  std::uint64_t referenceKey = 0u;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t wordsPerRow = 0u;
  std::span<const SupportComputeRun> sourceRuns;
  std::span<const SupportComputeRunRange> queries;
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
  std::size_t runSourceJobs = 0u;
  std::size_t residentReferenceUploads = 0u;
  std::size_t residentReferenceReuses = 0u;
  std::uint64_t submittedWorkgroups = 0u;
  std::size_t semanticLayerBatchCalls = 0u;
  std::size_t semanticLayerBatchJobs = 0u;
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

  // P6.2 path for large components: source matter remains compact RLE while a
  // full-layer reference bitmap may stay resident on the GPU. Implementations
  // that do not support it return false and the analyser uses the canonical CPU
  // result, so this remains an optional acceleration contract.
  virtual bool translatedRunOverlaps(
      const TranslatedRunOverlapBatch&,
      std::span<std::uint32_t>,
      std::string& error) {
    error = "run-based translated overlap is unsupported by this compute backend";
    return false;
  }

  // Exact zero-shift overlap for many component run ranges sharing one
  // full-layer reference. This is intentionally a layer-level contract: Vulkan
  // can enqueue all component jobs together and coalesce them into a handful of
  // submissions instead of blocking worker threads component by component.
  virtual bool runMaskOverlaps(
      const RunMaskOverlapBatch&,
      std::span<std::uint32_t>,
      std::string& error) {
    error = "layer run-mask overlap batching is unsupported by this compute backend";
    return false;
  }

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
        runSourceJobCount_.load(std::memory_order_relaxed),
        residentReferenceUploadCount_.load(std::memory_order_relaxed),
        residentReferenceReuseCount_.load(std::memory_order_relaxed),
        submittedWorkgroupCount_.load(std::memory_order_relaxed),
        semanticLayerBatchCallCount_.load(std::memory_order_relaxed),
        semanticLayerBatchJobCount_.load(std::memory_order_relaxed),
    };
  }

  void recordEligibleJob() noexcept {
    eligibleJobCount_.fetch_add(1u, std::memory_order_relaxed);
  }

  void recordEligibleJobs(std::size_t count) noexcept {
    eligibleJobCount_.fetch_add(count, std::memory_order_relaxed);
  }

  void recordHostPreparation(std::uint64_t nanoseconds) noexcept {
    hostPreparationNanoseconds_.fetch_add(nanoseconds, std::memory_order_relaxed);
  }

  void recordCpuFallbackJob() noexcept {
    cpuFallbackJobCount_.fetch_add(1u, std::memory_order_relaxed);
  }

protected:
  void recordRunSourceJobs(std::size_t count) noexcept {
    runSourceJobCount_.fetch_add(count, std::memory_order_relaxed);
  }

  void recordResidentReferenceUpload() noexcept {
    residentReferenceUploadCount_.fetch_add(1u, std::memory_order_relaxed);
  }

  void recordResidentReferenceReuse() noexcept {
    residentReferenceReuseCount_.fetch_add(1u, std::memory_order_relaxed);
  }

  void recordSubmittedWorkgroups(std::uint64_t count) noexcept {
    submittedWorkgroupCount_.fetch_add(count, std::memory_order_relaxed);
  }

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

  void recordSemanticLayerBatch(std::size_t jobs) noexcept {
    semanticLayerBatchCallCount_.fetch_add(1u, std::memory_order_relaxed);
    semanticLayerBatchJobCount_.fetch_add(jobs, std::memory_order_relaxed);
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
  std::atomic<std::size_t> runSourceJobCount_{0u};
  std::atomic<std::size_t> residentReferenceUploadCount_{0u};
  std::atomic<std::size_t> residentReferenceReuseCount_{0u};
  std::atomic<std::uint64_t> submittedWorkgroupCount_{0u};
  std::atomic<std::size_t> semanticLayerBatchCallCount_{0u};
  std::atomic<std::size_t> semanticLayerBatchJobCount_{0u};
};

[[nodiscard]] bool vulkanSupportComputeCompiled() noexcept;

// Returns a Vulkan backend for the normal Auto/hybrid mode when it is compiled
// in and a usable compute device can be initialised. CPU preference always
// returns nullptr. Auto callers treat nullptr as the canonical CPU fallback.
[[nodiscard]] std::unique_ptr<SupportComputeBackend> createSupportComputeBackend(
    SupportComputePreference preference,
    std::string& diagnostic);

} // namespace accloud::render3d::compute
