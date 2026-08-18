#include "render3d/compute/SupportComputeBackend.h"

#if defined(ACCLOUD_WITH_VULKAN_COMPUTE)
#include "render3d/compute/VulkanSupportComputeBackend.h"
#endif

namespace accloud::render3d::compute {

bool vulkanSupportComputeCompiled() noexcept {
#if defined(ACCLOUD_WITH_VULKAN_COMPUTE)
  return true;
#else
  return false;
#endif
}

std::unique_ptr<SupportComputeBackend> createSupportComputeBackend(
    SupportComputePreference preference,
    std::string& diagnostic) {
  diagnostic.clear();
  if (preference == SupportComputePreference::Cpu) {
    return nullptr;
  }
#if defined(ACCLOUD_WITH_VULKAN_COMPUTE)
  auto backend = createVulkanSupportComputeBackend(diagnostic);
  if (backend) {
    return backend;
  }
#else
  diagnostic = "Vulkan compute support was not compiled into this build";
#endif
  return nullptr;
}

} // namespace accloud::render3d::compute
