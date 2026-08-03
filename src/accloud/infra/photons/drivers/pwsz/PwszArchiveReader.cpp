#include "infra/photons/drivers/pwsz/PwszArchiveReader.h"

#include "infra/photons/drivers/pwmb/codec/Pw0Decoder.h"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <regex>
#include <span>
#include <utility>

namespace accloud::photons::pwsz {
namespace {

constexpr std::uint32_t kLocalHeaderSignature = 0x04034b50u;
constexpr std::uint32_t kCentralHeaderSignature = 0x02014b50u;
constexpr std::uint32_t kEndOfCentralDirectorySignature = 0x06054b50u;
constexpr std::size_t kLocalHeaderSize = 30;
constexpr std::size_t kCentralHeaderSize = 46;
constexpr std::size_t kEndOfCentralDirectorySize = 22;
constexpr std::size_t kMaximumZipCommentSize = 65535;
constexpr std::size_t kMaximumMetadataSize = 16u * 1024u * 1024u;
constexpr std::size_t kMaximumLayerStreamSize = 512u * 1024u * 1024u;

std::uint16_t readLe16(const std::uint8_t* bytes) {
  return static_cast<std::uint16_t>(bytes[0])
         | (static_cast<std::uint16_t>(bytes[1]) << 8u);
}

std::uint32_t readLe32(const std::uint8_t* bytes) {
  return static_cast<std::uint32_t>(bytes[0])
         | (static_cast<std::uint32_t>(bytes[1]) << 8u)
         | (static_cast<std::uint32_t>(bytes[2]) << 16u)
         | (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

bool readExact(std::istream& input, void* destination, std::size_t size) {
  if (size == 0) {
    return true;
  }
  input.read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
  return input.gcount() == static_cast<std::streamsize>(size);
}

std::optional<nlohmann::json> parseJson(
    const std::vector<std::uint8_t>& bytes,
    std::string_view entryName,
    std::string& error) {
  try {
    return nlohmann::json::parse(bytes.begin(), bytes.end());
  } catch (const nlohmann::json::exception& exception) {
    error = std::string(entryName) + " contains invalid JSON: " + exception.what();
    return std::nullopt;
  }
}

std::optional<double> numberAt(const nlohmann::json& object, std::string_view key) {
  const auto found = object.find(std::string(key));
  if (found == object.end() || !found->is_number()) {
    return std::nullopt;
  }
  return found->get<double>();
}

std::optional<std::uint32_t> uintAt(const nlohmann::json& object, std::string_view key) {
  const auto found = object.find(std::string(key));
  if (found == object.end() || !found->is_number_unsigned()) {
    if (found == object.end() || !found->is_number_integer()) {
      return std::nullopt;
    }
    const auto value = found->get<std::int64_t>();
    if (value < 0 || value > std::numeric_limits<std::uint32_t>::max()) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
  }
  const auto value = found->get<std::uint64_t>();
  if (value > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(value);
}

} // namespace

bool PwszArchiveReader::open(
    const std::filesystem::path& archivePath,
    std::string& error) {
  archivePath_.clear();
  entries_.clear();
  meta_ = {};
  layerIndex_.clear();
  diagnostics_.clear();

  std::error_code fileError;
  if (!std::filesystem::is_regular_file(archivePath, fileError) || fileError) {
    error = "PWSZ archive is not a readable regular file";
    return false;
  }

  archivePath_ = archivePath;
  if (!readDirectory(error) || !readMetadata(error) || !buildLayerIndex(error)) {
    archivePath_.clear();
    entries_.clear();
    meta_ = {};
    layerIndex_.clear();
    return false;
  }
  return true;
}

bool PwszArchiveReader::readDirectory(std::string& error) {
  std::error_code fileError;
  const auto fileSize = std::filesystem::file_size(archivePath_, fileError);
  if (fileError || fileSize < kEndOfCentralDirectorySize) {
    error = "PWSZ ZIP archive is too short";
    return false;
  }
  if (fileSize > std::numeric_limits<std::uint32_t>::max()) {
    error = "ZIP64 PWSZ archives are not supported";
    return false;
  }

  std::ifstream input(archivePath_, std::ios::binary);
  if (!input) {
    error = "PWSZ archive cannot be opened";
    return false;
  }

  const std::size_t tailSize = static_cast<std::size_t>(std::min<std::uintmax_t>(
      fileSize,
      kEndOfCentralDirectorySize + kMaximumZipCommentSize));
  std::vector<std::uint8_t> tail(tailSize);
  input.seekg(static_cast<std::streamoff>(fileSize - tailSize), std::ios::beg);
  if (!readExact(input, tail.data(), tail.size())) {
    error = "PWSZ ZIP end directory cannot be read";
    return false;
  }

  std::optional<std::size_t> eocdOffset;
  for (std::size_t offset = tail.size() - kEndOfCentralDirectorySize + 1;
       offset-- > 0;) {
    if (readLe32(tail.data() + offset) != kEndOfCentralDirectorySignature) {
      continue;
    }
    const auto commentLength = readLe16(tail.data() + offset + 20);
    if (offset + kEndOfCentralDirectorySize + commentLength == tail.size()) {
      eocdOffset = offset;
      break;
    }
  }
  if (!eocdOffset) {
    error = "PWSZ ZIP end directory signature is missing";
    return false;
  }

  const auto* eocd = tail.data() + *eocdOffset;
  const auto diskNumber = readLe16(eocd + 4);
  const auto centralDisk = readLe16(eocd + 6);
  const auto entriesOnDisk = readLe16(eocd + 8);
  const auto totalEntries = readLe16(eocd + 10);
  const auto centralSize = readLe32(eocd + 12);
  const auto centralOffset = readLe32(eocd + 16);

  if (diskNumber != 0 || centralDisk != 0 || entriesOnDisk != totalEntries) {
    error = "multi-disk PWSZ ZIP archives are not supported";
    return false;
  }
  if (totalEntries == std::numeric_limits<std::uint16_t>::max()
      || centralSize == std::numeric_limits<std::uint32_t>::max()
      || centralOffset == std::numeric_limits<std::uint32_t>::max()) {
    error = "ZIP64 PWSZ archives are not supported";
    return false;
  }
  if (static_cast<std::uint64_t>(centralOffset) + centralSize > fileSize) {
    error = "PWSZ ZIP central directory is outside the archive";
    return false;
  }

  std::vector<std::uint8_t> central(centralSize);
  input.clear();
  input.seekg(static_cast<std::streamoff>(centralOffset), std::ios::beg);
  if (!readExact(input, central.data(), central.size())) {
    error = "PWSZ ZIP central directory cannot be read";
    return false;
  }

  std::size_t cursor = 0;
  for (std::uint16_t index = 0; index < totalEntries; ++index) {
    if (cursor + kCentralHeaderSize > central.size()
        || readLe32(central.data() + cursor) != kCentralHeaderSignature) {
      error = "PWSZ ZIP central directory contains a malformed entry";
      return false;
    }

    const auto* fixed = central.data() + cursor;
    const auto nameLength = readLe16(fixed + 28);
    const auto extraLength = readLe16(fixed + 30);
    const auto commentLength = readLe16(fixed + 32);
    const std::size_t entrySize = kCentralHeaderSize + nameLength + extraLength
                                  + commentLength;
    if (cursor + entrySize > central.size()) {
      error = "PWSZ ZIP central directory entry is truncated";
      return false;
    }

    ZipEntry entry;
    entry.flags = readLe16(fixed + 8);
    entry.compressionMethod = readLe16(fixed + 10);
    entry.crc32 = readLe32(fixed + 16);
    entry.compressedSize = readLe32(fixed + 20);
    entry.uncompressedSize = readLe32(fixed + 24);
    entry.localHeaderOffset = readLe32(fixed + 42);
    entry.name.assign(
        reinterpret_cast<const char*>(fixed + kCentralHeaderSize),
        nameLength);

    if (entry.name.empty() || entry.name.front() == '/'
        || entry.name.find("..") != std::string::npos) {
      error = "PWSZ ZIP contains an unsafe entry path";
      return false;
    }
    if ((entry.flags & 0x1u) != 0u) {
      error = "encrypted PWSZ ZIP entries are not supported";
      return false;
    }
    if (entry.compressionMethod != 0u && entry.compressionMethod != 8u) {
      error = "PWSZ ZIP uses an unsupported compression method";
      return false;
    }
    if (!entries_.emplace(entry.name, std::move(entry)).second) {
      error = "PWSZ ZIP contains duplicate entry names";
      return false;
    }
    cursor += entrySize;
  }

  return true;
}

std::optional<std::vector<std::uint8_t>> PwszArchiveReader::readEntry(
    const std::string& name,
    std::size_t maximumSize,
    std::string& error) const {
  const auto found = entries_.find(name);
  if (found == entries_.end()) {
    error = "PWSZ ZIP entry is missing: " + name;
    return std::nullopt;
  }
  const ZipEntry& entry = found->second;
  if (entry.uncompressedSize > maximumSize) {
    error = "PWSZ ZIP entry exceeds its allowed size: " + name;
    return std::nullopt;
  }

  std::ifstream input(archivePath_, std::ios::binary);
  if (!input) {
    error = "PWSZ archive cannot be reopened";
    return std::nullopt;
  }

  std::array<std::uint8_t, kLocalHeaderSize> local{};
  input.seekg(static_cast<std::streamoff>(entry.localHeaderOffset), std::ios::beg);
  if (!readExact(input, local.data(), local.size())
      || readLe32(local.data()) != kLocalHeaderSignature) {
    error = "PWSZ ZIP local entry header is invalid: " + name;
    return std::nullopt;
  }

  const auto localMethod = readLe16(local.data() + 8);
  const auto nameLength = readLe16(local.data() + 26);
  const auto extraLength = readLe16(local.data() + 28);
  if (localMethod != entry.compressionMethod) {
    error = "PWSZ ZIP local and central compression methods disagree";
    return std::nullopt;
  }

  const std::uint64_t dataOffset = static_cast<std::uint64_t>(entry.localHeaderOffset)
                                   + kLocalHeaderSize + nameLength + extraLength;
  input.seekg(static_cast<std::streamoff>(dataOffset), std::ios::beg);
  std::vector<std::uint8_t> compressed(entry.compressedSize);
  if (!readExact(input, compressed.data(), compressed.size())) {
    error = "PWSZ ZIP entry data is truncated: " + name;
    return std::nullopt;
  }

  std::vector<std::uint8_t> output(entry.uncompressedSize);
  if (entry.compressionMethod == 0u) {
    if (entry.compressedSize != entry.uncompressedSize) {
      error = "stored PWSZ ZIP entry has inconsistent sizes: " + name;
      return std::nullopt;
    }
    output = std::move(compressed);
  } else {
    z_stream stream{};
    stream.next_in = compressed.empty() ? nullptr : compressed.data();
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = output.empty() ? nullptr : output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
      error = "zlib could not initialize raw Deflate for PWSZ";
      return std::nullopt;
    }
    const int status = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.total_out != output.size()) {
      error = "PWSZ ZIP Deflate stream is invalid: " + name;
      return std::nullopt;
    }
  }

  const auto actualCrc = static_cast<std::uint32_t>(
      crc32(0L, output.empty() ? Z_NULL : output.data(), static_cast<uInt>(output.size())));
  if (actualCrc != entry.crc32) {
    error = "PWSZ ZIP entry CRC mismatch: " + name;
    return std::nullopt;
  }
  return output;
}

bool PwszArchiveReader::readMetadata(std::string& error) {
  const auto machineBytes = readEntry(
      "anycubic_photon_resins.pwsp",
      kMaximumMetadataSize,
      error);
  if (!machineBytes) {
    return false;
  }
  const auto machineJson = parseJson(*machineBytes, "anycubic_photon_resins.pwsp", error);
  if (!machineJson) {
    return false;
  }

  const auto machineType = machineJson->find("machine_type");
  if (machineType == machineJson->end() || !machineType->is_object()) {
    error = "PWSZ machine metadata does not contain machine_type";
    return false;
  }

  meta_.resolutionX = uintAt(*machineType, "res_x");
  meta_.resolutionY = uintAt(*machineType, "res_y");
  if (!meta_.resolutionX || !meta_.resolutionY
      || *meta_.resolutionX == 0 || *meta_.resolutionY == 0) {
    error = "PWSZ machine metadata has invalid raster dimensions";
    return false;
  }

  if (const auto pitchXUm = numberAt(*machineType, "xy_pixel")) {
    meta_.pitchXMm = *pitchXUm / 1000.0;
    meta_.pitchXYMm = meta_.pitchXMm;
  }
  if (const auto pitchYUm = numberAt(*machineType, "xy_pixel_y")) {
    meta_.pitchYMm = *pitchYUm / 1000.0;
  } else {
    meta_.pitchYMm = meta_.pitchXMm;
  }
  if (const auto antialiasing = uintAt(*machineType, "raster_antialiasing")) {
    meta_.antiAliasingLevel = *antialiasing;
  }
  const auto machineName = machineType->find("name");
  if (machineName != machineType->end() && machineName->is_string()) {
    meta_.machineName = machineName->get<std::string>();
  }

  const auto controllerBytes = readEntry(
      "layers_controller.conf",
      kMaximumMetadataSize,
      error);
  if (!controllerBytes) {
    return false;
  }
  const auto controller = parseJson(*controllerBytes, "layers_controller.conf", error);
  if (!controller) {
    return false;
  }
  meta_.layerCount = uintAt(*controller, "count");
  const auto paras = controller->find("paras");
  if (!meta_.layerCount || paras == controller->end() || !paras->is_array()) {
    error = "PWSZ layer controller has no valid count/paras array";
    return false;
  }

  for (const auto& parameter : *paras) {
    if (!parameter.is_object()) {
      continue;
    }
    const auto thickness = numberAt(parameter, "layer_thickness");
    if (thickness && *thickness > 0.0) {
      meta_.pitchZMm = *thickness;
      break;
    }
  }
  if (!meta_.pitchZMm) {
    error = "PWSZ layer controller has no positive layer thickness";
    return false;
  }

  return true;
}

bool PwszArchiveReader::buildLayerIndex(std::string& error) {
  static const std::regex layerPattern(
      R"(^layer_images/layer_([0-9]+)\.pw0Img$)",
      std::regex::icase);

  std::map<std::size_t, const ZipEntry*> layerEntries;
  for (const auto& [name, entry] : entries_) {
    std::smatch match;
    if (!std::regex_match(name, match, layerPattern)) {
      continue;
    }
    try {
      const auto layerNumber = static_cast<std::size_t>(std::stoull(match[1].str()));
      if (!layerEntries.emplace(layerNumber, &entry).second) {
        error = "PWSZ contains duplicate numeric layer identifiers";
        return false;
      }
    } catch (const std::exception&) {
      error = "PWSZ layer filename contains an invalid numeric identifier";
      return false;
    }
  }

  if (layerEntries.empty()) {
    error = "PWSZ does not contain pw0Img layer entries";
    return false;
  }
  if (!meta_.layerCount || layerEntries.size() != *meta_.layerCount) {
    error = "PWSZ layer count does not match layers_controller.conf";
    return false;
  }

  const auto controllerBytes = readEntry(
      "layers_controller.conf",
      kMaximumMetadataSize,
      error);
  if (!controllerBytes) {
    return false;
  }
  const auto controller = parseJson(*controllerBytes, "layers_controller.conf", error);
  if (!controller) {
    return false;
  }

  std::map<std::size_t, std::pair<double, double>> layerGeometry;
  for (const auto& parameter : controller->at("paras")) {
    if (!parameter.is_object()) {
      continue;
    }
    const auto layerNumber = uintAt(parameter, "layer_index");
    const auto z = numberAt(parameter, "layer_minheight");
    const auto thickness = numberAt(parameter, "layer_thickness");
    if (layerNumber && z && thickness) {
      layerGeometry[*layerNumber] = {*z, *thickness};
    }
  }

  layerIndex_.reserve(layerEntries.size());
  for (std::size_t expected = 0; expected < layerEntries.size(); ++expected) {
    const auto found = layerEntries.find(expected);
    if (found == layerEntries.end()) {
      error = "PWSZ layer numbering is not contiguous from zero";
      return false;
    }
    const auto geometry = layerGeometry.find(expected);
    if (geometry == layerGeometry.end()) {
      error = "PWSZ layer controller is missing parameters for a layer";
      return false;
    }

    const ZipEntry& entry = *found->second;
    LayerIndex index;
    index.layerNumber = expected;
    index.storageMember = entry.name;
    index.fileOffset = entry.localHeaderOffset;
    index.byteLength = entry.compressedSize;
    index.uncompressedByteLength = entry.uncompressedSize;
    index.zHeightMm = geometry->second.first;
    index.thicknessMm = geometry->second.second;
    layerIndex_.push_back(std::move(index));
  }
  return true;
}

std::optional<LayerSlice> PwszArchiveReader::decodeLayer(
    std::size_t layerNumber,
    bool retainGray,
    std::string& error) const {
  if (layerNumber >= layerIndex_.size()) {
    error = "PWSZ layer number is outside the document";
    return std::nullopt;
  }
  const auto encoded = readEntry(
      layerIndex_[layerNumber].storageMember,
      kMaximumLayerStreamSize,
      error);
  if (!encoded) {
    return std::nullopt;
  }

  const auto decoded = pw0::decode(
      std::span<const std::uint8_t>(*encoded),
      width(),
      height(),
      pw0::DecodeOptions{retainGray});
  if (!decoded.ok) {
    error = "PWSZ layer " + std::to_string(layerNumber) + ": " + decoded.error;
    return std::nullopt;
  }
  return decoded.layer;
}

std::uint32_t PwszArchiveReader::width() const noexcept {
  return meta_.resolutionX.value_or(0);
}

std::uint32_t PwszArchiveReader::height() const noexcept {
  return meta_.resolutionY.value_or(0);
}

std::optional<BinaryMask> PwszArchiveReader::loadMask(
    std::size_t layerNumber,
    std::string& error) {
  auto layer = decodeLayer(layerNumber, false, error);
  if (!layer || !layer->maskTruth) {
    if (error.empty()) {
      error = "PWSZ layer decoder did not return a material mask";
    }
    return std::nullopt;
  }
  return std::move(*layer->maskTruth);
}

} // namespace accloud::photons::pwsz
