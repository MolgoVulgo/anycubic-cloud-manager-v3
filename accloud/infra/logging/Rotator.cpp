#include "infra/logging/Rotator.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace accloud::logging {
namespace {

std::filesystem::path rotatedPath(const std::filesystem::path& path, int index) {
  return std::filesystem::path(path.string() + "." + std::to_string(index));
}

} // namespace

bool shouldRotateFile(const std::filesystem::path& path, const RotationPolicy& policy,
                      std::size_t pendingWriteBytes) {
  if (policy.maxBytes == 0) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return false;
  }
  const std::uintmax_t currentSize = std::filesystem::file_size(path, ec);
  if (ec) {
    return false;
  }
  return currentSize + pendingWriteBytes > policy.maxBytes;
}

void rotateFile(const std::filesystem::path& path, const RotationPolicy& policy) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return;
  }

  const int retention = policy.retention > 0 ? policy.retention : 1;
  std::filesystem::remove(rotatedPath(path, retention), ec);
  ec.clear();

  for (int i = retention - 1; i >= 1; --i) {
    const std::filesystem::path from = rotatedPath(path, i);
    const std::filesystem::path to = rotatedPath(path, i + 1);
    if (std::filesystem::exists(from, ec) && !ec) {
      std::filesystem::rename(from, to, ec);
      ec.clear();
    }
  }

  std::filesystem::rename(path, rotatedPath(path, 1), ec);
}

} // namespace accloud::logging
