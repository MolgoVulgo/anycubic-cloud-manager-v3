#pragma once

#include "domain/photons/MeshChunk.h"

#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

namespace accloud::render3d {

class UploadQueue {
public:
  explicit UploadQueue(
      std::size_t maximumChunks = 8,
      std::size_t maximumBytes = 256u * 1024u * 1024u) noexcept;

  [[nodiscard]] bool tryPush(photons::MeshChunk&& chunk);
  [[nodiscard]] std::vector<photons::MeshChunk> takeAll();
  void clear() noexcept;

  [[nodiscard]] std::size_t pendingChunks() const noexcept;
  [[nodiscard]] std::size_t pendingBytes() const noexcept;
  [[nodiscard]] std::size_t maximumChunks() const noexcept { return maximumChunks_; }
  [[nodiscard]] std::size_t maximumBytes() const noexcept { return maximumBytes_; }

  [[nodiscard]] static std::size_t byteSize(const photons::MeshChunk& chunk) noexcept;

private:
  const std::size_t maximumChunks_;
  const std::size_t maximumBytes_;
  mutable std::mutex mutex_;
  std::deque<photons::MeshChunk> queue_;
  std::size_t pendingBytes_ = 0;
};

} // namespace accloud::render3d
