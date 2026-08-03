#include "render3d/gl/UploadQueue.h"

#include <algorithm>
#include <utility>

namespace accloud::render3d {

bool GpuMemoryBudget::tryReserve(std::size_t bytes) noexcept {
  if (bytes > remainingBytes()) {
    return false;
  }
  residentBytes_ += bytes;
  return true;
}

void GpuMemoryBudget::release(std::size_t bytes) noexcept {
  residentBytes_ -= std::min(bytes, residentBytes_);
}

UploadQueue::UploadQueue(
    std::size_t maximumChunks,
    std::size_t maximumBytes) noexcept
    : maximumChunks_(maximumChunks == 0 ? 1 : maximumChunks),
      maximumBytes_(maximumBytes == 0 ? 1 : maximumBytes) {}

std::size_t UploadQueue::byteSize(const photons::MeshChunk& chunk) noexcept {
  return chunk.compactByteSize();
}

std::size_t UploadQueue::legacyEquivalentByteSize(
    const photons::MeshChunk& chunk) noexcept {
  return chunk.legacyEquivalentByteSize();
}

bool UploadQueue::tryPush(photons::MeshChunk&& chunk) {
  const std::size_t bytes = byteSize(chunk);
  std::scoped_lock lock(mutex_);
  if (queue_.size() >= maximumChunks_) {
    return false;
  }
  if (!queue_.empty() && bytes > maximumBytes_ - pendingBytes_) {
    return false;
  }
  queue_.push_back(std::move(chunk));
  pendingBytes_ += bytes;
  return true;
}

std::vector<photons::MeshChunk> UploadQueue::takeAll() {
  std::vector<photons::MeshChunk> chunks;
  std::scoped_lock lock(mutex_);
  chunks.reserve(queue_.size());
  while (!queue_.empty()) {
    chunks.push_back(std::move(queue_.front()));
    queue_.pop_front();
  }
  pendingBytes_ = 0;
  return chunks;
}

void UploadQueue::clear() noexcept {
  std::scoped_lock lock(mutex_);
  queue_.clear();
  pendingBytes_ = 0;
}

std::size_t UploadQueue::pendingChunks() const noexcept {
  std::scoped_lock lock(mutex_);
  return queue_.size();
}

std::size_t UploadQueue::pendingBytes() const noexcept {
  std::scoped_lock lock(mutex_);
  return pendingBytes_;
}

} // namespace accloud::render3d
