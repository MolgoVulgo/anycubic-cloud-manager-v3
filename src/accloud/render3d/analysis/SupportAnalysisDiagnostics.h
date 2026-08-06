#pragma once

#include "render3d/analysis/SupportAnalyzer.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace accloud::render3d {

struct SupportAnalysisBundleMetadata {
  std::string inputFileName;
  double pitchXMillimetres = 1.0;
  double pitchYMillimetres = 1.0;
  double pitchZMillimetres = 1.0;
};

struct SupportAnalysisBundleOptions {
  std::uint32_t downsample = 16;
  bool writeImages = true;
};

struct SupportAnalysisBundleCallbacks {
  std::function<bool()> isCancelled;
  std::function<void(std::size_t completedLayers, std::size_t totalLayers)> progress;
};

class SupportAnalysisBundleWriter {
public:
  [[nodiscard]] bool write(
      photons::LayerMaskSource& source,
      const SupportAnalyzer& analyzer,
      const SupportAnalysisResult& result,
      const SupportAnalysisOptions& analysisOptions,
      const SupportAnalysisBundleMetadata& metadata,
      const std::filesystem::path& outputDirectory,
      const SupportAnalysisBundleOptions& options,
      const SupportAnalysisBundleCallbacks& callbacks,
      std::string& error) const;
};

[[nodiscard]] const char* supportDecisionReasonCode(
    SupportDecisionReason reason) noexcept;
[[nodiscard]] const char* supportDecisionReasonText(
    SupportDecisionReason reason) noexcept;

} // namespace accloud::render3d
