#include "render3d/core/OrbitCamera.h"

#include <algorithm>
#include <cmath>

namespace accloud::render3d {
namespace {
constexpr double kHalfPi = 1.5707963267948966;
constexpr double kPitchMargin = 0.017453292519943295;
constexpr double kMinimumDistance = 0.001;
constexpr double kPi = 3.14159265358979323846;
}

void OrbitCamera::orbit(double deltaYawRadians, double deltaPitchRadians) noexcept {
  yaw_ += deltaYawRadians;
  pitch_ = std::clamp(
      pitch_ + deltaPitchRadians,
      -kHalfPi + kPitchMargin,
      kHalfPi - kPitchMargin);
}

void OrbitCamera::zoom(double wheelSteps) noexcept {
  distance_ = std::max(kMinimumDistance, distance_ * std::exp(-wheelSteps * 0.1));
}

void OrbitCamera::pan(double rightMm, double upMm) noexcept {
  const double sinYaw = std::sin(yaw_);
  const double cosYaw = std::cos(yaw_);
  target_.x += cosYaw * rightMm - sinYaw * std::sin(pitch_) * upMm;
  target_.y += sinYaw * rightMm + cosYaw * std::sin(pitch_) * upMm;
  target_.z += std::cos(pitch_) * upMm;
}

void OrbitCamera::fit(
    const photons::MeshBounds& bounds,
    double verticalFovDegrees) noexcept {
  if (!bounds.valid()) {
    return;
  }

  target_ = Vec3{
      (static_cast<double>(bounds.minX) + bounds.maxX) * 0.5,
      (static_cast<double>(bounds.minY) + bounds.maxY) * 0.5,
      (static_cast<double>(bounds.minZ) + bounds.maxZ) * 0.5,
  };
  const double dx = static_cast<double>(bounds.maxX) - bounds.minX;
  const double dy = static_cast<double>(bounds.maxY) - bounds.minY;
  const double dz = static_cast<double>(bounds.maxZ) - bounds.minZ;
  const double radius = std::max(kMinimumDistance, 0.5 * std::sqrt(dx * dx + dy * dy + dz * dz));
  const double fov = std::clamp(verticalFovDegrees, 1.0, 179.0) * kPi / 180.0;
  distance_ = radius / std::sin(fov * 0.5) * 1.1;
}

Vec3 OrbitCamera::position() const noexcept {
  const double horizontal = distance_ * std::cos(pitch_);
  return Vec3{
      target_.x + horizontal * std::cos(yaw_),
      target_.y + horizontal * std::sin(yaw_),
      target_.z + distance_ * std::sin(pitch_),
  };
}

} // namespace accloud::render3d
