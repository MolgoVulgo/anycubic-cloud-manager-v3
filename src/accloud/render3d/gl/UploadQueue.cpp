#include "render3d/gl/UploadQueue.h"

#include <utility>

namespace accloud::render3d {

UploadQueue::UploadQueue(
    std::size_t maximumChunks,
    std::size_t maximumBytes) noexcept
    : maximumChunks_(maximumChunks == 0 ? 1 : maximumChunks),
      maximumBytes_(maximumBytes == 0 ? 1 : maximumBytes) {}

std::size_t UploadQueue::byteSize(const photons::MeshChunk& chunk) noexcept {
  return chunk.vertices.size() * sizeof(photons::MeshVertex)
         + chunk.indices.size() * sizeof(std::uint32_t);
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
