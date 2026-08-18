#include "render3d/compute/VulkanSupportComputeBackend.h"

#include "SupportOverlapSpirv.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace accloud::render3d::compute {
namespace {

constexpr std::uint32_t kDenseWordsPerTile = 4096u;
constexpr std::uint32_t kRunsPerTile = 64u;
constexpr std::size_t kMaximumBatchJobs = 64u;
constexpr auto kCoalesceWindow = std::chrono::microseconds(250);

enum class GpuInputMode : std::uint32_t {
  DenseWords = 0u,
  CompactRuns = 1u,
};

struct GpuJobDescriptor {
  std::uint32_t inputOffset = 0u;
  std::uint32_t referenceWordOffset = 0u;
  std::uint32_t wordsPerRow = 0u;
  std::uint32_t height = 0u;
  std::uint32_t radius = 0u;
  std::uint32_t candidateCount = 0u;
  std::uint32_t outputOffset = 0u;
  std::uint32_t elementCount = 0u;
  std::uint32_t width = 0u;
  std::uint32_t mode = static_cast<std::uint32_t>(GpuInputMode::DenseWords);
};
static_assert(sizeof(GpuJobDescriptor) == 40u);
static_assert(sizeof(SupportComputeRun) == 16u);

std::string vkFailure(const char* operation, VkResult result) {
  std::ostringstream stream;
  stream << operation << " failed with VkResult " << static_cast<int>(result);
  return stream.str();
}

bool checkedAdd(std::uint64_t left, std::uint64_t right, std::uint64_t& result) {
  if (right > std::numeric_limits<std::uint64_t>::max() - left) {
    return false;
  }
  result = left + right;
  return true;
}

class VulkanSupportComputeBackend final : public SupportComputeBackend {
public:
  explicit VulkanSupportComputeBackend(std::string& error) {
    initialise(error);
    if (!ready_) {
      return;
    }
    try {
      dispatcher_ = std::jthread([this](std::stop_token stopToken) {
        dispatchLoop(stopToken);
      });
    } catch (const std::exception& exception) {
      ready_ = false;
      error = std::string("unable to start Vulkan support-compute dispatcher: ")
          + exception.what();
    }
  }

  ~VulkanSupportComputeBackend() override {
    ready_.store(false, std::memory_order_release);
    if (dispatcher_.joinable()) {
      dispatcher_.request_stop();
      queueCv_.notify_all();
      dispatcher_.join();
    }
    failPending("Vulkan support-compute backend is shutting down");

    if (device_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device_);
    }
    destroyBuffer(uploadBuffer_);
    destroyBuffer(readbackBuffer_);
    destroyBuffer(sourceBuffer_);
    destroyBuffer(referenceBuffer_);
    destroyBuffer(residentReferenceBuffer_);
    destroyBuffer(runBuffer_);
    destroyBuffer(jobBuffer_);
    destroyBuffer(outputBuffer_);
    if (device_ != VK_NULL_HANDLE && fence_ != VK_NULL_HANDLE) {
      vkDestroyFence(device_, fence_, nullptr);
    }
    if (device_ != VK_NULL_HANDLE && commandPool_ != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_, commandPool_, nullptr);
    }
    if (device_ != VK_NULL_HANDLE && descriptorPool_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    }
    if (device_ != VK_NULL_HANDLE && pipeline_ != VK_NULL_HANDLE) {
      vkDestroyPipeline(device_, pipeline_, nullptr);
    }
    if (device_ != VK_NULL_HANDLE && pipelineLayout_ != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    }
    if (device_ != VK_NULL_HANDLE && descriptorSetLayout_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
    }
    if (device_ != VK_NULL_HANDLE) {
      vkDestroyDevice(device_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
      vkDestroyInstance(instance_, nullptr);
    }
  }

  [[nodiscard]] bool ready() const noexcept {
    return ready_.load(std::memory_order_acquire);
  }

  [[nodiscard]] const char* name() const noexcept override {
    return "vulkan";
  }

  [[nodiscard]] const char* deviceName() const noexcept override {
    return physicalDeviceProperties_.deviceName[0] == '\0'
        ? "vulkan"
        : physicalDeviceProperties_.deviceName;
  }

  bool translatedOverlaps(
      const TranslatedOverlapBatch& batch,
      std::span<std::uint32_t> overlaps,
      std::string& error) override {
    error.clear();
    if (!ready()) {
      error = "Vulkan compute backend is not initialised";
      recordFailedDispatch();
      return false;
    }
    if (!validateBatch(batch, overlaps, error)) {
      recordFailedDispatch();
      return false;
    }

    auto request = std::make_shared<PendingRequest>();
    request->kind = RequestKind::Dense;
    request->batch = batch;
    request->overlaps = overlaps;
    request->queuedAt = Clock::now();
    recordSubmittedJobs(1u);
    {
      std::lock_guard queueLock(queueMutex_);
      if (!ready()) {
        error = "Vulkan compute backend stopped before submission";
        recordFailedDispatch();
        return false;
      }
      pending_.push_back(request);
    }
    queueCv_.notify_one();

    std::unique_lock requestLock(request->mutex);
    request->cv.wait(requestLock, [&] { return request->done; });
    error = request->error;
    return request->success;
  }

  bool translatedRunOverlaps(
      const TranslatedRunOverlapBatch& batch,
      std::span<std::uint32_t> overlaps,
      std::string& error) override {
    error.clear();
    if (!ready()) {
      error = "Vulkan compute backend is not initialised";
      recordFailedDispatch();
      return false;
    }
    if (!validateRunBatch(batch, overlaps, error)) {
      recordFailedDispatch();
      return false;
    }

    auto request = std::make_shared<PendingRequest>();
    request->kind = RequestKind::Runs;
    request->runBatch = batch;
    request->overlaps = overlaps;
    request->queuedAt = Clock::now();
    recordSubmittedJobs(1u);
    {
      std::lock_guard queueLock(queueMutex_);
      if (!ready()) {
        error = "Vulkan compute backend stopped before run submission";
        recordFailedDispatch();
        return false;
      }
      pending_.push_back(request);
    }
    queueCv_.notify_one();

    std::unique_lock requestLock(request->mutex);
    request->cv.wait(requestLock, [&] { return request->done; });
    error = request->error;
    return request->success;
  }

  bool runMaskOverlaps(
      const RunMaskOverlapBatch& batch,
      std::span<std::uint32_t> overlaps,
      std::string& error) override {
    error.clear();
    if (batch.queries.empty()) {
      return true;
    }
    if (!ready()) {
      error = "Vulkan compute backend is not initialised";
      recordFailedDispatch(batch.queries.size());
      return false;
    }
    if (batch.referenceKey == 0u
        || batch.width == 0u
        || batch.height == 0u
        || batch.wordsPerRow != (batch.width + 31u) / 32u
        || overlaps.size() != batch.queries.size()) {
      error = "Vulkan layer run-mask batch dimensions are invalid";
      recordFailedDispatch(batch.queries.size());
      return false;
    }
    const auto referenceWordCount = static_cast<std::uint64_t>(batch.wordsPerRow)
        * static_cast<std::uint64_t>(batch.height);
    if (referenceWordCount == 0u
        || referenceWordCount > std::numeric_limits<std::size_t>::max()
        || batch.referenceWords.size() != static_cast<std::size_t>(referenceWordCount)
        || batch.sourceRuns.size() > std::numeric_limits<std::uint32_t>::max()) {
      error = "Vulkan layer run-mask reference/source payload is invalid";
      recordFailedDispatch(batch.queries.size());
      return false;
    }

    std::vector<std::shared_ptr<PendingRequest>> requests;
    requests.reserve(batch.queries.size());
    const auto queuedAt = Clock::now();
    for (std::size_t index = 0u; index < batch.queries.size(); ++index) {
      const auto& query = batch.queries[index];
      const auto firstRun = static_cast<std::size_t>(query.firstRun);
      const auto runCount = static_cast<std::size_t>(query.runCount);
      if (runCount == 0u
          || firstRun > batch.sourceRuns.size()
          || runCount > batch.sourceRuns.size() - firstRun) {
        error = "Vulkan layer run-mask query range is invalid";
        recordFailedDispatch(batch.queries.size());
        return false;
      }
      auto request = std::make_shared<PendingRequest>();
      request->kind = RequestKind::Runs;
      request->runBatch.referenceKey = batch.referenceKey;
      request->runBatch.width = batch.width;
      request->runBatch.height = batch.height;
      request->runBatch.wordsPerRow = batch.wordsPerRow;
      request->runBatch.radius = 0u;
      request->runBatch.sourceRuns = batch.sourceRuns.subspan(firstRun, runCount);
      request->runBatch.referenceWords = batch.referenceWords;
      request->overlaps = overlaps.subspan(index, 1u);
      request->queuedAt = queuedAt;
      requests.push_back(std::move(request));
    }

    recordSemanticLayerBatch(requests.size());
    recordSubmittedJobs(requests.size());
    {
      std::lock_guard queueLock(queueMutex_);
      if (!ready()) {
        error = "Vulkan compute backend stopped before layer batch submission";
        recordFailedDispatch(requests.size());
        return false;
      }
      for (const auto& request : requests) {
        pending_.push_back(request);
      }
    }
    queueCv_.notify_all();

    bool success = true;
    std::string firstError;
    for (const auto& request : requests) {
      std::unique_lock requestLock(request->mutex);
      request->cv.wait(requestLock, [&] { return request->done; });
      if (!request->success && success) {
        success = false;
        firstError = request->error;
      }
    }
    error = std::move(firstError);
    return success;
  }

private:
  using Clock = std::chrono::steady_clock;

  struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize capacity = 0u;
    VkBufferUsageFlags usage = 0u;
    VkMemoryPropertyFlags properties = 0u;
  };

  enum class RequestKind : std::uint8_t {
    Dense,
    Runs,
  };

  struct PendingRequest {
    RequestKind kind = RequestKind::Dense;
    TranslatedOverlapBatch batch;
    TranslatedRunOverlapBatch runBatch;
    std::span<std::uint32_t> overlaps;
    Clock::time_point queuedAt{};
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    bool success = false;
    std::string error;
  };

  static std::uint64_t nanoseconds(Clock::duration duration) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
  }

  bool validateBatch(
      const TranslatedOverlapBatch& batch,
      std::span<std::uint32_t> overlaps,
      std::string& error) const {
    if (batch.wordsPerRow == 0u || batch.height == 0u || batch.radius > 31u) {
      error = "Vulkan translated-overlap batch dimensions are invalid";
      return false;
    }
    const auto wordCount = static_cast<std::uint64_t>(batch.wordsPerRow)
        * static_cast<std::uint64_t>(batch.height);
    if (wordCount == 0u
        || wordCount > std::numeric_limits<std::uint32_t>::max()
        || wordCount > std::numeric_limits<std::size_t>::max()
        || batch.sourceWords.size() != static_cast<std::size_t>(wordCount)
        || batch.referenceWords.size() != static_cast<std::size_t>(wordCount)) {
      error = "Vulkan translated-overlap buffers do not match their declared dimensions";
      return false;
    }
    const auto diameter = static_cast<std::uint64_t>(batch.radius) * 2u + 1u;
    const auto candidateCount = diameter * diameter;
    if (candidateCount == 0u
        || candidateCount > std::numeric_limits<std::uint32_t>::max()
        || overlaps.size() != static_cast<std::size_t>(candidateCount)) {
      error = "Vulkan translated-overlap output size is invalid";
      return false;
    }
    return true;
  }

  bool validateRunBatch(
      const TranslatedRunOverlapBatch& batch,
      std::span<std::uint32_t> overlaps,
      std::string& error) const {
    if (batch.referenceKey == 0u
        || batch.width == 0u
        || batch.height == 0u
        || batch.wordsPerRow != (batch.width + 31u) / 32u
        || batch.radius > 31u
        || batch.sourceRuns.empty()) {
      error = "Vulkan run-overlap batch dimensions are invalid";
      return false;
    }
    const auto referenceWordCount = static_cast<std::uint64_t>(batch.wordsPerRow)
        * static_cast<std::uint64_t>(batch.height);
    if (referenceWordCount == 0u
        || referenceWordCount > std::numeric_limits<std::size_t>::max()
        || batch.referenceWords.size() != static_cast<std::size_t>(referenceWordCount)
        || batch.sourceRuns.size() > std::numeric_limits<std::uint32_t>::max()) {
      error = "Vulkan run-overlap reference/source payload is invalid";
      return false;
    }
    const auto diameter = static_cast<std::uint64_t>(batch.radius) * 2u + 1u;
    const auto candidateCount = diameter * diameter;
    if (candidateCount == 0u
        || candidateCount > std::numeric_limits<std::uint32_t>::max()
        || overlaps.size() != static_cast<std::size_t>(candidateCount)) {
      error = "Vulkan run-overlap output size is invalid";
      return false;
    }
    return true;
  }

  void dispatchLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
      std::vector<std::shared_ptr<PendingRequest>> requests;
      RequestKind selectedKind = RequestKind::Dense;
      std::uint64_t selectedReferenceKey = 0u;
      {
        std::unique_lock queueLock(queueMutex_);
        queueCv_.wait(queueLock, [&] {
          return stopToken.stop_requested() || !pending_.empty();
        });
        if (stopToken.stop_requested()) {
          break;
        }

        selectedKind = pending_.front()->kind;
        if (selectedKind == RequestKind::Runs) {
          selectedReferenceKey = pending_.front()->runBatch.referenceKey;
        }
        const auto deadline = Clock::now() + kCoalesceWindow;
        queueCv_.wait_until(queueLock, deadline, [&] {
          if (stopToken.stop_requested()) {
            return true;
          }
          std::size_t matching = 0u;
          for (const auto& request : pending_) {
            const bool sameKind = request->kind == selectedKind;
            const bool sameReference = selectedKind != RequestKind::Runs
                || request->runBatch.referenceKey == selectedReferenceKey;
            if (sameKind && sameReference && ++matching >= kMaximumBatchJobs) {
              return true;
            }
          }
          return false;
        });
        if (stopToken.stop_requested()) {
          break;
        }

        requests.reserve(std::min(pending_.size(), kMaximumBatchJobs));
        for (auto iterator = pending_.begin();
             iterator != pending_.end() && requests.size() < kMaximumBatchJobs;) {
          const bool sameKind = (*iterator)->kind == selectedKind;
          const bool sameReference = selectedKind != RequestKind::Runs
              || (*iterator)->runBatch.referenceKey == selectedReferenceKey;
          if (sameKind && sameReference) {
            requests.push_back(std::move(*iterator));
            iterator = pending_.erase(iterator);
          } else {
            ++iterator;
          }
        }
      }
      if (selectedKind == RequestKind::Runs) {
        dispatchRunBatch(requests);
      } else {
        dispatchDenseBatch(requests);
      }
    }
  }

  void complete(
      const std::shared_ptr<PendingRequest>& request,
      bool success,
      const std::string& error) {
    {
      std::lock_guard requestLock(request->mutex);
      request->success = success;
      request->error = error;
      request->done = true;
    }
    request->cv.notify_one();
  }

  void failPending(const std::string& error) {
    std::deque<std::shared_ptr<PendingRequest>> remaining;
    {
      std::lock_guard queueLock(queueMutex_);
      remaining.swap(pending_);
    }
    for (const auto& request : remaining) {
      recordFailedDispatch();
      complete(request, false, error);
    }
  }

  void failBatch(
      const std::vector<std::shared_ptr<PendingRequest>>& requests,
      const std::string& error) {
    recordFailedDispatch(requests.size());
    for (const auto& request : requests) {
      complete(request, false, error);
    }
  }

  void dispatchDenseBatch(const std::vector<std::shared_ptr<PendingRequest>>& requests) {
    if (requests.empty()) {
      return;
    }

    std::uint64_t totalSourceWords = 0u;
    std::uint64_t totalReferenceWords = 0u;
    std::uint64_t totalOutputWords = 0u;
    std::uint32_t maximumCandidateCount = 0u;
    std::uint32_t maximumTileCount = 0u;
    std::vector<GpuJobDescriptor> jobs;
    jobs.reserve(requests.size());

    for (const auto& request : requests) {
      const auto wordCount = static_cast<std::uint64_t>(request->batch.wordsPerRow)
          * static_cast<std::uint64_t>(request->batch.height);
      const auto diameter = static_cast<std::uint64_t>(request->batch.radius) * 2u + 1u;
      const auto candidateCount = diameter * diameter;
      std::uint64_t nextSourceWords = 0u;
      std::uint64_t nextReferenceWords = 0u;
      std::uint64_t nextOutputWords = 0u;
      if (!checkedAdd(totalSourceWords, wordCount, nextSourceWords)
          || !checkedAdd(totalReferenceWords, wordCount, nextReferenceWords)
          || !checkedAdd(totalOutputWords, candidateCount, nextOutputWords)
          || totalSourceWords > std::numeric_limits<std::uint32_t>::max()
          || totalReferenceWords > std::numeric_limits<std::uint32_t>::max()
          || totalOutputWords > std::numeric_limits<std::uint32_t>::max()
          || wordCount > std::numeric_limits<std::uint32_t>::max()) {
        failBatch(requests, "Vulkan support-compute coalesced dense batch is too large");
        return;
      }
      const auto tileCount = static_cast<std::uint32_t>(
          (wordCount + kDenseWordsPerTile - 1u) / kDenseWordsPerTile);
      jobs.push_back(GpuJobDescriptor{
          static_cast<std::uint32_t>(totalSourceWords),
          static_cast<std::uint32_t>(totalReferenceWords),
          request->batch.wordsPerRow,
          request->batch.height,
          request->batch.radius,
          static_cast<std::uint32_t>(candidateCount),
          static_cast<std::uint32_t>(totalOutputWords),
          static_cast<std::uint32_t>(wordCount),
          request->batch.wordsPerRow * 32u,
          static_cast<std::uint32_t>(GpuInputMode::DenseWords),
      });
      totalSourceWords = nextSourceWords;
      totalReferenceWords = nextReferenceWords;
      totalOutputWords = nextOutputWords;
      maximumCandidateCount = std::max(
          maximumCandidateCount, static_cast<std::uint32_t>(candidateCount));
      maximumTileCount = std::max(maximumTileCount, tileCount);
    }

    if (totalSourceWords > std::numeric_limits<std::uint32_t>::max()
        || totalReferenceWords > std::numeric_limits<std::uint32_t>::max()
        || totalOutputWords > std::numeric_limits<std::uint32_t>::max()) {
      failBatch(requests, "Vulkan support-compute dense offsets overflow 32-bit storage");
      return;
    }

    const VkDeviceSize sourceBytes = static_cast<VkDeviceSize>(totalSourceWords)
        * sizeof(std::uint32_t);
    const VkDeviceSize referenceBytes = static_cast<VkDeviceSize>(totalReferenceWords)
        * sizeof(std::uint32_t);
    const VkDeviceSize jobBytes = static_cast<VkDeviceSize>(jobs.size())
        * sizeof(GpuJobDescriptor);
    const VkDeviceSize outputBytes = static_cast<VkDeviceSize>(totalOutputWords)
        * sizeof(std::uint32_t);
    const auto maximumStorageRange = static_cast<VkDeviceSize>(
        physicalDeviceProperties_.limits.maxStorageBufferRange);
    if (requests.size() > 1u
        && (sourceBytes > maximumStorageRange
            || referenceBytes > maximumStorageRange
            || jobBytes > maximumStorageRange
            || outputBytes > maximumStorageRange)) {
      const auto middle = requests.begin()
          + static_cast<std::ptrdiff_t>(requests.size() / 2u);
      dispatchDenseBatch(std::vector<std::shared_ptr<PendingRequest>>(
          requests.begin(), middle));
      dispatchDenseBatch(std::vector<std::shared_ptr<PendingRequest>>(
          middle, requests.end()));
      return;
    }
    if (sourceBytes > maximumStorageRange
        || referenceBytes > maximumStorageRange
        || jobBytes > maximumStorageRange
        || outputBytes > maximumStorageRange) {
      failBatch(requests, "Vulkan support-compute dense job exceeds maxStorageBufferRange");
      return;
    }
    if (maximumTileCount == 0u
        || maximumTileCount > physicalDeviceProperties_.limits.maxComputeWorkGroupCount[0]
        || maximumCandidateCount > physicalDeviceProperties_.limits.maxComputeWorkGroupCount[1]
        || jobs.size() > physicalDeviceProperties_.limits.maxComputeWorkGroupCount[2]) {
      failBatch(requests, "Vulkan support-compute dense dispatch exceeds workgroup-count limits");
      return;
    }

    const auto batchStart = Clock::now();
    for (const auto& request : requests) {
      recordQueueWait(nanoseconds(batchStart - request->queuedAt));
    }
    const VkDeviceSize referenceUploadOffset = sourceBytes;
    const VkDeviceSize jobUploadOffset = referenceUploadOffset + referenceBytes;
    const VkDeviceSize uploadBytes = jobUploadOffset + jobBytes;

    std::string error;
    if (!ensureBuffer(
            uploadBuffer_, uploadBytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, error)
        || !ensureBuffer(
            readbackBuffer_, outputBytes,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, error)
        || !ensureBuffer(
            sourceBuffer_, sourceBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false, error)
        || !ensureBuffer(
            referenceBuffer_, referenceBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false, error)
        || !ensureBuffer(
            runBuffer_, sizeof(SupportComputeRun),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false, error)
        || !ensureBuffer(
            jobBuffer_, jobBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false, error)
        || !ensureBuffer(
            outputBuffer_, outputBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false, error)) {
      failBatch(requests, error);
      return;
    }

    auto* upload = static_cast<std::byte*>(uploadBuffer_.mapped);
    VkDeviceSize sourceCursorBytes = 0u;
    VkDeviceSize referenceCursorBytes = referenceUploadOffset;
    for (const auto& request : requests) {
      const auto bytes = static_cast<VkDeviceSize>(request->batch.sourceWords.size())
          * sizeof(std::uint32_t);
      std::memcpy(upload + sourceCursorBytes, request->batch.sourceWords.data(),
                  static_cast<std::size_t>(bytes));
      std::memcpy(upload + referenceCursorBytes, request->batch.referenceWords.data(),
                  static_cast<std::size_t>(bytes));
      sourceCursorBytes += bytes;
      referenceCursorBytes += bytes;
    }
    std::memcpy(upload + jobUploadOffset, jobs.data(), static_cast<std::size_t>(jobBytes));

    std::array<VkDescriptorBufferInfo, 5> bufferInfos{
        VkDescriptorBufferInfo{sourceBuffer_.buffer, 0u, sourceBytes},
        VkDescriptorBufferInfo{referenceBuffer_.buffer, 0u, referenceBytes},
        VkDescriptorBufferInfo{jobBuffer_.buffer, 0u, jobBytes},
        VkDescriptorBufferInfo{outputBuffer_.buffer, 0u, outputBytes},
        VkDescriptorBufferInfo{runBuffer_.buffer, 0u, sizeof(SupportComputeRun)},
    };
    std::array<VkWriteDescriptorSet, 5> writes{};
    for (std::uint32_t binding = 0u; binding < writes.size(); ++binding) {
      writes[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[binding].dstSet = descriptorSet_;
      writes[binding].dstBinding = binding;
      writes[binding].descriptorCount = 1u;
      writes[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[binding].pBufferInfo = &bufferInfos[binding];
    }
    vkUpdateDescriptorSets(
        device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0u, nullptr);

    auto status = vkResetCommandBuffer(commandBuffer_, 0u);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkResetCommandBuffer", status));
      return;
    }
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    status = vkBeginCommandBuffer(commandBuffer_, &beginInfo);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkBeginCommandBuffer", status));
      return;
    }

    VkBufferCopy sourceCopy{0u, 0u, sourceBytes};
    VkBufferCopy referenceCopy{referenceUploadOffset, 0u, referenceBytes};
    VkBufferCopy jobCopy{jobUploadOffset, 0u, jobBytes};
    vkCmdCopyBuffer(commandBuffer_, uploadBuffer_.buffer, sourceBuffer_.buffer, 1u, &sourceCopy);
    vkCmdCopyBuffer(commandBuffer_, uploadBuffer_.buffer, referenceBuffer_.buffer, 1u, &referenceCopy);
    vkCmdCopyBuffer(commandBuffer_, uploadBuffer_.buffer, jobBuffer_.buffer, 1u, &jobCopy);
    vkCmdFillBuffer(commandBuffer_, outputBuffer_.buffer, 0u, outputBytes, 0u);

    std::array<VkBufferMemoryBarrier, 4> barriers{};
    const std::array<VkBuffer, 4> barrierBuffers{
        sourceBuffer_.buffer, referenceBuffer_.buffer, jobBuffer_.buffer, outputBuffer_.buffer};
    const std::array<VkDeviceSize, 4> barrierSizes{sourceBytes, referenceBytes, jobBytes, outputBytes};
    for (std::size_t index = 0u; index < barriers.size(); ++index) {
      auto& barrier = barriers[index];
      barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = index == 3u
          ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
          : VK_ACCESS_SHADER_READ_BIT;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer = barrierBuffers[index];
      barrier.offset = 0u;
      barrier.size = barrierSizes[index];
    }
    vkCmdPipelineBarrier(
        commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u,
        0u, nullptr, static_cast<std::uint32_t>(barriers.size()), barriers.data(), 0u, nullptr);

    struct PushConstants {
      std::uint32_t jobCount;
      std::uint32_t denseWordsPerTile;
      std::uint32_t runsPerTile;
    } constants{
        static_cast<std::uint32_t>(jobs.size()), kDenseWordsPerTile, kRunsPerTile};
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(
        commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_,
        0u, 1u, &descriptorSet_, 0u, nullptr);
    vkCmdPushConstants(
        commandBuffer_, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT,
        0u, sizeof(constants), &constants);
    vkCmdDispatch(
        commandBuffer_, maximumTileCount, maximumCandidateCount,
        static_cast<std::uint32_t>(jobs.size()));

    VkBufferMemoryBarrier outputBarrier{};
    outputBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    outputBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.buffer = outputBuffer_.buffer;
    outputBarrier.offset = 0u;
    outputBarrier.size = outputBytes;
    vkCmdPipelineBarrier(
        commandBuffer_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
        0u, nullptr, 1u, &outputBarrier, 0u, nullptr);
    VkBufferCopy outputCopy{0u, 0u, outputBytes};
    vkCmdCopyBuffer(
        commandBuffer_, outputBuffer_.buffer, readbackBuffer_.buffer, 1u, &outputCopy);

    VkBufferMemoryBarrier readbackBarrier{};
    readbackBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    readbackBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    readbackBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    readbackBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    readbackBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    readbackBarrier.buffer = readbackBuffer_.buffer;
    readbackBarrier.offset = 0u;
    readbackBarrier.size = outputBytes;
    vkCmdPipelineBarrier(
        commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
        0u, nullptr, 1u, &readbackBarrier, 0u, nullptr);

    status = vkEndCommandBuffer(commandBuffer_);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkEndCommandBuffer", status));
      return;
    }
    status = vkResetFences(device_, 1u, &fence_);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkResetFences", status));
      return;
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1u;
    submitInfo.pCommandBuffers = &commandBuffer_;
    const auto executionStart = Clock::now();
    status = vkQueueSubmit(queue_, 1u, &submitInfo, fence_);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkQueueSubmit", status));
      return;
    }
    status = vkWaitForFences(
        device_, 1u, &fence_, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
    const auto executionEnd = Clock::now();
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkWaitForFences", status));
      return;
    }

    const auto* readback = static_cast<const std::uint32_t*>(readbackBuffer_.mapped);
    for (std::size_t index = 0u; index < requests.size(); ++index) {
      const auto& job = jobs[index];
      std::memcpy(
          requests[index]->overlaps.data(), readback + job.outputOffset,
          static_cast<std::size_t>(job.candidateCount) * sizeof(std::uint32_t));
    }

    recordTransferBytes(
        static_cast<std::uint64_t>(uploadBytes), static_cast<std::uint64_t>(outputBytes));
    recordSubmittedWorkgroups(
        static_cast<std::uint64_t>(maximumTileCount) * maximumCandidateCount * jobs.size());
    recordBatchExecution(nanoseconds(executionEnd - executionStart));
    recordSuccessfulDispatch(requests.size());
    for (const auto& request : requests) {
      complete(request, true, {});
    }
  }

  void dispatchRunBatch(const std::vector<std::shared_ptr<PendingRequest>>& requests) {
    if (requests.empty()) {
      return;
    }
    const auto& firstBatch = requests.front()->runBatch;
    for (const auto& request : requests) {
      if (request->kind != RequestKind::Runs
          || request->runBatch.referenceKey != firstBatch.referenceKey
          || request->runBatch.width != firstBatch.width
          || request->runBatch.height != firstBatch.height
          || request->runBatch.wordsPerRow != firstBatch.wordsPerRow
          || request->runBatch.referenceWords.size() != firstBatch.referenceWords.size()) {
        failBatch(requests, "Vulkan run batch contains incompatible resident references");
        return;
      }
    }

    std::uint64_t totalRuns = 0u;
    std::uint64_t totalOutputWords = 0u;
    std::uint32_t maximumCandidateCount = 0u;
    std::uint32_t maximumTileCount = 0u;
    std::vector<GpuJobDescriptor> jobs;
    jobs.reserve(requests.size());
    for (const auto& request : requests) {
      const auto runCount = static_cast<std::uint64_t>(request->runBatch.sourceRuns.size());
      const auto diameter = static_cast<std::uint64_t>(request->runBatch.radius) * 2u + 1u;
      const auto candidateCount = diameter * diameter;
      std::uint64_t nextRuns = 0u;
      std::uint64_t nextOutput = 0u;
      if (!checkedAdd(totalRuns, runCount, nextRuns)
          || !checkedAdd(totalOutputWords, candidateCount, nextOutput)
          || totalRuns > std::numeric_limits<std::uint32_t>::max()
          || totalOutputWords > std::numeric_limits<std::uint32_t>::max()
          || runCount > std::numeric_limits<std::uint32_t>::max()) {
        failBatch(requests, "Vulkan support-compute run batch is too large");
        return;
      }
      const auto tileCount = static_cast<std::uint32_t>(
          (runCount + kRunsPerTile - 1u) / kRunsPerTile);
      jobs.push_back(GpuJobDescriptor{
          static_cast<std::uint32_t>(totalRuns),
          0u,
          request->runBatch.wordsPerRow,
          request->runBatch.height,
          request->runBatch.radius,
          static_cast<std::uint32_t>(candidateCount),
          static_cast<std::uint32_t>(totalOutputWords),
          static_cast<std::uint32_t>(runCount),
          request->runBatch.width,
          static_cast<std::uint32_t>(GpuInputMode::CompactRuns),
      });
      totalRuns = nextRuns;
      totalOutputWords = nextOutput;
      maximumCandidateCount = std::max(
          maximumCandidateCount, static_cast<std::uint32_t>(candidateCount));
      maximumTileCount = std::max(maximumTileCount, tileCount);
    }

    const VkDeviceSize runBytes = static_cast<VkDeviceSize>(totalRuns)
        * sizeof(SupportComputeRun);
    const VkDeviceSize referenceBytes = static_cast<VkDeviceSize>(firstBatch.referenceWords.size())
        * sizeof(std::uint32_t);
    const VkDeviceSize jobBytes = static_cast<VkDeviceSize>(jobs.size())
        * sizeof(GpuJobDescriptor);
    const VkDeviceSize outputBytes = static_cast<VkDeviceSize>(totalOutputWords)
        * sizeof(std::uint32_t);
    const auto maximumStorageRange = static_cast<VkDeviceSize>(
        physicalDeviceProperties_.limits.maxStorageBufferRange);
    if (requests.size() > 1u
        && (runBytes > maximumStorageRange
            || referenceBytes > maximumStorageRange
            || jobBytes > maximumStorageRange
            || outputBytes > maximumStorageRange)) {
      const auto middle = requests.begin()
          + static_cast<std::ptrdiff_t>(requests.size() / 2u);
      dispatchRunBatch(std::vector<std::shared_ptr<PendingRequest>>(
          requests.begin(), middle));
      dispatchRunBatch(std::vector<std::shared_ptr<PendingRequest>>(
          middle, requests.end()));
      return;
    }
    if (runBytes > maximumStorageRange
        || referenceBytes > maximumStorageRange
        || jobBytes > maximumStorageRange
        || outputBytes > maximumStorageRange) {
      failBatch(requests, "Vulkan support-compute run job exceeds maxStorageBufferRange");
      return;
    }
    if (maximumTileCount == 0u
        || maximumTileCount > physicalDeviceProperties_.limits.maxComputeWorkGroupCount[0]
        || maximumCandidateCount > physicalDeviceProperties_.limits.maxComputeWorkGroupCount[1]
        || jobs.size() > physicalDeviceProperties_.limits.maxComputeWorkGroupCount[2]) {
      failBatch(requests, "Vulkan support-compute run dispatch exceeds workgroup-count limits");
      return;
    }

    const auto batchStart = Clock::now();
    for (const auto& request : requests) {
      recordQueueWait(nanoseconds(batchStart - request->queuedAt));
    }

    const bool residentReusable = residentReferenceBuffer_.buffer != VK_NULL_HANDLE
        && residentReferenceBuffer_.capacity >= referenceBytes
        && residentReferenceKey_ == firstBatch.referenceKey
        && residentReferenceWidth_ == firstBatch.width
        && residentReferenceHeight_ == firstBatch.height
        && residentReferenceWordsPerRow_ == firstBatch.wordsPerRow;
    const bool uploadReference = !residentReusable;
    const VkDeviceSize referenceUploadOffset = 0u;
    const VkDeviceSize runUploadOffset = uploadReference ? referenceBytes : 0u;
    const VkDeviceSize jobUploadOffset = runUploadOffset + runBytes;
    const VkDeviceSize uploadBytes = jobUploadOffset + jobBytes;

    std::string error;
    if (!ensureBuffer(
            uploadBuffer_, std::max<VkDeviceSize>(uploadBytes, 4u),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, error)
        || !ensureBuffer(
            readbackBuffer_, outputBytes,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true, error)
        || !ensureBuffer(
            sourceBuffer_, sizeof(std::uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, error)
        || !ensureBuffer(
            residentReferenceBuffer_, referenceBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, error)
        || !ensureBuffer(
            runBuffer_, runBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, error)
        || !ensureBuffer(
            jobBuffer_, jobBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, error)
        || !ensureBuffer(
            outputBuffer_, outputBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, error)) {
      failBatch(requests, error);
      return;
    }

    auto* upload = static_cast<std::byte*>(uploadBuffer_.mapped);
    if (uploadReference) {
      std::memcpy(
          upload + referenceUploadOffset, firstBatch.referenceWords.data(),
          static_cast<std::size_t>(referenceBytes));
    }
    VkDeviceSize runCursor = runUploadOffset;
    for (const auto& request : requests) {
      const auto bytes = static_cast<VkDeviceSize>(request->runBatch.sourceRuns.size())
          * sizeof(SupportComputeRun);
      std::memcpy(
          upload + runCursor, request->runBatch.sourceRuns.data(),
          static_cast<std::size_t>(bytes));
      runCursor += bytes;
    }
    std::memcpy(upload + jobUploadOffset, jobs.data(), static_cast<std::size_t>(jobBytes));

    std::array<VkDescriptorBufferInfo, 5> bufferInfos{
        VkDescriptorBufferInfo{sourceBuffer_.buffer, 0u, sizeof(std::uint32_t)},
        VkDescriptorBufferInfo{residentReferenceBuffer_.buffer, 0u, referenceBytes},
        VkDescriptorBufferInfo{jobBuffer_.buffer, 0u, jobBytes},
        VkDescriptorBufferInfo{outputBuffer_.buffer, 0u, outputBytes},
        VkDescriptorBufferInfo{runBuffer_.buffer, 0u, runBytes},
    };
    std::array<VkWriteDescriptorSet, 5> writes{};
    for (std::uint32_t binding = 0u; binding < writes.size(); ++binding) {
      writes[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[binding].dstSet = descriptorSet_;
      writes[binding].dstBinding = binding;
      writes[binding].descriptorCount = 1u;
      writes[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[binding].pBufferInfo = &bufferInfos[binding];
    }
    vkUpdateDescriptorSets(
        device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0u, nullptr);

    auto status = vkResetCommandBuffer(commandBuffer_, 0u);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkResetCommandBuffer", status));
      return;
    }
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    status = vkBeginCommandBuffer(commandBuffer_, &beginInfo);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkBeginCommandBuffer", status));
      return;
    }

    if (uploadReference) {
      VkBufferCopy referenceCopy{referenceUploadOffset, 0u, referenceBytes};
      vkCmdCopyBuffer(
          commandBuffer_, uploadBuffer_.buffer, residentReferenceBuffer_.buffer,
          1u, &referenceCopy);
    }
    VkBufferCopy runCopy{runUploadOffset, 0u, runBytes};
    VkBufferCopy jobCopy{jobUploadOffset, 0u, jobBytes};
    vkCmdCopyBuffer(commandBuffer_, uploadBuffer_.buffer, runBuffer_.buffer, 1u, &runCopy);
    vkCmdCopyBuffer(commandBuffer_, uploadBuffer_.buffer, jobBuffer_.buffer, 1u, &jobCopy);
    vkCmdFillBuffer(commandBuffer_, outputBuffer_.buffer, 0u, outputBytes, 0u);

    std::array<VkBufferMemoryBarrier, 4> barriers{};
    std::size_t barrierCount = 0u;
    if (uploadReference) {
      auto& barrier = barriers[barrierCount++];
      barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer = residentReferenceBuffer_.buffer;
      barrier.offset = 0u;
      barrier.size = referenceBytes;
    }
    for (const auto [buffer, size, access] : std::array{
             std::tuple<VkBuffer, VkDeviceSize, VkAccessFlags>{
                 runBuffer_.buffer, runBytes, VK_ACCESS_SHADER_READ_BIT},
             std::tuple<VkBuffer, VkDeviceSize, VkAccessFlags>{
                 jobBuffer_.buffer, jobBytes, VK_ACCESS_SHADER_READ_BIT},
             std::tuple<VkBuffer, VkDeviceSize, VkAccessFlags>{
                 outputBuffer_.buffer, outputBytes,
                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT}}) {
      auto& barrier = barriers[barrierCount++];
      barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = access;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer = buffer;
      barrier.offset = 0u;
      barrier.size = size;
    }
    vkCmdPipelineBarrier(
        commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u,
        0u, nullptr, static_cast<std::uint32_t>(barrierCount), barriers.data(), 0u, nullptr);

    struct PushConstants {
      std::uint32_t jobCount;
      std::uint32_t denseWordsPerTile;
      std::uint32_t runsPerTile;
    } constants{
        static_cast<std::uint32_t>(jobs.size()), kDenseWordsPerTile, kRunsPerTile};
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(
        commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_,
        0u, 1u, &descriptorSet_, 0u, nullptr);
    vkCmdPushConstants(
        commandBuffer_, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT,
        0u, sizeof(constants), &constants);
    vkCmdDispatch(
        commandBuffer_, maximumTileCount, maximumCandidateCount,
        static_cast<std::uint32_t>(jobs.size()));

    VkBufferMemoryBarrier outputBarrier{};
    outputBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    outputBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    outputBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.buffer = outputBuffer_.buffer;
    outputBarrier.offset = 0u;
    outputBarrier.size = outputBytes;
    vkCmdPipelineBarrier(
        commandBuffer_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
        0u, nullptr, 1u, &outputBarrier, 0u, nullptr);
    VkBufferCopy outputCopy{0u, 0u, outputBytes};
    vkCmdCopyBuffer(commandBuffer_, outputBuffer_.buffer, readbackBuffer_.buffer, 1u, &outputCopy);
    VkBufferMemoryBarrier readbackBarrier{};
    readbackBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    readbackBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    readbackBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    readbackBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    readbackBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    readbackBarrier.buffer = readbackBuffer_.buffer;
    readbackBarrier.offset = 0u;
    readbackBarrier.size = outputBytes;
    vkCmdPipelineBarrier(
        commandBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
        0u, nullptr, 1u, &readbackBarrier, 0u, nullptr);

    status = vkEndCommandBuffer(commandBuffer_);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkEndCommandBuffer", status));
      return;
    }
    status = vkResetFences(device_, 1u, &fence_);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkResetFences", status));
      return;
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1u;
    submitInfo.pCommandBuffers = &commandBuffer_;
    const auto executionStart = Clock::now();
    status = vkQueueSubmit(queue_, 1u, &submitInfo, fence_);
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkQueueSubmit", status));
      return;
    }
    status = vkWaitForFences(
        device_, 1u, &fence_, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
    const auto executionEnd = Clock::now();
    if (status != VK_SUCCESS) {
      failBatch(requests, vkFailure("vkWaitForFences", status));
      return;
    }

    const auto* readback = static_cast<const std::uint32_t*>(readbackBuffer_.mapped);
    for (std::size_t index = 0u; index < requests.size(); ++index) {
      const auto& job = jobs[index];
      std::memcpy(
          requests[index]->overlaps.data(), readback + job.outputOffset,
          static_cast<std::size_t>(job.candidateCount) * sizeof(std::uint32_t));
    }

    if (uploadReference) {
      residentReferenceKey_ = firstBatch.referenceKey;
      residentReferenceWidth_ = firstBatch.width;
      residentReferenceHeight_ = firstBatch.height;
      residentReferenceWordsPerRow_ = firstBatch.wordsPerRow;
      recordResidentReferenceUpload();
    } else {
      recordResidentReferenceReuse();
    }
    recordRunSourceJobs(requests.size());
    recordTransferBytes(
        static_cast<std::uint64_t>(uploadBytes), static_cast<std::uint64_t>(outputBytes));
    recordSubmittedWorkgroups(
        static_cast<std::uint64_t>(maximumTileCount) * maximumCandidateCount * jobs.size());
    recordBatchExecution(nanoseconds(executionEnd - executionStart));
    recordSuccessfulDispatch(requests.size());
    for (const auto& request : requests) {
      complete(request, true, {});
    }
  }

  void initialise(std::string& error) {
    error.clear();
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "accloud-support-compute";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
    applicationInfo.pEngineName = "accloud";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &applicationInfo;
    auto status = vkCreateInstance(&instanceInfo, nullptr, &instance_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateInstance", status);
      return;
    }

    std::uint32_t physicalDeviceCount = 0u;
    status = vkEnumeratePhysicalDevices(instance_, &physicalDeviceCount, nullptr);
    if (status != VK_SUCCESS || physicalDeviceCount == 0u) {
      error = status == VK_SUCCESS
          ? "no Vulkan physical device is available"
          : vkFailure("vkEnumeratePhysicalDevices", status);
      return;
    }
    std::vector<VkPhysicalDevice> devices(physicalDeviceCount);
    status = vkEnumeratePhysicalDevices(instance_, &physicalDeviceCount, devices.data());
    if (status != VK_SUCCESS) {
      error = vkFailure("vkEnumeratePhysicalDevices", status);
      return;
    }

    int bestScore = std::numeric_limits<int>::min();
    for (const auto device : devices) {
      std::uint32_t queueFamilyCount = 0u;
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
      if (queueFamilyCount == 0u) {
        continue;
      }
      std::vector<VkQueueFamilyProperties> queues(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queues.data());
      for (std::uint32_t index = 0u; index < queueFamilyCount; ++index) {
        if (queues[index].queueCount == 0u
            || (queues[index].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0u) {
          continue;
        }
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        int score = 0;
        switch (properties.deviceType) {
          case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score = 400; break;
          case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 300; break;
          case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score = 200; break;
          case VK_PHYSICAL_DEVICE_TYPE_CPU: score = 100; break;
          default: score = 0; break;
        }
        if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u) {
          score += 25;
        }
        if (score > bestScore) {
          bestScore = score;
          physicalDevice_ = device;
          queueFamilyIndex_ = index;
          physicalDeviceProperties_ = properties;
        }
      }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
      error = "no Vulkan compute queue is available";
      return;
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamilyIndex_;
    queueInfo.queueCount = 1u;
    queueInfo.pQueuePriorities = &queuePriority;
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1u;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    status = vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateDevice", status);
      return;
    }
    vkGetDeviceQueue(device_, queueFamilyIndex_, 0u, &queue_);

    std::array<VkDescriptorSetLayoutBinding, 5> bindings{};
    for (std::uint32_t binding = 0u; binding < bindings.size(); ++binding) {
      bindings[binding].binding = binding;
      bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[binding].descriptorCount = 1u;
      bindings[binding].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    status = vkCreateDescriptorSetLayout(
        device_, &layoutInfo, nullptr, &descriptorSetLayout_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateDescriptorSetLayout", status);
      return;
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0u;
    pushRange.size = sizeof(std::uint32_t) * 3u;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1u;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1u;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    status = vkCreatePipelineLayout(
        device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreatePipelineLayout", status);
      return;
    }

    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = generated::kSupportOverlapSpirvWordCount
        * sizeof(std::uint32_t);
    shaderInfo.pCode = generated::kSupportOverlapSpirv;
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    status = vkCreateShaderModule(device_, &shaderInfo, nullptr, &shaderModule);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateShaderModule", status);
      return;
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = pipelineLayout_;
    status = vkCreateComputePipelines(
        device_, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &pipeline_);
    vkDestroyShaderModule(device_, shaderModule, nullptr);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateComputePipelines", status);
      return;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 5u;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1u;
    poolInfo.poolSizeCount = 1u;
    poolInfo.pPoolSizes = &poolSize;
    status = vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateDescriptorPool", status);
      return;
    }
    VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
    descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocateInfo.descriptorPool = descriptorPool_;
    descriptorAllocateInfo.descriptorSetCount = 1u;
    descriptorAllocateInfo.pSetLayouts = &descriptorSetLayout_;
    status = vkAllocateDescriptorSets(device_, &descriptorAllocateInfo, &descriptorSet_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkAllocateDescriptorSets", status);
      return;
    }

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = queueFamilyIndex_;
    status = vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &commandPool_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateCommandPool", status);
      return;
    }
    VkCommandBufferAllocateInfo commandAllocateInfo{};
    commandAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocateInfo.commandPool = commandPool_;
    commandAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAllocateInfo.commandBufferCount = 1u;
    status = vkAllocateCommandBuffers(device_, &commandAllocateInfo, &commandBuffer_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkAllocateCommandBuffers", status);
      return;
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    status = vkCreateFence(device_, &fenceInfo, nullptr, &fence_);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateFence", status);
      return;
    }

    ready_.store(true, std::memory_order_release);
    error = std::string("Vulkan compute device: ") + physicalDeviceProperties_.deviceName;
  }

  bool ensureBuffer(
      Buffer& buffer,
      VkDeviceSize requested,
      VkBufferUsageFlags usage,
      VkMemoryPropertyFlags properties,
      bool map,
      std::string& error) {
    if (buffer.buffer != VK_NULL_HANDLE
        && buffer.capacity >= requested
        && buffer.usage == usage
        && buffer.properties == properties
        && (!map || buffer.mapped != nullptr)) {
      return true;
    }
    destroyBuffer(buffer);
    VkDeviceSize capacity = 4096u;
    while (capacity < requested
           && capacity <= std::numeric_limits<VkDeviceSize>::max() / 2u) {
      capacity *= 2u;
    }
    if (capacity < requested) {
      capacity = requested;
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = capacity;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    auto status = vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer.buffer);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkCreateBuffer", status);
      return false;
    }
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer.buffer, &requirements);
    const auto memoryType = findMemoryType(requirements.memoryTypeBits, properties);
    if (!memoryType) {
      error = "Vulkan device has no compatible memory type for support compute";
      destroyBuffer(buffer);
      return false;
    }
    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = *memoryType;
    status = vkAllocateMemory(device_, &allocateInfo, nullptr, &buffer.memory);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkAllocateMemory", status);
      destroyBuffer(buffer);
      return false;
    }
    status = vkBindBufferMemory(device_, buffer.buffer, buffer.memory, 0u);
    if (status != VK_SUCCESS) {
      error = vkFailure("vkBindBufferMemory", status);
      destroyBuffer(buffer);
      return false;
    }
    if (map) {
      status = vkMapMemory(device_, buffer.memory, 0u, capacity, 0u, &buffer.mapped);
      if (status != VK_SUCCESS) {
        error = vkFailure("vkMapMemory", status);
        destroyBuffer(buffer);
        return false;
      }
    }
    buffer.capacity = capacity;
    buffer.usage = usage;
    buffer.properties = properties;
    return true;
  }

  std::optional<std::uint32_t> findMemoryType(
      std::uint32_t typeBits,
      VkMemoryPropertyFlags required) const {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &properties);
    for (std::uint32_t index = 0u; index < properties.memoryTypeCount; ++index) {
      if ((typeBits & (1u << index)) != 0u
          && (properties.memoryTypes[index].propertyFlags & required) == required) {
        return index;
      }
    }
    return std::nullopt;
  }

  void destroyBuffer(Buffer& buffer) {
    if (device_ == VK_NULL_HANDLE) {
      buffer = Buffer{};
      return;
    }
    if (buffer.mapped != nullptr && buffer.memory != VK_NULL_HANDLE) {
      vkUnmapMemory(device_, buffer.memory);
    }
    if (buffer.buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device_, buffer.buffer, nullptr);
    }
    if (buffer.memory != VK_NULL_HANDLE) {
      vkFreeMemory(device_, buffer.memory, nullptr);
    }
    buffer = Buffer{};
  }

  std::atomic_bool ready_{false};
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties physicalDeviceProperties_{};
  std::uint32_t queueFamilyIndex_ = 0u;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  VkFence fence_ = VK_NULL_HANDLE;
  Buffer uploadBuffer_;
  Buffer readbackBuffer_;
  Buffer sourceBuffer_;
  Buffer referenceBuffer_;
  Buffer residentReferenceBuffer_;
  Buffer runBuffer_;
  Buffer jobBuffer_;
  Buffer outputBuffer_;
  std::uint64_t residentReferenceKey_ = 0u;
  std::uint32_t residentReferenceWidth_ = 0u;
  std::uint32_t residentReferenceHeight_ = 0u;
  std::uint32_t residentReferenceWordsPerRow_ = 0u;

  std::mutex queueMutex_;
  std::condition_variable queueCv_;
  std::deque<std::shared_ptr<PendingRequest>> pending_;
  std::jthread dispatcher_;
};

} // namespace

std::unique_ptr<SupportComputeBackend> createVulkanSupportComputeBackend(
    std::string& error) {
  auto backend = std::make_unique<VulkanSupportComputeBackend>(error);
  if (!backend->ready()) {
    return nullptr;
  }
  return backend;
}

} // namespace accloud::render3d::compute
