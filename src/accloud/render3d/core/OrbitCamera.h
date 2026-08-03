#pragma once

#include "domain/photons/MeshChunk.h"

namespace accloud::render3d {

struct Vec3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

class OrbitCamera {
public:
  void orbit(double deltaYawRadians, double deltaPitchRadians) noexcept;
  void zoom(double wheelSteps) noexcept;
  void pan(double rightMm, double upMm) noexcept;
  void fit(const photons::MeshBounds& bounds, double verticalFovDegrees = 45.0) noexcept;

  [[nodiscard]] Vec3 target() const noexcept { return target_; }
  [[nodiscard]] Vec3 position() const noexcept;
  [[nodiscard]] double distance() const noexcept { return distance_; }
  [[nodiscard]] double yaw() const noexcept { return yaw_; }
  [[nodiscard]] double pitch() const noexcept { return pitch_; }

private:
  Vec3 target_{};
  double yaw_ = 0.7853981633974483;
  double pitch_ = 0.5235987755982988;
  double distance_ = 100.0;
};

} // namespace accloud::render3d
