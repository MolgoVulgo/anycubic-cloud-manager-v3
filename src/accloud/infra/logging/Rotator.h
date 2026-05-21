#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace accloud::logging {

struct RotationPolicy {
  std::uintmax_t maxBytes = 2U * 1024U * 1024U;
  int retention = 5;
};

[[nodiscard]] bool shouldRotateFile(const std::filesystem::path& path,
                                    const RotationPolicy& policy,
                                    std::size_t pendingWriteBytes = 0);
void rotateFile(const std::filesystem::path& path, const RotationPolicy& policy);

} // namespace accloud::logging
