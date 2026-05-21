#pragma once

#include <cstdint>
#include <string>

namespace accloud::cloud {

struct CloudFile {
  std::string id;
  std::string name;
  std::uint64_t sizeBytes = 0;
  std::string contentType;
};

struct CloudPrinter {
  std::string id;
  std::string model;
  std::string firmwareVersion;
  bool online = false;
};

} // namespace accloud::cloud
