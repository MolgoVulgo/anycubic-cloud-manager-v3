#include "infra/photons/drivers/pwsz/PwszArchiveReader.h"

#include <zlib.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct EntryData {
  std::string name;
  std::vector<std::uint8_t> data;
  bool deflate = false;
};

struct WrittenEntry {
  std::string name;
  std::uint16_t method = 0;
  std::uint32_t crc = 0;
  std::uint32_t compressedSize = 0;
  std::uint32_t uncompressedSize = 0;
  std::uint32_t localOffset = 0;
};

void writeLe16(std::ostream& output, std::uint16_t value) {
  const std::array<char, 2> bytes{
      static_cast<char>(value & 0xffu),
      static_cast<char>((value >> 8u) & 0xffu),
  };
  output.write(bytes.data(), bytes.size());
}

void writeLe32(std::ostream& output, std::uint32_t value) {
  const std::array<char, 4> bytes{
      static_cast<char>(value & 0xffu),
      static_cast<char>((value >> 8u) & 0xffu),
      static_cast<char>((value >> 16u) & 0xffu),
      static_cast<char>((value >> 24u) & 0xffu),
  };
  output.write(bytes.data(), bytes.size());
}

std::vector<std::uint8_t> rawDeflate(std::span<const std::uint8_t> input) {
  z_stream stream{};
  if (deflateInit2(&stream,
                   Z_BEST_SPEED,
                   Z_DEFLATED,
                   -MAX_WBITS,
                   8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    return {};
  }

  std::vector<std::uint8_t> output(compressBound(input.size()));
  stream.next_in = const_cast<Bytef*>(input.data());
  stream.avail_in = static_cast<uInt>(input.size());
  stream.next_out = output.data();
  stream.avail_out = static_cast<uInt>(output.size());
  const int status = deflate(&stream, Z_FINISH);
  if (status != Z_STREAM_END) {
    deflateEnd(&stream);
    return {};
  }
  output.resize(stream.total_out);
  deflateEnd(&stream);
  return output;
}

bool writeZip(const std::filesystem::path& path, const std::vector<EntryData>& entries) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }

  std::vector<WrittenEntry> written;
  for (const auto& source : entries) {
    std::vector<std::uint8_t> compressed = source.deflate
                                               ? rawDeflate(source.data)
                                               : source.data;
    if (source.deflate && compressed.empty() && !source.data.empty()) {
      return false;
    }

    WrittenEntry entry;
    entry.name = source.name;
    entry.method = source.deflate ? 8u : 0u;
    entry.crc = static_cast<std::uint32_t>(
        crc32(0L, source.data.empty() ? Z_NULL : source.data.data(), source.data.size()));
    entry.compressedSize = static_cast<std::uint32_t>(compressed.size());
    entry.uncompressedSize = static_cast<std::uint32_t>(source.data.size());
    entry.localOffset = static_cast<std::uint32_t>(output.tellp());

    writeLe32(output, 0x04034b50u);
    writeLe16(output, 20);
    writeLe16(output, 0);
    writeLe16(output, entry.method);
    writeLe16(output, 0);
    writeLe16(output, 0);
    writeLe32(output, entry.crc);
    writeLe32(output, entry.compressedSize);
    writeLe32(output, entry.uncompressedSize);
    writeLe16(output, static_cast<std::uint16_t>(entry.name.size()));
    writeLe16(output, 0);
    output.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size()));
    output.write(reinterpret_cast<const char*>(compressed.data()),
                 static_cast<std::streamsize>(compressed.size()));
    written.push_back(std::move(entry));
  }

  const auto centralOffset = static_cast<std::uint32_t>(output.tellp());
  for (const auto& entry : written) {
    writeLe32(output, 0x02014b50u);
    writeLe16(output, 20);
    writeLe16(output, 20);
    writeLe16(output, 0);
    writeLe16(output, entry.method);
    writeLe16(output, 0);
    writeLe16(output, 0);
    writeLe32(output, entry.crc);
    writeLe32(output, entry.compressedSize);
    writeLe32(output, entry.uncompressedSize);
    writeLe16(output, static_cast<std::uint16_t>(entry.name.size()));
    writeLe16(output, 0);
    writeLe16(output, 0);
    writeLe16(output, 0);
    writeLe16(output, 0);
    writeLe32(output, 0);
    writeLe32(output, entry.localOffset);
    output.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size()));
  }
  const auto centralEnd = static_cast<std::uint32_t>(output.tellp());

  writeLe32(output, 0x06054b50u);
  writeLe16(output, 0);
  writeLe16(output, 0);
  writeLe16(output, static_cast<std::uint16_t>(written.size()));
  writeLe16(output, static_cast<std::uint16_t>(written.size()));
  writeLe32(output, centralEnd - centralOffset);
  writeLe32(output, centralOffset);
  writeLe16(output, 0);
  return output.good();
}

std::vector<std::uint8_t> bytes(std::string value) {
  return {value.begin(), value.end()};
}

bool require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << '\n';
    return false;
  }
  return true;
}

} // namespace

bool validateExternalArchive(const std::filesystem::path& path, bool allLayers) {
  accloud::photons::pwsz::PwszArchiveReader reader;
  std::string error;
  if (!reader.open(path, error)) {
    std::cerr << path << ": " << error << '\n';
    return false;
  }
  if (reader.layerCount() == 0 || reader.width() == 0 || reader.height() == 0) {
    std::cerr << path << ": invalid document dimensions/count\n";
    return false;
  }

  std::vector<std::size_t> samples;
  if (allLayers) {
    samples.reserve(reader.layerCount());
    for (std::size_t layer = 0; layer < reader.layerCount(); ++layer) {
      samples.push_back(layer);
    }
  } else {
    samples = {0, reader.layerCount() / 2, reader.layerCount() - 1};
  }

  std::size_t antialiasedLayers = 0;
  for (const std::size_t layerNumber : samples) {
    error.clear();
    const auto layer = reader.decodeLayer(layerNumber, false, error);
    if (!layer || !layer->maskTruth
        || layer->maskTruth->width() != reader.width()
        || layer->maskTruth->height() != reader.height()) {
      std::cerr << path << ": layer " << layerNumber << ": " << error << '\n';
      return false;
    }
    if (layer->hasIntermediateGray) {
      ++antialiasedLayers;
    }
  }
  std::cout << path.filename().string() << ": " << samples.size() << '/'
            << reader.layerCount() << " layers, " << reader.width() << 'x'
            << reader.height() << ", antialiased=" << antialiasedLayers
            << " validated\n";
  return true;
}

int main(int argc, char** argv) {
  const auto path = std::filesystem::temp_directory_path() / "accloud-pwsz-reader-test.pwsz";
  std::error_code removeError;
  std::filesystem::remove(path, removeError);

  const std::string machine = R"({
    "machine_type": {
      "name": "Synthetic Photon",
      "res_x": 4,
      "res_y": 3,
      "xy_pixel": 20.0,
      "xy_pixel_y": 25.0,
      "raster_antialiasing": 4
    }
  })";
  const std::string controller = R"({
    "count": 2,
    "paras": [
      {"layer_index": 0, "layer_minheight": 0.0, "layer_thickness": 0.05},
      {"layer_index": 1, "layer_minheight": 0.05, "layer_thickness": 0.05}
    ]
  })";

  const std::vector<EntryData> entries{
      {"anycubic_photon_resins.pwsp", bytes(machine), false},
      {"layers_controller.conf", bytes(controller), true},
      {"layer_images/layer_1.pw0Img", {0x52, 0x00, 0x0a}, true},
      {"layer_images/layer_0.pw0Img", {0x00, 0x06, 0xf0, 0x06}, true},
  };

  bool ok = true;
  ok &= require(writeZip(path, entries), "synthetic PWSZ archive must be created");

  accloud::photons::pwsz::PwszArchiveReader reader;
  std::string error;
  ok &= require(reader.open(path, error), "synthetic PWSZ must open: " + error);
  ok &= require(reader.layerCount() == 2, "PWSZ layer count must match controller metadata");
  ok &= require(reader.width() == 4 && reader.height() == 3,
                "PWSZ raster dimensions must come from machine metadata");
  ok &= require(reader.meta().pitchXMm && *reader.meta().pitchXMm == 0.02,
                "PWSZ X pitch must be converted from micrometres to millimetres");
  ok &= require(reader.meta().pitchYMm && *reader.meta().pitchYMm == 0.025,
                "PWSZ Y pitch must remain independent from X pitch");
  ok &= require(reader.layerIndex().size() == 2
                    && reader.layerIndex()[0].storageMember.find("layer_0") != std::string::npos
                    && reader.layerIndex()[1].storageMember.find("layer_1") != std::string::npos,
                "PWSZ layers must be sorted numerically rather than lexicographically");

  error.clear();
  const auto layer0 = reader.decodeLayer(0, false, error);
  ok &= require(layer0 && layer0->maskTruth && layer0->maskTruth->count() == 6,
                "binary PWSZ layer must decode to the exact material mask: " + error);
  ok &= require(layer0 && !layer0->hasIntermediateGray,
                "binary PWSZ layer must not require antialiasing");

  error.clear();
  const auto layer1 = reader.decodeLayer(1, true, error);
  ok &= require(layer1 && layer1->hasIntermediateGray,
                "intermediate grey must be detected from layer data");
  ok &= require(layer1 && layer1->decodedGray && (*layer1->decodedGray)[0] == 5,
                "intermediate exposure value must be retained on demand");
  ok &= require(layer1 && layer1->maskTruth && layer1->maskTruth->count() == 2,
                "all non-black antialiased pixels must remain material");

  std::filesystem::remove(path, removeError);

  bool allLayers = false;
  int firstArchiveArgument = 1;
  if (argc > 1 && std::string_view(argv[1]) == "--all") {
    allLayers = true;
    firstArchiveArgument = 2;
  }
  for (int argument = firstArchiveArgument; argument < argc; ++argument) {
    ok &= validateExternalArchive(argv[argument], allLayers);
  }
  return ok ? 0 : 1;
}
