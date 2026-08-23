#include "render3d/core/CutSurfaceTransactionCoordinator.h"

#include <cassert>
#include <cstdint>
#include <optional>

namespace {

struct Request {
  std::uint64_t generation = 0;
  int firstLayer = 0;
  int lastLayer = 0;
};

struct Result {
  std::uint64_t generation = 0;
  int firstLayer = 0;
  int lastLayer = 0;
};

struct DisplayedRange {
  std::uint64_t generation = 0;
  int firstLayer = 1;
  int lastLayer = 100;
};

} // namespace

int main() {
  using Coordinator =
      accloud::render3d::CutSurfaceTransactionCoordinator<Request, Result>;

  Coordinator coordinator;
  DisplayedRange displayed;

  const auto generation1 = coordinator.replaceRequest(
      [](std::uint64_t generation) -> std::optional<Request> {
        return Request{generation, 10, 90};
      });
  auto request1 = coordinator.waitAndTake(std::stop_token{});
  assert(request1.has_value());
  assert(request1->generation == generation1);

  const auto generation2 = coordinator.replaceRequest(
      [](std::uint64_t generation) -> std::optional<Request> {
        return Request{generation, 20, 80};
      });
  const auto generation3 = coordinator.replaceRequest(
      [](std::uint64_t generation) -> std::optional<Request> {
        return Request{generation, 30, 70};
      });

  // A rapid slider movement keeps only the newest request pending.
  auto latestRequest = coordinator.waitAndTake(std::stop_token{});
  assert(latestRequest.has_value());
  assert(latestRequest->generation == generation3);
  assert(latestRequest->firstLayer == 30);
  assert(latestRequest->lastLayer == 70);
  assert(!coordinator.isCurrent(generation1));
  assert(!coordinator.isCurrent(generation2));
  assert(coordinator.isCurrent(generation3));

  // Intermediate worker results are rejected and the displayed range remains
  // unchanged until the latest complete batch is committed.
  assert(!coordinator.publishIfCurrent(
      generation2,
      Result{generation2, 20, 80}));
  assert(displayed.firstLayer == 1);
  assert(displayed.lastLayer == 100);

  assert(coordinator.publishIfCurrent(
      generation3,
      Result{generation3, 30, 70}));
  auto ready3 = coordinator.takeReadySnapshot();
  assert(ready3.generation == generation3);
  assert(ready3.result.has_value());

  // A newer slider request arriving after CPU publication but before GPU
  // commit invalidates the prepared batch without touching the active range.
  const auto generation4 = coordinator.replaceRequest(
      [](std::uint64_t generation) -> std::optional<Request> {
        return Request{generation, 40, 60};
      });
  const bool staleCommit = coordinator.commitIfCurrent(
      ready3.result->generation,
      [&] {
        displayed = DisplayedRange{
            ready3.result->generation,
            ready3.result->firstLayer,
            ready3.result->lastLayer};
      });
  assert(!staleCommit);
  assert(displayed.generation == 0);
  assert(displayed.firstLayer == 1);
  assert(displayed.lastLayer == 100);

  auto request4 = coordinator.waitAndTake(std::stop_token{});
  assert(request4.has_value());
  assert(request4->generation == generation4);
  assert(coordinator.publishIfCurrent(
      generation4,
      Result{generation4, 40, 60}));
  auto ready4 = coordinator.takeReadySnapshot();
  assert(ready4.result.has_value());

  const bool committed = coordinator.commitIfCurrent(
      ready4.result->generation,
      [&] {
        displayed = DisplayedRange{
            ready4.result->generation,
            ready4.result->firstLayer,
            ready4.result->lastLayer};
      });
  assert(committed);
  assert(displayed.generation == generation4);
  assert(displayed.firstLayer == 40);
  assert(displayed.lastLayer == 60);

  coordinator.invalidate();
  assert(!coordinator.isCurrent(generation4));
  assert(!coordinator.publishIfCurrent(
      generation4,
      Result{generation4, 40, 60}));

  return 0;
}
