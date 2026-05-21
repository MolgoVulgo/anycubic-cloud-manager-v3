#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace accloud::cloud {

struct CloudSession {
  std::string accessToken;
  std::optional<std::string> refreshToken;
  std::chrono::system_clock::time_point expiresAt;
  bool isAuthenticated = false;
};

} // namespace accloud::cloud
