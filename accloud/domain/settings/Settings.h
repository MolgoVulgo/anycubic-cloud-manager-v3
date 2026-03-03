#pragma once

#include <cstdint>

namespace accloud::settings {

struct Settings {
  std::uint64_t ramCacheBudgetBytes = 256ull * 1024ull * 1024ull;
  std::uint64_t diskCacheBudgetBytes = 4ull * 1024ull * 1024ull * 1024ull;
  bool debugLogsEnabled = false;
  int renderStride = 1;
  int renderLod = 0;
};

} // namespace accloud::settings
