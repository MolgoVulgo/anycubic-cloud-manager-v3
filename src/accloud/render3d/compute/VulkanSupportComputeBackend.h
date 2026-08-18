#pragma once

#include "render3d/compute/SupportComputeBackend.h"

#include <memory>
#include <string>

namespace accloud::render3d::compute {

[[nodiscard]] std::unique_ptr<SupportComputeBackend> createVulkanSupportComputeBackend(
    std::string& error);

} // namespace accloud::render3d::compute
