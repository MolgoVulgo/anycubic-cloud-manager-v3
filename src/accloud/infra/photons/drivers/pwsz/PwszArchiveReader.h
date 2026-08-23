#pragma once

#include "domain/photons/LayerIndex.h"
#include "domain/photons/LayerMaskSource.h"
#include "domain/photons/LayerSlice.h"
#include "domain/photons/PhotonsMeta.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace accloud::photons::pwsz {

class PwszArchiveReader final : public LayerMaskSource {
public:
  PwszArchiveReader() = default;

  [[nodiscard]] bool open(const std::filesystem::path& archivePath, std::string& error);
  [[nodiscard]] bool isOpen() const noexcept { return !archivePath_.empty(); }

  [[nodiscard]] const std::filesystem::path& archivePath() const noexcept {
    return archivePath_;
  }
  [[nodiscard]] const PhotonsMeta& meta() const noexcept { return meta_; }
  [[nodiscard]] const std::vector<LayerIndex>& layerIndex() const noexcept {
    return layerIndex_;
  }
  [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept {
    return diagnostics_;
  }

  [[nodiscard]] std::optional<LayerSlice> decodeLayer(
      std::size_t layerNumber,
      bool retainGray,
      std::string& error) const;

  [[nodiscard]] std::size_t layerCount() const noexcept override {
    return layerIndex_.size();
  }
  [[nodiscard]] std::uint32_t width() const noexcept override;
  [[nodiscard]] std::uint32_t height() const noexcept override;
  [[nodiscard]] bool supportsConcurrentMaskLoads() const noexcept override {
    return true;
  }
  [[nodiscard]] std::optional<BinaryMask> loadMask(
      std::size_t layerNumber,
      std::string& error) override;

private:
  struct ZipEntry {
    std::string name;
    std::uint16_t flags = 0;
    std::uint16_t compressionMethod = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t compressedSize = 0;
    std::uint32_t uncompressedSize = 0;
    std::uint32_t localHeaderOffset = 0;
  };

  [[nodiscard]] bool readDirectory(std::string& error);
  [[nodiscard]] std::optional<std::vector<std::uint8_t>> readEntry(
      const std::string& name,
      std::size_t maximumSize,
      std::string& error) const;
  [[nodiscard]] bool readMetadata(std::string& error);
  [[nodiscard]] bool buildLayerIndex(std::string& error);

  std::filesystem::path archivePath_;
  std::unordered_map<std::string, ZipEntry> entries_;
  PhotonsMeta meta_;
  std::vector<LayerIndex> layerIndex_;
  std::vector<std::string> diagnostics_;
};

} // namespace accloud::photons::pwsz
