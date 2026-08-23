#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace accloud::render3d {

// Coordinates replace-latest cut-surface requests across the GUI, worker and
// render threads. A request replacement invalidates every older build result.
// The final commit callback is serialized with replacement so a stale batch
// cannot replace the currently displayed cut surfaces.
template <typename Request, typename Result>
class CutSurfaceTransactionCoordinator {
public:
  struct ReadySnapshot {
    std::uint64_t generation = 0;
    std::optional<Result> result;
  };

  CutSurfaceTransactionCoordinator() = default;
  CutSurfaceTransactionCoordinator(const CutSurfaceTransactionCoordinator&) = delete;
  CutSurfaceTransactionCoordinator& operator=(const CutSurfaceTransactionCoordinator&) = delete;

  template <typename RequestFactory>
  std::uint64_t replaceRequest(RequestFactory&& factory) {
    std::uint64_t generation = 0;
    {
      std::scoped_lock lock(mutex_);
      generation = generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
      ready_.reset();
      request_ = std::invoke(std::forward<RequestFactory>(factory), generation);
    }
    requestChanged_.notify_all();
    return generation;
  }

  std::uint64_t invalidate() {
    return replaceRequest([](std::uint64_t) -> std::optional<Request> {
      return std::nullopt;
    });
  }

  [[nodiscard]] std::uint64_t currentGeneration() const noexcept {
    return generation_.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool isCurrent(std::uint64_t generation) const noexcept {
    return currentGeneration() == generation;
  }

  [[nodiscard]] std::optional<Request> waitAndTake(std::stop_token stopToken) {
    std::unique_lock lock(mutex_);
    requestChanged_.wait(
        lock,
        stopToken,
        [&] { return request_.has_value(); });
    if (stopToken.stop_requested()) {
      return std::nullopt;
    }
    auto request = std::move(request_);
    request_.reset();
    return request;
  }

  bool publishIfCurrent(std::uint64_t generation, Result result) {
    std::scoped_lock lock(mutex_);
    if (!isCurrent(generation)) {
      return false;
    }
    ready_ = ReadyResult{generation, std::move(result)};
    return true;
  }

  [[nodiscard]] ReadySnapshot takeReadySnapshot() {
    std::scoped_lock lock(mutex_);
    ReadySnapshot snapshot;
    snapshot.generation = currentGeneration();
    if (ready_ && ready_->generation == snapshot.generation) {
      snapshot.result = std::move(ready_->result);
    }
    ready_.reset();
    return snapshot;
  }

  template <typename Commit>
  bool commitIfCurrent(std::uint64_t generation, Commit&& commit) {
    std::scoped_lock lock(mutex_);
    if (!isCurrent(generation)) {
      return false;
    }
    std::invoke(std::forward<Commit>(commit));
    return true;
  }

  void notifyAll() noexcept {
    requestChanged_.notify_all();
  }

private:
  struct ReadyResult {
    std::uint64_t generation = 0;
    Result result;
  };

  mutable std::mutex mutex_;
  std::condition_variable_any requestChanged_;
  std::atomic<std::uint64_t> generation_{0};
  std::optional<Request> request_;
  std::optional<ReadyResult> ready_;
};

} // namespace accloud::render3d
