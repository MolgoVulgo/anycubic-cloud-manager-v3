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
#include <utility>
#include <vector>

namespace accloud::render3d::compute {
namespace {

constexpr std::uint32_t kShaderLocalSize = 64u;
constexpr std::size_t kMaximumBatchJobs = 64u;
constexpr auto kCoalesceWindow = std::chrono::microseconds(250);

struct GpuJobDescriptor {
  std::uint32_t sourceWordOffset = 0u;
  std::uint32_t referenceWordOffset = 0u;
  std::uint32_t wordsPerRow = 0u;
  std::uint32_t height = 0u;
  std::uint32_t radius = 0u;
  std::uint32_t candidateCount = 0u;
  std::uint32_t outputOffset = 0u;
  std::uint32_t wordCount = 0u;
};
static_assert(sizeof(GpuJobDescriptor) == 32u);

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

  struct PendingRequest {
    TranslatedOverlapBatch batch;
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

  void dispatchLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
      std::vector<std::shared_ptr<PendingRequest>> requests;
      {
        std::unique_lock queueLock(queueMutex_);
        queueCv_.wait(queueLock, [&] {
          return stopToken.stop_requested() || !pending_.empty();
        });
        if (stopToken.stop_requested()) {
          break;
        }

        const auto deadline = Clock::now() + kCoalesceWindow;
        queueCv_.wait_until(queueLock, deadline, [&] {
          return stopToken.stop_requested()
              || pending_.size() >= kMaximumBatchJobs;
        });
        if (stopToken.stop_requested()) {
          break;
        }

        const auto count = std::min(pending_.size(), kMaximumBatchJobs);
        requests.reserve(count);
        for (std::size_t index = 0u; index < count; ++index) {
          requests.push_back(std::move(pending_.front()));
          pending_.pop_front();
        }
      }
      dispatchBatch(requests);
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

  void dispatchBatch(const std::vector<std::shared_ptr<PendingRequest>>& requests) {
    if (requests.empty()) {
      return;
    }

    std::uint64_t totalSourceWords = 0u;
    std::uint64_t totalReferenceWords = 0u;
    std::uint64_t totalOutputWords = 0u;
    std::uint32_t maximumCandidateCount = 0u;
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
        failBatch(requests, "Vulkan support-compute coalesced batch is too large");
        return;
      }
      jobs.push_back(GpuJobDescriptor{
          static_cast<std::uint32_t>(totalSourceWords),
          static_cast<std::uint32_t>(totalReferenceWords),
          request->batch.wordsPerRow,
          request->batch.height,
          request->batch.radius,
          static_cast<std::uint32_t>(candidateCount),
          static_cast<std::uint32_t>(totalOutputWords),
          static_cast<std::uint32_t>(wordCount),
      });
      totalSourceWords = nextSourceWords;
      totalReferenceWords = nextReferenceWords;
      totalOutputWords = nextOutputWords;
      maximumCandidateCount = std::max(
          maximumCandidateCount, static_cast<std::uint32_t>(candidateCount));
    }

    if (totalSourceWords > std::numeric_limits<std::uint32_t>::max()
        || totalReferenceWords > std::numeric_limits<std::uint32_t>::max()
        || totalOutputWords > std::numeric_limits<std::uint32_t>::max()) {
      failBatch(requests, "Vulkan support-compute coalesced offsets overflow 32-bit storage");
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

    // A storage-buffer descriptor may be smaller than the device's available
    // memory. Keep coalescing opportunistic, but split an oversized group
    // rather than turning a valid individual job into a CPU fallback merely
    // because too many workers reached the dispatcher at the same time.
    const auto maximumStorageRange = static_cast<VkDeviceSize>(
        physicalDeviceProperties_.limits.maxStorageBufferRange);
    if (requests.size() > 1u
        && (sourceBytes > maximumStorageRange
            || referenceBytes > maximumStorageRange
            || jobBytes > maximumStorageRange
            || outputBytes > maximumStorageRange)) {
      const auto middle = requests.begin()
          + static_cast<std::ptrdiff_t>(requests.size() / 2u);
      dispatchBatch(std::vector<std::shared_ptr<PendingRequest>>(
          requests.begin(), middle));
      dispatchBatch(std::vector<std::shared_ptr<PendingRequest>>(
          middle, requests.end()));
      return;
    }
    if (sourceBytes > maximumStorageRange
        || referenceBytes > maximumStorageRange
        || jobBytes > maximumStorageRange
        || outputBytes > maximumStorageRange) {
      failBatch(requests, "Vulkan support-compute job exceeds maxStorageBufferRange");
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
            jobBuffer_, jobBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            false, error)
        || !ensureBuffer(
            outputBuffer_, outputBytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
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
      std::memcpy(
          upload + sourceCursorBytes,
          request->batch.sourceWords.data(),
          static_cast<std::size_t>(bytes));
      std::memcpy(
          upload + referenceCursorBytes,
          request->batch.referenceWords.data(),
          static_cast<std::size_t>(bytes));
      sourceCursorBytes += bytes;
      referenceCursorBytes += bytes;
    }
    std::memcpy(
        upload + jobUploadOffset,
        jobs.data(),
        static_cast<std::size_t>(jobBytes));

    std::array<VkDescriptorBufferInfo, 4> bufferInfos{
        VkDescriptorBufferInfo{sourceBuffer_.buffer, 0u, sourceBytes},
        VkDescriptorBufferInfo{referenceBuffer_.buffer, 0u, referenceBytes},
        VkDescriptorBufferInfo{jobBuffer_.buffer, 0u, jobBytes},
        VkDescriptorBufferInfo{outputBuffer_.buffer, 0u, outputBytes},
    };
    std::array<VkWriteDescriptorSet, 4> writes{};
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

    std::array<VkBufferCopy, 3> inputCopies{
        VkBufferCopy{0u, 0u, sourceBytes},
        VkBufferCopy{referenceUploadOffset, 0u, referenceBytes},
        VkBufferCopy{jobUploadOffset, 0u, jobBytes},
    };
    vkCmdCopyBuffer(
        commandBuffer_, uploadBuffer_.buffer, sourceBuffer_.buffer,
        1u, &inputCopies[0]);
    vkCmdCopyBuffer(
        commandBuffer_, uploadBuffer_.buffer, referenceBuffer_.buffer,
        1u, &inputCopies[1]);
    vkCmdCopyBuffer(
        commandBuffer_, uploadBuffer_.buffer, jobBuffer_.buffer,
        1u, &inputCopies[2]);

    std::array<VkBufferMemoryBarrier, 3> inputBarriers{};
    const std::array<VkBuffer, 3> inputBuffers{
        sourceBuffer_.buffer, referenceBuffer_.buffer, jobBuffer_.buffer};
    const std::array<VkDeviceSize, 3> inputSizes{sourceBytes, referenceBytes, jobBytes};
    for (std::size_t index = 0u; index < inputBarriers.size(); ++index) {
      auto& barrier = inputBarriers[index];
      barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer = inputBuffers[index];
      barrier.offset = 0u;
      barrier.size = inputSizes[index];
    }
    vkCmdPipelineBarrier(
        commandBuffer_,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0u,
        0u, nullptr,
        static_cast<std::uint32_t>(inputBarriers.size()), inputBarriers.data(),
        0u, nullptr);

    const std::uint32_t jobCount = static_cast<std::uint32_t>(jobs.size());
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(
        commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_,
        0u, 1u, &descriptorSet_, 0u, nullptr);
    vkCmdPushConstants(
        commandBuffer_, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT,
        0u, sizeof(jobCount), &jobCount);
    vkCmdDispatch(commandBuffer_, maximumCandidateCount, jobCount, 1u);

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
        commandBuffer_,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0u,
        0u, nullptr,
        1u, &outputBarrier,
        0u, nullptr);

    VkBufferCopy outputCopy{0u, 0u, outputBytes};
    vkCmdCopyBuffer(
        commandBuffer_, outputBuffer_.buffer, readbackBuffer_.buffer,
        1u, &outputCopy);

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
        commandBuffer_,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0u,
        0u, nullptr,
        1u, &readbackBarrier,
        0u, nullptr);

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
          requests[index]->overlaps.data(),
          readback + job.outputOffset,
          static_cast<std::size_t>(job.candidateCount) * sizeof(std::uint32_t));
    }

    recordTransferBytes(
        static_cast<std::uint64_t>(uploadBytes),
        static_cast<std::uint64_t>(outputBytes));
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

    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
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
    pushRange.size = sizeof(std::uint32_t);
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
    poolSize.descriptorCount = 4u;
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
  Buffer jobBuffer_;
  Buffer outputBuffer_;

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
