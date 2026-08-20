#include "render3d/analysis/SupportAnalysisDiagnostics.h"

#include "domain/photons/BinaryMask.h"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace accloud::render3d {
namespace {

using photons::BinaryMask;

const char* phaseName(PrintPhase phase) {
  switch (phase) {
  case PrintPhase::Raft: return "raft";
  case PrintPhase::SupportsOnly: return "supports_only";
  case PrintPhase::ModelAndSupports: return "model_and_supports";
  case PrintPhase::ModelMostly: return "model_mostly";
  }
  return "unknown";
}

const char* semanticName(MaterialSemantic semantic) {
  switch (semantic) {
  case MaterialSemantic::Model: return "model";
  case MaterialSemantic::Support: return "support";
  case MaterialSemantic::Raft: return "raft";
  }
  return "unknown";
}

const char* nodeKindName(SupportNodeKind kind) {
  switch (kind) {
  case SupportNodeKind::RaftRoot: return "raft_root";
  case SupportNodeKind::Pillar: return "pillar";
  case SupportNodeKind::Branch: return "branch";
  case SupportNodeKind::Brace: return "brace";
  case SupportNodeKind::Head: return "head";
  case SupportNodeKind::Rejected: return "rejected";
  }
  return "unknown";
}

const char* edgeKindName(SupportEdgeKind kind) {
  switch (kind) {
  case SupportEdgeKind::Continuation: return "continuation";
  case SupportEdgeKind::Split: return "split";
  case SupportEdgeKind::Brace: return "brace";
  case SupportEdgeKind::ModelContact: return "model_contact";
  }
  return "unknown";
}

struct Bounds {
  std::uint32_t minX = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t minY = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxX = 0;
  std::uint32_t maxY = 0;

  [[nodiscard]] bool empty() const noexcept {
    return minX == std::numeric_limits<std::uint32_t>::max();
  }
};

Bounds materialBounds(const BinaryMask& material) {
  Bounds bounds;
  for (std::uint32_t y = 0; y < material.height(); ++y) {
    for (std::size_t wordIndex = 0; wordIndex < material.wordsPerRow(); ++wordIndex) {
      std::uint64_t word = material.rowWord(y, wordIndex);
      if (wordIndex + 1u == material.wordsPerRow()
          && (material.width() % 64u) != 0u) {
        word &= (std::uint64_t{1} << (material.width() % 64u)) - 1u;
      }
      if (word == 0u) {
        continue;
      }
      const auto first = static_cast<std::uint32_t>(wordIndex * 64u)
                         + static_cast<std::uint32_t>(std::countr_zero(word));
      const auto last = static_cast<std::uint32_t>(wordIndex * 64u)
                        + 64u - static_cast<std::uint32_t>(std::countl_zero(word));
      bounds.minX = std::min(bounds.minX, first);
      bounds.minY = std::min(bounds.minY, y);
      bounds.maxX = std::max(bounds.maxX, std::min(material.width(), last));
      bounds.maxY = std::max(bounds.maxY, y + 1u);
    }
  }
  return bounds;
}

void appendBigEndian(std::vector<std::uint8_t>& output, std::uint32_t value) {
  output.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
  output.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
  output.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
  output.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendChunk(
    std::vector<std::uint8_t>& png,
    const std::array<char, 4>& type,
    const std::vector<std::uint8_t>& payload) {
  appendBigEndian(png, static_cast<std::uint32_t>(payload.size()));
  const auto typeOffset = png.size();
  png.insert(png.end(), type.begin(), type.end());
  png.insert(png.end(), payload.begin(), payload.end());
  const auto crc = crc32(
      0u,
      reinterpret_cast<const Bytef*>(png.data() + typeOffset),
      static_cast<uInt>(4u + payload.size()));
  appendBigEndian(png, static_cast<std::uint32_t>(crc));
}

bool writeRgbPng(
    const std::filesystem::path& path,
    std::uint32_t width,
    std::uint32_t height,
    const std::vector<std::uint8_t>& rgb,
    std::string& error) {
  if (width == 0u || height == 0u
      || rgb.size() != static_cast<std::size_t>(width) * height * 3u) {
    error = "invalid RGB image buffer";
    return false;
  }

  std::vector<std::uint8_t> scanlines;
  scanlines.resize(static_cast<std::size_t>(height) * (1u + width * 3u));
  for (std::uint32_t y = 0; y < height; ++y) {
    const auto destinationOffset = static_cast<std::size_t>(y) * (1u + width * 3u);
    scanlines[destinationOffset] = 0u;
    std::copy_n(
        rgb.data() + static_cast<std::size_t>(y) * width * 3u,
        static_cast<std::size_t>(width) * 3u,
        scanlines.data() + destinationOffset + 1u);
  }

  uLongf compressedSize = compressBound(static_cast<uLong>(scanlines.size()));
  std::vector<std::uint8_t> compressed(compressedSize);
  const auto status = compress2(
      compressed.data(), &compressedSize,
      scanlines.data(), static_cast<uLong>(scanlines.size()), Z_BEST_SPEED);
  if (status != Z_OK) {
    error = "zlib could not compress diagnostic PNG";
    return false;
  }
  compressed.resize(compressedSize);

  std::vector<std::uint8_t> png = {
      137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u,
  };
  std::vector<std::uint8_t> ihdr;
  appendBigEndian(ihdr, width);
  appendBigEndian(ihdr, height);
  ihdr.insert(ihdr.end(), {8u, 2u, 0u, 0u, 0u});
  appendChunk(png, {'I', 'H', 'D', 'R'}, ihdr);
  appendChunk(png, {'I', 'D', 'A', 'T'}, compressed);
  appendChunk(png, {'I', 'E', 'N', 'D'}, {});

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open diagnostic PNG output";
    return false;
  }
  file.write(reinterpret_cast<const char*>(png.data()),
             static_cast<std::streamsize>(png.size()));
  if (!file) {
    error = "cannot write diagnostic PNG output";
    return false;
  }
  return true;
}

void setPixel(
    std::vector<std::uint8_t>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t x,
    std::uint32_t y,
    const std::array<std::uint8_t, 3>& color) {
  if (x >= width || y >= height) {
    return;
  }
  const auto offset = (static_cast<std::size_t>(y) * width + x) * 3u;
  pixels[offset] = color[0];
  pixels[offset + 1u] = color[1];
  pixels[offset + 2u] = color[2];
}

void drawRect(
    std::vector<std::uint8_t>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t minX,
    std::uint32_t minY,
    std::uint32_t maxX,
    std::uint32_t maxY,
    const std::array<std::uint8_t, 3>& color) {
  if (minX >= maxX || minY >= maxY || minX >= width || minY >= height) {
    return;
  }
  maxX = std::min(maxX, width);
  maxY = std::min(maxY, height);
  for (std::uint32_t x = minX; x < maxX; ++x) {
    setPixel(pixels, width, height, x, minY, color);
    setPixel(pixels, width, height, x, maxY - 1u, color);
  }
  for (std::uint32_t y = minY; y < maxY; ++y) {
    setPixel(pixels, width, height, minX, y, color);
    setPixel(pixels, width, height, maxX - 1u, y, color);
  }
}

void fillPixelRect(
    std::vector<std::uint8_t>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t minX,
    std::uint32_t minY,
    std::uint32_t maxX,
    std::uint32_t maxY,
    const std::array<std::uint8_t, 3>& color) {
  maxX = std::min(maxX, width);
  maxY = std::min(maxY, height);
  for (std::uint32_t y = minY; y < maxY; ++y) {
    for (std::uint32_t x = minX; x < maxX; ++x) {
      setPixel(pixels, width, height, x, y, color);
    }
  }
}

struct LabelRect {
  std::uint32_t minX = 0u;
  std::uint32_t minY = 0u;
  std::uint32_t maxX = 0u;
  std::uint32_t maxY = 0u;
};

bool overlaps(const LabelRect& left, const LabelRect& right) noexcept {
  return left.minX < right.maxX && right.minX < left.maxX
         && left.minY < right.maxY && right.minY < left.maxY;
}

constexpr std::array<std::array<std::uint8_t, 5>, 10> digitGlyphs = {{
    {{0b111u, 0b101u, 0b101u, 0b101u, 0b111u}},
    {{0b010u, 0b110u, 0b010u, 0b010u, 0b111u}},
    {{0b111u, 0b001u, 0b111u, 0b100u, 0b111u}},
    {{0b111u, 0b001u, 0b111u, 0b001u, 0b111u}},
    {{0b101u, 0b101u, 0b111u, 0b001u, 0b001u}},
    {{0b111u, 0b100u, 0b111u, 0b001u, 0b111u}},
    {{0b111u, 0b100u, 0b111u, 0b101u, 0b111u}},
    {{0b111u, 0b001u, 0b010u, 0b010u, 0b010u}},
    {{0b111u, 0b101u, 0b111u, 0b101u, 0b111u}},
    {{0b111u, 0b101u, 0b111u, 0b001u, 0b111u}},
}};

void drawNodeIdLabel(
    std::vector<std::uint8_t>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    const LabelRect& label,
    std::size_t nodeId,
    const std::array<std::uint8_t, 3>& borderColor) {
  constexpr std::array<std::uint8_t, 3> background = {18u, 20u, 25u};
  constexpr std::array<std::uint8_t, 3> textColor = {245u, 247u, 250u};
  fillPixelRect(pixels, width, height, label.minX, label.minY,
                label.maxX, label.maxY, background);
  drawRect(pixels, width, height, label.minX, label.minY,
           label.maxX, label.maxY, borderColor);

  const auto text = std::to_string(nodeId);
  std::uint32_t cursorX = label.minX + 2u;
  const auto originY = label.minY + 2u;
  for (const char character : text) {
    if (character < '0' || character > '9') {
      continue;
    }
    const auto& glyph = digitGlyphs[static_cast<std::size_t>(character - '0')];
    for (std::uint32_t row = 0u; row < glyph.size(); ++row) {
      for (std::uint32_t column = 0u; column < 3u; ++column) {
        if ((glyph[row] & (1u << (2u - column))) != 0u) {
          setPixel(pixels, width, height, cursorX + column,
                   originY + row, textColor);
        }
      }
    }
    cursorX += 4u;
  }
}

bool shouldLabelNode(const SupportDecisionTrace& decision) noexcept {
  if (decision.nodeId == std::numeric_limits<std::size_t>::max()) {
    return false;
  }
  if (decision.decision != MaterialSemantic::Support
      || decision.contactCandidate || decision.contactConfirmed
      || decision.mixedSemanticProjection) {
    return true;
  }
  return decision.reason == SupportDecisionReason::SupportMotionContinuation
         || decision.reason == SupportDecisionReason::SupportFusionContinuation;
}

class DiagnosticDisjointSet {
public:
  std::size_t add() {
    const auto index = parent_.size();
    parent_.push_back(index);
    rank_.push_back(0u);
    return index;
  }

  std::size_t find(std::size_t value) {
    if (parent_[value] != value) {
      parent_[value] = find(parent_[value]);
    }
    return parent_[value];
  }

  void unite(std::size_t left, std::size_t right) {
    left = find(left);
    right = find(right);
    if (left == right) {
      return;
    }
    if (rank_[left] < rank_[right]) {
      std::swap(left, right);
    }
    parent_[right] = left;
    if (rank_[left] == rank_[right]) {
      ++rank_[left];
    }
  }

private:
  std::vector<std::size_t> parent_;
  std::vector<std::uint8_t> rank_;
};

using DiagnosticPixelRun = std::pair<std::uint32_t, std::uint32_t>;

template <typename WordProvider>
std::vector<DiagnosticPixelRun> collectDiagnosticRuns(
    std::uint32_t width,
    std::size_t wordsPerRow,
    WordProvider wordProvider) {
  std::vector<DiagnosticPixelRun> runs;
  for (std::size_t wordIndex = 0; wordIndex < wordsPerRow; ++wordIndex) {
    std::uint64_t word = wordProvider(wordIndex);
    if (wordIndex + 1u == wordsPerRow && (width % 64u) != 0u) {
      word &= (std::uint64_t{1} << (width % 64u)) - 1u;
    }
    while (word != 0u) {
      const auto firstBit = static_cast<std::uint32_t>(std::countr_zero(word));
      const auto shifted = word >> firstBit;
      const auto runLength = static_cast<std::uint32_t>(std::countr_one(shifted));
      const auto first = static_cast<std::uint32_t>(wordIndex * 64u) + firstBit;
      const auto last = std::min<std::uint32_t>(width, first + runLength);
      if (!runs.empty() && runs.back().second == first) {
        runs.back().second = last;
      } else {
        runs.emplace_back(first, last);
      }
      const auto remaining = 64u - firstBit;
      if (runLength >= remaining) {
        word = 0u;
      } else {
        word &= ~(((std::uint64_t{1} << runLength) - 1u) << firstBit);
      }
    }
  }
  return runs;
}

struct DiagnosticRawRun {
  std::uint32_t y = 0u;
  std::uint32_t first = 0u;
  std::uint32_t last = 0u;
  std::size_t label = 0u;
};

struct DiagnosticComponent {
  std::size_t localId = 0u;
  std::vector<SemanticRun> runs;
};

struct DiagnosticSemanticRawRun {
  std::uint32_t y = 0u;
  std::uint32_t first = 0u;
  std::uint32_t last = 0u;
  std::size_t componentId = 0u;
  MaterialSemantic semantic = MaterialSemantic::Model;
  std::size_t label = 0u;
};

struct DiagnosticRegionParent {
  std::string regionId;
  std::string lineageId;
  std::size_t overlapPixels = 0u;
  double currentCoverageRatio = 0.0;
  double parentCoverageRatio = 0.0;
};

struct DiagnosticSemanticRegion {
  std::size_t selectionId = 0u;
  std::size_t componentId = 0u;
  std::size_t nodeId = std::numeric_limits<std::size_t>::max();
  MaterialSemantic semantic = MaterialSemantic::Model;
  std::size_t areaPixels = 0u;
  Bounds bounds;
  std::vector<SemanticRun> runs;
  std::string regionId;
  std::string lineageId;
  std::string derivedFromLineageId;
  std::vector<std::string> mergedFromLineageIds;
  std::vector<DiagnosticRegionParent> parents;
};

std::vector<DiagnosticComponent> describeDiagnosticComponents(
    const BinaryMask& mask) {
  std::vector<DiagnosticRawRun> runs;
  std::vector<std::size_t> previousRow;
  DiagnosticDisjointSet sets;
  for (std::uint32_t y = 0u; y < mask.height(); ++y) {
    const auto rowRuns = collectDiagnosticRuns(
        mask.width(), mask.wordsPerRow(),
        [&](std::size_t wordIndex) { return mask.rowWord(y, wordIndex); });
    std::vector<std::size_t> currentRow;
    currentRow.reserve(rowRuns.size());
    std::size_t previousCursor = 0u;
    for (const auto& [first, last] : rowRuns) {
      const auto label = sets.add();
      const auto runIndex = runs.size();
      runs.push_back(DiagnosticRawRun{y, first, last, label});
      currentRow.push_back(runIndex);

      while (previousCursor < previousRow.size()
             && runs[previousRow[previousCursor]].last <= first) {
        ++previousCursor;
      }
      for (std::size_t cursor = previousCursor; cursor < previousRow.size(); ++cursor) {
        const auto& upper = runs[previousRow[cursor]];
        if (upper.first >= last) {
          break;
        }
        sets.unite(label, upper.label);
      }
    }
    previousRow = std::move(currentRow);
  }

  std::vector<DiagnosticComponent> components;
  std::map<std::size_t, std::size_t> componentIndex;
  for (auto& run : runs) {
    const auto root = sets.find(run.label);
    const auto [iterator, inserted] = componentIndex.try_emplace(
        root, components.size());
    if (inserted) {
      components.push_back(DiagnosticComponent{});
      components.back().localId = iterator->second;
    }
    components[iterator->second].runs.push_back(
        SemanticRun{run.y, run.first, run.last, MaterialSemantic::Model});
  }
  return components;
}

std::vector<DiagnosticSemanticRegion> describeDiagnosticSemanticRegions(
    const BinaryMask& mask,
    const std::vector<DiagnosticComponent>& components,
    const std::vector<SemanticRun>& semanticRuns,
    std::size_t oneBasedLayer,
    const std::vector<const SupportDecisionTrace*>& decisions) {
  std::vector<std::vector<const SemanticRun*>> semanticsByRow(mask.height());
  for (const auto& run : semanticRuns) {
    if (run.y < semanticsByRow.size()) {
      semanticsByRow[run.y].push_back(&run);
    }
  }

  struct ComponentRun {
    std::uint32_t y = 0u;
    std::uint32_t first = 0u;
    std::uint32_t last = 0u;
    std::size_t componentId = 0u;
  };
  std::vector<ComponentRun> componentRuns;
  for (const auto& component : components) {
    for (const auto& run : component.runs) {
      componentRuns.push_back(ComponentRun{
          run.y, run.firstX, run.lastX, component.localId});
    }
  }
  std::sort(componentRuns.begin(), componentRuns.end(), [](const auto& left, const auto& right) {
    if (left.y != right.y) {
      return left.y < right.y;
    }
    if (left.first != right.first) {
      return left.first < right.first;
    }
    return left.last < right.last;
  });

  std::vector<DiagnosticSemanticRawRun> classifiedRuns;
  classifiedRuns.reserve(componentRuns.size() + semanticRuns.size());
  for (const auto& componentRun : componentRuns) {
    auto cursor = componentRun.first;
    const auto& rowSemantics = semanticsByRow[componentRun.y];
    for (const auto* semanticRun : rowSemantics) {
      if (semanticRun == nullptr || semanticRun->lastX <= cursor) {
        continue;
      }
      if (semanticRun->firstX >= componentRun.last) {
        break;
      }
      if (cursor < semanticRun->firstX) {
        classifiedRuns.push_back(DiagnosticSemanticRawRun{
            componentRun.y,
            cursor,
            std::min(componentRun.last, semanticRun->firstX),
            componentRun.componentId,
            MaterialSemantic::Model,
            0u});
      }
      const auto semanticFirst = std::max(cursor, semanticRun->firstX);
      const auto semanticLast = std::min(componentRun.last, semanticRun->lastX);
      if (semanticFirst < semanticLast) {
        classifiedRuns.push_back(DiagnosticSemanticRawRun{
            componentRun.y,
            semanticFirst,
            semanticLast,
            componentRun.componentId,
            semanticRun->semantic,
            0u});
        cursor = semanticLast;
      }
      if (cursor >= componentRun.last) {
        break;
      }
    }
    if (cursor < componentRun.last) {
      classifiedRuns.push_back(DiagnosticSemanticRawRun{
          componentRun.y,
          cursor,
          componentRun.last,
          componentRun.componentId,
          MaterialSemantic::Model,
          0u});
    }
  }

  DiagnosticDisjointSet sets;
  std::vector<std::size_t> previousRow;
  std::vector<std::size_t> currentRow;
  std::uint32_t currentY = std::numeric_limits<std::uint32_t>::max();
  for (std::size_t index = 0u; index < classifiedRuns.size(); ++index) {
    auto& run = classifiedRuns[index];
    if (run.y != currentY) {
      if (currentY != std::numeric_limits<std::uint32_t>::max()) {
        previousRow = std::move(currentRow);
        currentRow.clear();
      }
      if (currentY == std::numeric_limits<std::uint32_t>::max()
          || run.y != currentY + 1u) {
        previousRow.clear();
      }
      currentY = run.y;
    }
    run.label = sets.add();
    currentRow.push_back(index);
    for (const auto previousIndex : previousRow) {
      const auto& previous = classifiedRuns[previousIndex];
      if (previous.last <= run.first || previous.first >= run.last) {
        continue;
      }
      if (previous.componentId == run.componentId
          && previous.semantic == run.semantic) {
        sets.unite(run.label, previous.label);
      }
    }
  }

  std::unordered_map<std::size_t, std::size_t> nodeByComponent;
  for (const auto* decision : decisions) {
    if (decision != nullptr
        && decision->nodeId != std::numeric_limits<std::size_t>::max()) {
      nodeByComponent[decision->componentId] = decision->nodeId;
    }
  }

  std::vector<DiagnosticSemanticRegion> regions;
  std::unordered_map<std::size_t, std::size_t> regionByRoot;
  for (auto& run : classifiedRuns) {
    const auto root = sets.find(run.label);
    auto iterator = regionByRoot.find(root);
    if (iterator == regionByRoot.end()) {
      const auto regionIndex = regions.size();
      iterator = regionByRoot.emplace(root, regionIndex).first;
      DiagnosticSemanticRegion region;
      region.selectionId = regionIndex + 1u;
      region.componentId = run.componentId;
      region.semantic = run.semantic;
      const auto node = nodeByComponent.find(run.componentId);
      if (node != nodeByComponent.end()) {
        region.nodeId = node->second;
      }
      regions.push_back(std::move(region));
    }
    auto& region = regions[iterator->second];
    const auto length = static_cast<std::size_t>(run.last - run.first);
    region.areaPixels += length;
    region.bounds.minX = std::min(region.bounds.minX, run.first);
    region.bounds.minY = std::min(region.bounds.minY, run.y);
    region.bounds.maxX = std::max(region.bounds.maxX, run.last);
    region.bounds.maxY = std::max(region.bounds.maxY, run.y + 1u);
    region.runs.push_back(SemanticRun{
        run.y, run.first, run.last, run.semantic});
  }

  std::map<std::pair<std::size_t, MaterialSemantic>, std::size_t> sequenceByOwner;
  for (auto& region : regions) {
    auto& sequence = sequenceByOwner[{region.componentId, region.semantic}];
    ++sequence;
    const char semanticPrefix = region.semantic == MaterialSemantic::Support
                                    ? 'S'
                                    : region.semantic == MaterialSemantic::Raft
                                          ? 'R'
                                          : 'M';
    std::ostringstream identifier;
    identifier << 'L' << std::setw(6) << std::setfill('0') << oneBasedLayer << '-';
    if (region.nodeId != std::numeric_limits<std::size_t>::max()) {
      identifier << 'N' << region.nodeId;
    } else {
      identifier << 'C' << region.componentId;
    }
    identifier << '-' << semanticPrefix << std::setw(3) << std::setfill('0')
               << sequence;
    region.regionId = identifier.str();
  }
  return regions;
}

std::size_t regionOverlapPixels(
    const DiagnosticSemanticRegion& left,
    const DiagnosticSemanticRegion& right) {
  if (left.semantic != right.semantic
      || left.bounds.maxX <= right.bounds.minX
      || right.bounds.maxX <= left.bounds.minX
      || left.bounds.maxY <= right.bounds.minY
      || right.bounds.maxY <= left.bounds.minY) {
    return 0u;
  }
  std::size_t overlap = 0u;
  std::size_t leftIndex = 0u;
  std::size_t rightIndex = 0u;
  while (leftIndex < left.runs.size() && rightIndex < right.runs.size()) {
    const auto& leftRun = left.runs[leftIndex];
    const auto& rightRun = right.runs[rightIndex];
    if (leftRun.y < rightRun.y) {
      ++leftIndex;
      continue;
    }
    if (rightRun.y < leftRun.y) {
      ++rightIndex;
      continue;
    }
    const auto first = std::max(leftRun.firstX, rightRun.firstX);
    const auto last = std::min(leftRun.lastX, rightRun.lastX);
    if (first < last) {
      overlap += static_cast<std::size_t>(last - first);
    }
    if (leftRun.lastX < rightRun.lastX) {
      ++leftIndex;
    } else if (rightRun.lastX < leftRun.lastX) {
      ++rightIndex;
    } else {
      ++leftIndex;
      ++rightIndex;
    }
  }
  return overlap;
}

std::string newDiagnosticLineageId(
    MaterialSemantic semantic,
    std::array<std::size_t, 3>& counters) {
  const auto index = semantic == MaterialSemantic::Model
                         ? 0u
                         : semantic == MaterialSemantic::Support ? 1u : 2u;
  const char prefix = semantic == MaterialSemantic::Support
                          ? 'S'
                          : semantic == MaterialSemantic::Raft ? 'R' : 'M';
  std::ostringstream identifier;
  identifier << prefix << std::setw(6) << std::setfill('0') << ++counters[index];
  return identifier.str();
}

void assignDiagnosticLineages(
    std::vector<DiagnosticSemanticRegion>& current,
    const std::vector<DiagnosticSemanticRegion>& previous,
    std::array<std::size_t, 3>& lineageCounters) {
  struct BestParent {
    std::size_t previousIndex = std::numeric_limits<std::size_t>::max();
    std::size_t overlapPixels = 0u;
  };
  std::vector<BestParent> bestParents(current.size());
  for (std::size_t currentIndex = 0u; currentIndex < current.size(); ++currentIndex) {
    auto& region = current[currentIndex];
    for (std::size_t previousIndex = 0u; previousIndex < previous.size(); ++previousIndex) {
      const auto overlap = regionOverlapPixels(region, previous[previousIndex]);
      if (overlap == 0u) {
        continue;
      }
      const auto& parent = previous[previousIndex];
      region.parents.push_back(DiagnosticRegionParent{
          parent.regionId,
          parent.lineageId,
          overlap,
          region.areaPixels == 0u
              ? 0.0
              : static_cast<double>(overlap) / region.areaPixels,
          parent.areaPixels == 0u
              ? 0.0
              : static_cast<double>(overlap) / parent.areaPixels});
      if (overlap > bestParents[currentIndex].overlapPixels) {
        bestParents[currentIndex] = BestParent{previousIndex, overlap};
      }
    }
    std::sort(region.parents.begin(), region.parents.end(), [](const auto& left, const auto& right) {
      if (left.overlapPixels != right.overlapPixels) {
        return left.overlapPixels > right.overlapPixels;
      }
      return left.regionId < right.regionId;
    });
  }

  std::unordered_map<std::size_t, std::vector<std::size_t>> childrenByParent;
  for (std::size_t currentIndex = 0u; currentIndex < current.size(); ++currentIndex) {
    if (bestParents[currentIndex].previousIndex
        != std::numeric_limits<std::size_t>::max()) {
      childrenByParent[bestParents[currentIndex].previousIndex].push_back(currentIndex);
    }
  }

  std::vector<bool> inheritsParent(current.size(), false);
  for (auto& [previousIndex, children] : childrenByParent) {
    const auto winner = *std::max_element(
        children.begin(), children.end(), [&](std::size_t left, std::size_t right) {
          if (bestParents[left].overlapPixels != bestParents[right].overlapPixels) {
            return bestParents[left].overlapPixels < bestParents[right].overlapPixels;
          }
          return current[left].regionId > current[right].regionId;
        });
    inheritsParent[winner] = true;
    (void)previousIndex;
  }

  for (std::size_t currentIndex = 0u; currentIndex < current.size(); ++currentIndex) {
    auto& region = current[currentIndex];
    const auto bestParentIndex = bestParents[currentIndex].previousIndex;
    if (bestParentIndex == std::numeric_limits<std::size_t>::max()) {
      region.lineageId = newDiagnosticLineageId(region.semantic, lineageCounters);
      continue;
    }
    const auto& bestParent = previous[bestParentIndex];
    if (inheritsParent[currentIndex]) {
      region.lineageId = bestParent.lineageId;
    } else {
      region.lineageId = newDiagnosticLineageId(region.semantic, lineageCounters);
      region.derivedFromLineageId = bestParent.lineageId;
    }
    for (const auto& relation : region.parents) {
      if (!relation.lineageId.empty() && relation.lineageId != region.lineageId
          && relation.lineageId != region.derivedFromLineageId
          && std::find(region.mergedFromLineageIds.begin(),
                       region.mergedFromLineageIds.end(), relation.lineageId)
                 == region.mergedFromLineageIds.end()) {
        region.mergedFromLineageIds.push_back(relation.lineageId);
      }
    }
  }
}

std::array<std::uint8_t, 3> pickColor(std::size_t componentId) {
  const auto value = static_cast<std::uint32_t>(componentId + 1u);
  return {
      static_cast<std::uint8_t>((value >> 16u) & 0xffu),
      static_cast<std::uint8_t>((value >> 8u) & 0xffu),
      static_cast<std::uint8_t>(value & 0xffu),
  };
}

struct LayerDiagnosticMetadata {
  Bounds sourceBounds;
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  std::uint32_t downsample = 1u;
  std::vector<DiagnosticSemanticRegion> semanticRegions;
};

bool writeLayerDiagnostics(
    photons::LayerMaskSource& source,
    const SupportAnalyzer& analyzer,
    const SupportAnalysisResult& result,
    std::size_t layer,
    std::uint32_t downsample,
    const std::vector<const SupportDecisionTrace*>& decisions,
    const std::filesystem::path& rawPath,
    const std::filesystem::path& semanticPath,
    const std::filesystem::path& nodesPath,
    const std::filesystem::path& pickPath,
    const std::filesystem::path& regionPickPath,
    LayerDiagnosticMetadata& metadata,
    std::string& error) {
  auto material = source.loadMask(layer, error);
  if (!material) {
    return false;
  }
  std::vector<SemanticRun> semanticRuns;
  if (!analyzer.materializeLayerSemantics(
          *material, result.layers[layer], semanticRuns, error)) {
    return false;
  }

  auto bounds = materialBounds(*material);
  if (bounds.empty()) {
    bounds = Bounds{0u, 0u, std::max(1u, material->width()),
                    std::max(1u, material->height())};
  }
  const std::uint32_t margin = 8u * downsample;
  bounds.minX = bounds.minX > margin ? bounds.minX - margin : 0u;
  bounds.minY = bounds.minY > margin ? bounds.minY - margin : 0u;
  bounds.maxX = std::min(material->width(), bounds.maxX + margin);
  bounds.maxY = std::min(material->height(), bounds.maxY + margin);
  const auto outputWidth = std::max(
      1u, (bounds.maxX - bounds.minX + downsample - 1u) / downsample);
  const auto outputHeight = std::max(
      1u, (bounds.maxY - bounds.minY + downsample - 1u) / downsample);
  metadata.sourceBounds = bounds;
  metadata.width = outputWidth;
  metadata.height = outputHeight;
  metadata.downsample = downsample;

  std::vector<std::uint8_t> rawPixels(
      static_cast<std::size_t>(outputWidth) * outputHeight * 3u, 10u);
  std::vector<std::uint8_t> semanticPixels = rawPixels;
  std::vector<std::uint8_t> nodePixels = rawPixels;
  std::vector<std::uint8_t> pickPixels(
      static_cast<std::size_t>(outputWidth) * outputHeight * 3u, 0u);
  std::vector<std::uint8_t> regionPickPixels(
      static_cast<std::size_t>(outputWidth) * outputHeight * 3u, 0u);

  constexpr std::array<std::uint8_t, 3> rawColor = {205u, 210u, 216u};
  constexpr std::array<std::uint8_t, 3> modelColor = {220u, 96u, 79u};
  constexpr std::array<std::uint8_t, 3> supportColor = {80u, 184u, 198u};
  constexpr std::array<std::uint8_t, 3> raftColor = {239u, 166u, 55u};

  const auto paintBlock = [&](std::vector<std::uint8_t>& pixels,
                              std::uint32_t sourceY,
                              std::uint32_t firstX,
                              std::uint32_t lastX,
                              const std::array<std::uint8_t, 3>& color) {
    if (sourceY < bounds.minY || sourceY >= bounds.maxY
        || lastX <= bounds.minX || firstX >= bounds.maxX) {
      return;
    }
    const auto clippedFirst = std::max(firstX, bounds.minX);
    const auto clippedLast = std::min(lastX, bounds.maxX);
    if (clippedFirst >= clippedLast) {
      return;
    }
    const auto firstBlock = (clippedFirst - bounds.minX) / downsample;
    const auto lastBlock = (clippedLast - 1u - bounds.minX) / downsample;
    const auto outputY = (sourceY - bounds.minY) / downsample;
    for (std::uint32_t x = firstBlock; x <= lastBlock; ++x) {
      setPixel(pixels, outputWidth, outputHeight, x, outputY, color);
    }
  };

  const auto components = describeDiagnosticComponents(*material);
  metadata.semanticRegions = describeDiagnosticSemanticRegions(
      *material, components, semanticRuns, layer + 1u, decisions);
  for (const auto& component : components) {
    if (component.localId >= 0x00ffffffu) {
      error = "too many components for diagnostic pick map";
      return false;
    }
    const auto encoded = pickColor(component.localId);
    for (const auto& run : component.runs) {
      paintBlock(rawPixels, run.y, run.firstX, run.lastX, rawColor);
      paintBlock(semanticPixels, run.y, run.firstX, run.lastX, modelColor);
      paintBlock(nodePixels, run.y, run.firstX, run.lastX, modelColor);
      paintBlock(pickPixels, run.y, run.firstX, run.lastX, encoded);
    }
  }

  for (const auto& run : semanticRuns) {
    const auto color = run.semantic == MaterialSemantic::Raft
                           ? raftColor
                           : supportColor;
    paintBlock(semanticPixels, run.y, run.firstX, run.lastX, color);
    paintBlock(nodePixels, run.y, run.firstX, run.lastX, color);
  }

  const auto paintRegionSelection = [&](MaterialSemantic semantic) {
    for (const auto& region : metadata.semanticRegions) {
      if (region.semantic != semantic || region.selectionId == 0u
          || region.selectionId > 0x00ffffffu) {
        continue;
      }
      const auto encoded = pickColor(region.selectionId - 1u);
      for (const auto& run : region.runs) {
        paintBlock(regionPickPixels, run.y, run.firstX, run.lastX, encoded);
      }
    }
  };
  // Match semantic-result visibility at diagnostic resolution: model is the
  // base material, then support/raft overlays win when a downsampled block
  // contains multiple native semantics.
  paintRegionSelection(MaterialSemantic::Model);
  paintRegionSelection(MaterialSemantic::Support);
  paintRegionSelection(MaterialSemantic::Raft);

  std::vector<std::pair<const SupportDecisionTrace*, std::array<std::uint8_t, 3>>>
      labelledDecisions;
  const auto convertX = [&](std::uint32_t sourceX) {
    if (sourceX <= bounds.minX) {
      return 0u;
    }
    return std::min(outputWidth, (sourceX - bounds.minX + downsample - 1u)
                                     / downsample);
  };
  const auto convertY = [&](std::uint32_t sourceY) {
    if (sourceY <= bounds.minY) {
      return 0u;
    }
    return std::min(outputHeight, (sourceY - bounds.minY + downsample - 1u)
                                      / downsample);
  };
  for (const auto* decision : decisions) {
    if (decision == nullptr) {
      continue;
    }
    std::array<std::uint8_t, 3> color = decision->decision == MaterialSemantic::Model
        ? std::array<std::uint8_t, 3>{255u, 80u, 80u}
        : decision->decision == MaterialSemantic::Raft
              ? std::array<std::uint8_t, 3>{255u, 210u, 60u}
              : std::array<std::uint8_t, 3>{80u, 230u, 245u};
    if (decision->contactConfirmed) {
      color = {255u, 70u, 230u};
    } else if (decision->contactCandidate) {
      color = {255u, 235u, 80u};
    } else if (decision->mixedSemanticProjection) {
      color = {180u, 100u, 255u};
    }
    drawRect(nodePixels, outputWidth, outputHeight,
             convertX(decision->minX), convertY(decision->minY),
             std::max(convertX(decision->maxX), convertX(decision->minX) + 1u),
             std::max(convertY(decision->maxY), convertY(decision->minY) + 1u),
             color);
    if (shouldLabelNode(*decision)) {
      labelledDecisions.emplace_back(decision, color);
    }
  }

  std::vector<LabelRect> occupiedLabels;
  for (const auto& [decision, color] : labelledDecisions) {
    const auto text = std::to_string(decision->nodeId);
    const auto labelWidth = static_cast<std::uint32_t>(text.size() * 4u + 3u);
    constexpr std::uint32_t labelHeight = 9u;
    if (labelWidth > outputWidth || labelHeight > outputHeight) {
      continue;
    }
    const auto minX = decision->minX <= bounds.minX
                          ? 0u
                          : std::min(outputWidth - 1u,
                                     (decision->minX - bounds.minX) / downsample);
    const auto maxX = decision->maxX <= bounds.minX
                          ? minX + 1u
                          : std::min(outputWidth,
                                     (decision->maxX - bounds.minX + downsample - 1u)
                                         / downsample);
    const auto minY = decision->minY <= bounds.minY
                          ? 0u
                          : std::min(outputHeight - 1u,
                                     (decision->minY - bounds.minY) / downsample);
    const auto maxY = decision->maxY <= bounds.minY
                          ? minY + 1u
                          : std::min(outputHeight,
                                     (decision->maxY - bounds.minY + downsample - 1u)
                                         / downsample);

    const auto clampX = [&](std::int64_t value) {
      return static_cast<std::uint32_t>(std::clamp<std::int64_t>(
          value, 0, static_cast<std::int64_t>(outputWidth - labelWidth)));
    };
    const auto clampY = [&](std::int64_t value) {
      return static_cast<std::uint32_t>(std::clamp<std::int64_t>(
          value, 0, static_cast<std::int64_t>(outputHeight - labelHeight)));
    };
    const auto centeredX = clampX(
        static_cast<std::int64_t>(minX + maxX) / 2
        - static_cast<std::int64_t>(labelWidth) / 2);
    const auto centeredY = clampY(
        static_cast<std::int64_t>(minY + maxY) / 2
        - static_cast<std::int64_t>(labelHeight) / 2);
    const std::array<LabelRect, 5> candidates = {{
        {centeredX, centeredY, centeredX + labelWidth, centeredY + labelHeight},
        {clampX(minX), clampY(static_cast<std::int64_t>(minY) - labelHeight - 1),
         clampX(minX) + labelWidth,
         clampY(static_cast<std::int64_t>(minY) - labelHeight - 1) + labelHeight},
        {clampX(minX), clampY(maxY + 1u),
         clampX(minX) + labelWidth, clampY(maxY + 1u) + labelHeight},
        {clampX(maxX + 1u), clampY(minY),
         clampX(maxX + 1u) + labelWidth, clampY(minY) + labelHeight},
        {clampX(static_cast<std::int64_t>(minX) - labelWidth - 1), clampY(minY),
         clampX(static_cast<std::int64_t>(minX) - labelWidth - 1) + labelWidth,
         clampY(minY) + labelHeight},
    }};

    auto selected = candidates.front();
    bool found = false;
    for (const auto& candidate : candidates) {
      const auto collision = std::any_of(
          occupiedLabels.begin(), occupiedLabels.end(),
          [&](const LabelRect& occupied) { return overlaps(candidate, occupied); });
      if (!collision) {
        selected = candidate;
        found = true;
        break;
      }
    }
    if (!found && decision->decision == MaterialSemantic::Support
        && !decision->contactCandidate && !decision->contactConfirmed
        && !decision->mixedSemanticProjection) {
      continue;
    }
    occupiedLabels.push_back(selected);
    drawNodeIdLabel(nodePixels, outputWidth, outputHeight, selected,
                    decision->nodeId, color);
  }

  return writeRgbPng(rawPath, outputWidth, outputHeight, rawPixels, error)
         && writeRgbPng(semanticPath, outputWidth, outputHeight,
                        semanticPixels, error)
         && writeRgbPng(nodesPath, outputWidth, outputHeight, nodePixels, error)
         && writeRgbPng(pickPath, outputWidth, outputHeight, pickPixels, error)
         && writeRgbPng(regionPickPath, outputWidth, outputHeight,
                        regionPickPixels, error);
}

nlohmann::json decisionJson(const SupportDecisionTrace& decision) {
  const char* supportResolution = "none";
  if (decision.monotonicModelLockedPixels != 0u) {
    supportResolution = decision.finalSupportPixels != 0u
        ? "partially_model_locked"
        : "model_locked";
  } else if (decision.absorbedSupportPixels != 0u) {
    supportResolution = decision.finalSupportPixels != 0u
        ? "partially_absorbed_by_model"
        : "absorbed_by_model";
  } else if (decision.reverseSupportCorePixels != 0u
             && (decision.reverseModelEvidencePixels != 0u
                 || decision.forwardModelCorePixels != 0u)) {
    supportResolution = "support_preserved";
  }
  return {
      {"layer", decision.layer + 1u},
      {"component_id", decision.componentId},
      {"node_id", decision.nodeId == std::numeric_limits<std::size_t>::max()
                      ? nlohmann::json(nullptr)
                      : nlohmann::json(decision.nodeId)},
      {"parent_node_id",
       decision.parentNodeId == std::numeric_limits<std::size_t>::max()
           ? nlohmann::json(nullptr)
           : nlohmann::json(decision.parentNodeId)},
      {"bounds_pixels",
       {{"min_x", decision.minX}, {"min_y", decision.minY},
        {"max_x", decision.maxX}, {"max_y", decision.maxY}}},
      {"surface_comparison",
       {{"current_pixels", decision.currentAreaPixels},
        {"parent_pixels", decision.parentAreaPixels},
        {"recent_support_maximum_pixels",
         decision.recentSupportMaximumAreaPixels},
        {"parent_ratio", decision.parentAreaRatio},
        {"primary_parent_coverage_ratio",
         decision.primaryParentCoverageRatio},
        {"support_fusion_coverage_ratio",
         decision.supportFusionCoverageRatio},
        {"terminal_taper_decrease_steps",
         decision.terminalTaperDecreaseSteps},
        {"pending_start_ratio", decision.pendingStartAreaRatio},
        {"added_pixels_after_alignment", decision.addedPixelsAfterAlignment},
        {"removed_pixels_after_alignment", decision.removedPixelsAfterAlignment},
        {"model_lineage_pixels", decision.modelLineageOverlapPixels},
        {"reverse_model_evidence_pixels", decision.reverseModelEvidencePixels},
        {"forward_model_core_pixels", decision.forwardModelCorePixels},
        {"reverse_support_core_pixels", decision.reverseSupportCorePixels},
        {"direct_absorbed_support_pixels",
         decision.directAbsorbedSupportPixels},
        {"overhang_absorbed_support_pixels",
         decision.overhangAbsorbedSupportPixels},
        {"absorbed_support_pixels", decision.absorbedSupportPixels},
        {"monotonic_model_locked_pixels",
         decision.monotonicModelLockedPixels},
        {"final_support_pixels", decision.finalSupportPixels},
        {"final_model_pixels", decision.finalModelPixels}}},
      {"geometric_comparison",
       {{"overlap_pixels", decision.overlapPixels},
        {"aligned_overlap_pixels", decision.alignedOverlapPixels},
        {"aligned_overlap_ratio", decision.alignedOverlapRatio},
        {"aligned_intersection_over_union",
         decision.alignedIntersectionOverUnion},
        {"centre_distance_pixels", decision.centreDistancePixels},
        {"material_distance_pixels", decision.materialDistancePixels},
        {"matched_support_parent_count",
         decision.matchedSupportParentCount},
        {"preserved_support_parent_count",
         decision.preservedSupportParentCount},
        {"matched_support_parent_node_ids",
         decision.matchedSupportParentNodeIds},
        {"matched_support_parent_overlap_pixels",
         decision.matchedSupportParentOverlapPixels},
        {"predicted_motion_pixels",
         {{"x", decision.predictedMotionXPixels},
          {"y", decision.predictedMotionYPixels}}},
        {"motion_residual_pixels", decision.motionResidualPixels},
        {"model_lineage_shift_pixels",
         {{"x", decision.modelLineageShiftXPixels},
          {"y", decision.modelLineageShiftYPixels}}},
        {"support_motion_continuation", decision.supportMotionContinuation},
        {"support_fusion_continuation", decision.supportFusionContinuation},
        {"model_lineage_continued", decision.modelLineageContinued},
        {"reverse_model_lineage_continued",
         decision.reverseModelLineageContinued},
        {"reverse_model_seed", decision.reverseModelSeed},
        {"bidirectional_conflict", decision.bidirectionalConflict},
        {"overlaps_previous_model", decision.overlapsPreviousModel},
        {"near_previous_model", decision.nearPreviousModel},
        {"terminal_taper_on_parent", decision.terminalTaperOnParent},
        {"immediate_terminal_taper_on_parent",
         decision.immediateTerminalTaperOnParent},
        {"terminal_taper_rebound_on_parent",
         decision.terminalTaperReboundOnParent}}},
      {"state",
       {{"contact_candidate", decision.contactCandidate},
        {"contact_confirmed", decision.contactConfirmed},
        {"pending_contact_length", decision.pendingContactLength},
        {"rooted_in_raft", decision.rootedInRaft},
        {"rooted_in_model", decision.rootedInModel},
        {"accepted", decision.accepted},
        {"mixed_semantic_projection", decision.mixedSemanticProjection},
        {"support_resolution", supportResolution}}},
      {"choice", semanticName(decision.decision)},
      {"reason_code", supportDecisionReasonCode(decision.reason)},
      {"why", supportDecisionReasonText(decision.reason)},
  };
}

nlohmann::json semanticRegionJson(
    const DiagnosticSemanticRegion& region,
    const LayerDiagnosticMetadata& metadata,
    std::size_t oneBasedLayer) {
  const auto convertX = [&](std::uint32_t sourceX) {
    if (sourceX <= metadata.sourceBounds.minX || metadata.width == 0u) {
      return 0u;
    }
    return std::min<std::uint32_t>(
        metadata.width,
        (sourceX - metadata.sourceBounds.minX + metadata.downsample - 1u)
            / metadata.downsample);
  };
  const auto convertY = [&](std::uint32_t sourceY) {
    if (sourceY <= metadata.sourceBounds.minY || metadata.height == 0u) {
      return 0u;
    }
    return std::min<std::uint32_t>(
        metadata.height,
        (sourceY - metadata.sourceBounds.minY + metadata.downsample - 1u)
            / metadata.downsample);
  };

  nlohmann::json parents = nlohmann::json::array();
  for (const auto& parent : region.parents) {
    parents.push_back({
        {"region_id", parent.regionId},
        {"lineage_id", parent.lineageId},
        {"overlap_pixels", parent.overlapPixels},
        {"current_coverage_ratio", parent.currentCoverageRatio},
        {"parent_coverage_ratio", parent.parentCoverageRatio},
    });
  }
  nlohmann::json mergedFrom = nlohmann::json::array();
  for (const auto& lineage : region.mergedFromLineageIds) {
    mergedFrom.push_back(lineage);
  }
  return {
      {"layer", oneBasedLayer},
      {"region_id", region.regionId},
      {"lineage_id", region.lineageId},
      {"derived_from_lineage_id",
       region.derivedFromLineageId.empty()
           ? nlohmann::json(nullptr)
           : nlohmann::json(region.derivedFromLineageId)},
      {"merged_from_lineage_ids", std::move(mergedFrom)},
      {"semantic", semanticName(region.semantic)},
      {"component_id", region.componentId},
      {"node_id",
       region.nodeId == std::numeric_limits<std::size_t>::max()
           ? nlohmann::json(nullptr)
           : nlohmann::json(region.nodeId)},
      {"selection_id", region.selectionId},
      {"area_pixels", region.areaPixels},
      {"bounds_pixels",
       {{"min_x", region.bounds.minX},
        {"min_y", region.bounds.minY},
        {"max_x", region.bounds.maxX},
        {"max_y", region.bounds.maxY}}},
      {"diagnostic_bounds_pixels",
       {{"min_x", convertX(region.bounds.minX)},
        {"min_y", convertY(region.bounds.minY)},
        {"max_x", std::max(convertX(region.bounds.maxX),
                            convertX(region.bounds.minX) + 1u)},
        {"max_y", std::max(convertY(region.bounds.maxY),
                            convertY(region.bounds.minY) + 1u)}}},
      {"parents", std::move(parents)},
  };
}

nlohmann::json summaryJson(const SupportAnalysisSummary& summary) {
  return {
      {"raft_last_layer", summary.raftLastLayer + 1u},
      {"first_model_layer", summary.firstModelLayer + 1u},
      {"last_support_layer", summary.lastSupportLayer + 1u},
      {"components", summary.componentCount},
      {"candidate_nodes", summary.candidateNodeCount},
      {"accepted_nodes", summary.acceptedNodeCount},
      {"raft_runs", summary.raftRunCount},
      {"support_runs", summary.supportRunCount},
      {"free_support_runs", summary.freeSupportRunCount},
      {"projected_support_runs", summary.projectedSupportRunCount},
      {"projected_contact_pixels", summary.projectedContactPixelCount},
      {"rejected_projection_runs", summary.rejectedProjectionRunCount},
      {"rejected_growth_pixels", summary.rejectedGrowthPixelCount},
      {"untapered_model_contacts", summary.untaperedModelContactCount},
      {"contacts_without_valid_projection",
       summary.contactsWithoutValidProjectionCount},
      {"maximum_contact_growth_ratio", summary.maximumContactGrowthRatio},
      {"terminal_support_stops", summary.terminalSupportStopCount},
      {"expanding_model_contacts", summary.expandingModelContactCount},
      {"maximum_model_expansion_ratio", summary.maximumModelExpansionRatio},
      {"continuations", summary.continuationEdgeCount},
      {"splits", summary.splitEdgeCount},
      {"braces", summary.braceEdgeCount},
      {"model_contacts", summary.modelContactEdgeCount},
      {"forced_semantic_samples", summary.forcedSemanticSampleCount},
      {"reverse_model_seeds", summary.reverseModelSeedCount},
      {"reverse_model_continuations", summary.reverseModelContinuationCount},
      {"bidirectional_mixed_components", summary.bidirectionalMixedComponentCount},
      {"vulkan_compute_compiled", summary.vulkanComputeCompiled},
      {"vulkan_compute_active", summary.vulkanComputeActive},
      {"vulkan_device", summary.vulkanDeviceName},
      {"vulkan_eligible_jobs", summary.vulkanEligibleJobCount},
      {"vulkan_submitted_jobs", summary.vulkanSubmittedJobCount},
      {"vulkan_gpu_jobs", summary.vulkanGpuJobCount},
      {"vulkan_cpu_fallback_jobs", summary.vulkanCpuFallbackJobCount},
      {"vulkan_dispatches", summary.vulkanDispatchCount},
      {"vulkan_dispatch_failures", summary.vulkanDispatchFailureCount},
      {"vulkan_max_batch_jobs", summary.vulkanMaximumBatchJobCount},
      {"vulkan_upload_bytes", summary.vulkanUploadBytes},
      {"vulkan_readback_bytes", summary.vulkanReadbackBytes},
      {"vulkan_host_prepare_us", summary.vulkanHostPreparationMicroseconds},
      {"vulkan_queue_wait_us", summary.vulkanQueueWaitMicroseconds},
      {"vulkan_batch_execution_us", summary.vulkanBatchExecutionMicroseconds},
      {"vulkan_run_source_jobs", summary.vulkanRunSourceJobCount},
      {"vulkan_resident_reference_uploads",
       summary.vulkanResidentReferenceUploadCount},
      {"vulkan_resident_reference_reuses",
       summary.vulkanResidentReferenceReuseCount},
      {"vulkan_submitted_workgroups", summary.vulkanSubmittedWorkgroupCount},
      {"vulkan_semantic_layer_batch_calls",
       summary.vulkanSemanticLayerBatchCallCount},
      {"vulkan_semantic_layer_batch_jobs",
       summary.vulkanSemanticLayerBatchJobCount},
      {"support_preparation_window", summary.supportPreparationWindowCapacity},
      {"support_prepared_layers", summary.supportPreparedLayerCount},
      {"support_max_preparation_inflight", summary.supportMaximumPreparationInflight},
      {"support_prepare_load_us", summary.supportPreparationLoadMicroseconds},
      {"support_prepare_describe_us", summary.supportPreparationDescribeMicroseconds},
      {"support_forward_semantic_us", summary.supportForwardSemanticMicroseconds},
      {"support_reverse_semantic_us", summary.supportReverseSemanticMicroseconds},
      {"support_forward_classification_us",
       summary.supportForwardClassificationMicroseconds},
      {"support_forward_commit_us", summary.supportForwardCommitMicroseconds},
      {"support_forward_lineage_us", summary.supportForwardLineageMicroseconds},
      {"support_forward_lineage_commit_us",
       summary.supportForwardLineageCommitMicroseconds},
      {"support_reverse_prepare_us", summary.supportReversePreparationMicroseconds},
      {"support_reverse_commit_us", summary.supportReverseCommitMicroseconds},
      {"support_semantic_evidence_us", summary.supportSemanticEvidenceMicroseconds},
      {"support_semantic_evidence_lots", summary.supportSemanticEvidenceLotCount},
      {"support_semantic_evidence_layer_pairs",
       summary.supportSemanticEvidenceLayerPairCount},
      {"support_semantic_evidence_edges", summary.supportSemanticEvidenceEdgeCount},
  };
}

std::string layerImageFileName(
    std::size_t layerOneBased,
    const char* panel) {
  std::ostringstream stream;
  stream << "layer_" << std::setw(6) << std::setfill('0') << layerOneBased
         << "_" << panel << ".png";
  return stream.str();
}

std::string layerJsonFileName(std::size_t layerOneBased) {
  std::ostringstream stream;
  stream << "layer_" << std::setw(6) << std::setfill('0') << layerOneBased
         << ".json";
  return stream.str();
}

bool writeJsonFile(
    const std::filesystem::path& path,
    const nlohmann::json& value,
    std::string& error) {
  std::ofstream file(path);
  if (!file) {
    error = "cannot open JSON output: " + path.string();
    return false;
  }
  file << value.dump(2) << '\n';
  if (!file) {
    error = "cannot write JSON output: " + path.string();
    return false;
  }
  return true;
}

} // namespace

const char* supportDecisionReasonCode(SupportDecisionReason reason) noexcept {
  switch (reason) {
  case SupportDecisionReason::RaftPrefix: return "raft_prefix";
  case SupportDecisionReason::FirstSupportLayer: return "first_support_layer";
  case SupportDecisionReason::SupportContinuation: return "support_continuation";
  case SupportDecisionReason::SupportMotionContinuation:
    return "support_motion_continuation";
  case SupportDecisionReason::SupportFusionContinuation:
    return "support_fusion_continuation";
  case SupportDecisionReason::SupportBornBeforeModel: return "support_born_before_model";
  case SupportDecisionReason::ModelRootCandidate: return "model_root_candidate";
  case SupportDecisionReason::ModelDominantMerge: return "model_dominant_merge";
  case SupportDecisionReason::RelativeExpansionBeforeFirstModel:
    return "relative_expansion_before_first_model";
  case SupportDecisionReason::UnrelatedAfterModel: return "unrelated_after_model";
  case SupportDecisionReason::ContactCandidateOpened: return "contact_candidate_opened";
  case SupportDecisionReason::ContactCandidateContinued:
    return "contact_candidate_continued";
  case SupportDecisionReason::ContactConfirmedAbrupt: return "contact_confirmed_abrupt";
  case SupportDecisionReason::ContactConfirmedProgressive:
    return "contact_confirmed_progressive";
  case SupportDecisionReason::MixedSemanticProjection:
    return "mixed_semantic_projection";
  case SupportDecisionReason::BidirectionalModelContinuation:
    return "bidirectional_model_continuation";
  case SupportDecisionReason::BidirectionalMixedReconciliation:
    return "bidirectional_mixed_reconciliation";
  case SupportDecisionReason::RejectedSupportPath: return "rejected_support_path";
  }
  return "unknown";
}

const char* supportDecisionReasonText(SupportDecisionReason reason) noexcept {
  switch (reason) {
  case SupportDecisionReason::RaftPrefix:
    return "The component belongs to the repeated raft prefix starting at layer 1.";
  case SupportDecisionReason::FirstSupportLayer:
    return "The first layer after the raft starts a raft-rooted support branch.";
  case SupportDecisionReason::SupportContinuation:
    return "A previous support parent was found and semantic continuity was preserved.";
  case SupportDecisionReason::SupportMotionContinuation:
    return "The translated section keeps the support shape and predicted trajectory, so local growth remains support matter.";
  case SupportDecisionReason::SupportFusionContinuation:
    return "Several previous support sections are preserved in the current component, so the local enlargement remains a support fusion.";
  case SupportDecisionReason::SupportBornBeforeModel:
    return "No model exists yet; the disconnected component remains part of the support network.";
  case SupportDecisionReason::ModelRootCandidate:
    return "The component starts beside established model matter and is tracked as a model-rooted support candidate.";
  case SupportDecisionReason::ModelDominantMerge:
    return "Previous model overlap dominates the recent support profile.";
  case SupportDecisionReason::RelativeExpansionBeforeFirstModel:
    return "The component expands beyond the complete previous support profile before the first model contact.";
  case SupportDecisionReason::UnrelatedAfterModel:
    return "No support parent exists after model matter has been established.";
  case SupportDecisionReason::ContactCandidateOpened:
    return "Growth after a terminal taper opened a provisional support-to-model contact.";
  case SupportDecisionReason::ContactCandidateContinued:
    return "The provisional contact persists on the following native layer.";
  case SupportDecisionReason::ContactConfirmedAbrupt:
    return "The local contact was confirmed by an abrupt expansion above the terminal support tip.";
  case SupportDecisionReason::ContactConfirmedProgressive:
    return "The local contact was confirmed by persistent cumulative growth above the terminal support tip.";
  case SupportDecisionReason::MixedSemanticProjection:
    return "Stable model matter and a raft-rooted support share one raster component; support runs were partitioned explicitly.";
  case SupportDecisionReason::BidirectionalModelContinuation:
    return "The descending model pass reaches this region and no surviving raft-rooted support core claims it after reconciliation.";
  case SupportDecisionReason::BidirectionalMixedReconciliation:
    return "Independent raft-rooted support and descending model provenance reach the same raster component; their pixel regions are preserved separately.";
  case SupportDecisionReason::RejectedSupportPath:
    return "The candidate path was not accepted as support, or a provisional contact did not persist.";
  }
  return "Unknown support-analysis decision.";
}

bool SupportAnalysisBundleWriter::write(
    photons::LayerMaskSource& source,
    const SupportAnalyzer& analyzer,
    const SupportAnalysisResult& result,
    const SupportAnalysisOptions& analysisOptions,
    const SupportAnalysisBundleMetadata& metadata,
    const std::filesystem::path& outputDirectory,
    const SupportAnalysisBundleOptions& options,
    const SupportAnalysisBundleCallbacks& callbacks,
    std::string& error) const {
  if (!result.ok) {
    error = "cannot export an unsuccessful support analysis";
    return false;
  }
  if (result.layers.size() != source.layerCount()) {
    error = "analysis layer count does not match source";
    return false;
  }
  if (options.downsample == 0u) {
    error = "diagnostic downsample must be positive";
    return false;
  }

  std::error_code filesystemError;
  std::filesystem::create_directories(outputDirectory, filesystemError);
  if (filesystemError) {
    error = "cannot create analysis bundle directory: " + filesystemError.message();
    return false;
  }
  const auto imagesDirectory = outputDirectory / "images";
  const auto layersDirectory = outputDirectory / "layers";
  if (options.writeImages) {
    std::filesystem::create_directories(imagesDirectory, filesystemError);
    if (filesystemError) {
      error = "cannot create diagnostic image directory: " + filesystemError.message();
      return false;
    }
  }
  std::filesystem::create_directories(layersDirectory, filesystemError);
  if (filesystemError) {
    error = "cannot create per-layer JSON directory: " + filesystemError.message();
    return false;
  }

  std::vector<std::vector<const SupportDecisionTrace*>> decisionsByLayer(
      source.layerCount());
  for (const auto& decision : result.decisions) {
    if (decision.layer < decisionsByLayer.size()) {
      decisionsByLayer[decision.layer].push_back(&decision);
    }
  }

  nlohmann::json analysis;
  analysis["schema"] = "accloud.support-analysis.v1";
  analysis["input"] = metadata.inputFileName;
  analysis["resolution"] = {source.width(), source.height()};
  analysis["layer_count"] = source.layerCount();
  analysis["pitch_mm"] = {
      metadata.pitchXMillimetres,
      metadata.pitchYMillimetres,
      metadata.pitchZMillimetres,
  };
  analysis["summary"] = summaryJson(result.summary);
  analysis["options"] = {
      {"worker_count", analysisOptions.workerCount},
      {"preparation_memory_budget_bytes", analysisOptions.preparationMemoryBudgetBytes},
      {"bitset_acceleration", analysisOptions.enableBitsetAcceleration},
      {"compute_preference",
       analysisOptions.computePreference == compute::SupportComputePreference::Cpu
           ? "cpu"
           : "auto"},
      {"vulkan_minimum_component_area_pixels",
       analysisOptions.vulkanMinimumComponentAreaPixels},
      {"minimum_track_layers", analysisOptions.minimumTrackLayers},
      {"taper_lookback_layers", analysisOptions.taperLookbackLayers},
      {"model_contact_confirmation_layers",
       analysisOptions.modelContactConfirmationLayers},
      {"maximum_layer_motion_pixels", analysisOptions.maximumLayerMotionPixels},
      {"minimum_terminal_taper_steps",
       analysisOptions.minimumTerminalTaperSteps},
      {"terminal_taper_step_ratio",
       analysisOptions.terminalTaperStepRatio},
      {"minimum_support_parent_coverage_ratio",
       analysisOptions.minimumSupportParentCoverageRatio},
      {"minimum_support_fusion_coverage_ratio",
       analysisOptions.minimumSupportFusionCoverageRatio},
      {"minimum_model_expansion_ratio",
       analysisOptions.minimumModelExpansionRatio},
      {"abrupt_model_expansion_ratio",
       analysisOptions.abruptModelExpansionRatio},
      {"terminal_taper_ratio", analysisOptions.terminalTaperRatio},
      {"model_root_taper_ratio", analysisOptions.modelRootTaperRatio},
  };
  analysis["forced_sample_layers"] = nlohmann::json::array();
  for (const auto forcedLayer : result.forcedSampleLayers) {
    analysis["forced_sample_layers"].push_back(forcedLayer + 1u);
  }
  analysis["layers"] = nlohmann::json::array();
  for (const auto& layer : result.layers) {
    analysis["layers"].push_back({
        {"layer", layer.layer + 1u},
        {"phase", phaseName(layer.phase)},
        {"support_components", layer.supportComponentIds.size()},
        {"projected_support_runs", layer.projectedSupportRuns.size()},
        {"decision_count", decisionsByLayer[layer.layer].size()},
    });
  }
  analysis["nodes"] = nlohmann::json::array();
  for (const auto& node : result.nodes) {
    analysis["nodes"].push_back({
        {"id", node.id},
        {"layer", node.layer + 1u},
        {"area_pixels", node.areaPixels},
        {"center_mm", {node.centerXMillimetres, node.centerYMillimetres}},
        {"equivalent_diameter_mm", node.equivalentDiameterMillimetres},
        {"kind", nodeKindName(node.kind)},
        {"rooted_in_raft", node.rootedInRaft},
        {"rooted_in_model", node.rootedInModel},
        {"terminal_taper", node.terminalTaper},
    });
  }
  analysis["edges"] = nlohmann::json::array();
  for (const auto& edge : result.edges) {
    analysis["edges"].push_back({
        {"lower_node", edge.lowerNode},
        {"upper_node", edge.upperNode},
        {"kind", edgeKindName(edge.kind)},
    });
  }

  nlohmann::json decisions;
  decisions["schema"] = "accloud.support-decisions.v1";
  decisions["input"] = metadata.inputFileName;
  decisions["decisions"] = nlohmann::json::array();
  for (const auto& decision : result.decisions) {
    decisions["decisions"].push_back(decisionJson(decision));
  }


  nlohmann::json overview = {
      {"schema", "accloud.support-analysis-summary.v1"},
      {"input", metadata.inputFileName},
      {"resolution", {source.width(), source.height()}},
      {"layer_count", source.layerCount()},
      {"pitch_mm",
       {metadata.pitchXMillimetres,
        metadata.pitchYMillimetres,
        metadata.pitchZMillimetres}},
      {"summary", analysis["summary"]},
      {"options", analysis["options"]},
      {"analysis_json", "analysis.json"},
      {"decisions_json", "decisions.json"},
      {"regions_json", "regions.json"},
  };

  if (!writeJsonFile(outputDirectory / "analysis.json", analysis, error)
      || !writeJsonFile(outputDirectory / "decisions.json", decisions, error)
      || !writeJsonFile(outputDirectory / "summary.json", overview, error)) {
    return false;
  }

  nlohmann::json manifest;
  manifest["schema"] = "accloud.support-analysis-bundle.v1";
  manifest["input"] = metadata.inputFileName;
  manifest["source_path"] = metadata.inputFileName;
  manifest["summary_json"] = "summary.json";
  manifest["analysis_json"] = "analysis.json";
  manifest["decisions_json"] = "decisions.json";
  manifest["regions_json"] = "regions.json";
  manifest["image_downsample"] = options.downsample;
  manifest["diagnostic_layout"] = {
      {"orientation", "vertical"},
      {"separate_images", true},
      {"interactive_panels", {"semantic_result", "decision_nodes"}},
      {"panels", {"raw_mask", "semantic_result", "decision_nodes"}},
      {"pick_map_encoding", "component_id_plus_one_rgb24"},
      {"region_pick_map_encoding", "semantic_region_selection_id_rgb24"},
      {"node_id_labels", true},
      {"node_id_label_policy",
       "model_contact_mixed_and_nonstandard_support"},
  };
  manifest["layer_count"] = source.layerCount();
  manifest["images"] = nlohmann::json::array();

  nlohmann::json regionsDocument;
  regionsDocument["schema"] = "accloud.support-regions.v1";
  regionsDocument["input"] = metadata.inputFileName;
  regionsDocument["lineage_method"] = "adjacent_exact_overlap";
  regionsDocument["regions"] = nlohmann::json::array();
  std::vector<DiagnosticSemanticRegion> previousRegions;
  std::array<std::size_t, 3> lineageCounters = {0u, 0u, 0u};

  for (std::size_t layer = 0; layer < source.layerCount(); ++layer) {
    if (callbacks.isCancelled && callbacks.isCancelled()) {
      error = "analysis bundle export cancelled";
      return false;
    }
    const auto rawFileName = layerImageFileName(layer + 1u, "raw");
    const auto semanticFileName = layerImageFileName(layer + 1u, "semantic");
    const auto nodesFileName = layerImageFileName(layer + 1u, "nodes");
    const auto pickFileName = layerImageFileName(layer + 1u, "pick");
    const auto regionPickFileName = layerImageFileName(layer + 1u, "regions");
    const auto jsonFileName = layerJsonFileName(layer + 1u);
    LayerDiagnosticMetadata diagnosticMetadata;
    if (options.writeImages
        && !writeLayerDiagnostics(
            source, analyzer, result, layer, options.downsample,
            decisionsByLayer[layer],
            imagesDirectory / rawFileName,
            imagesDirectory / semanticFileName,
            imagesDirectory / nodesFileName,
            imagesDirectory / pickFileName,
            imagesDirectory / regionPickFileName,
            diagnosticMetadata, error)) {
      return false;
    }
    assignDiagnosticLineages(
        diagnosticMetadata.semanticRegions, previousRegions, lineageCounters);

    nlohmann::json layerJson = {
        {"schema", "accloud.support-analysis-layer.v1"},
        {"layer", layer + 1u},
        {"phase", phaseName(result.layers[layer].phase)},
        {"image", options.writeImages ? "images/" + nodesFileName : ""},
        {"support_components", result.layers[layer].supportComponentIds.size()},
        {"projected_support_runs", result.layers[layer].projectedSupportRuns.size()},
        {"decisions", nlohmann::json::array()},
        {"semantic_regions", nlohmann::json::array()},
        {"diagnostic",
         {{"orientation", "vertical"},
          {"separate_images", true},
          {"interactive_panels", {"semantic_result", "decision_nodes"}},
          {"images",
           {{"raw_mask", options.writeImages ? "images/" + rawFileName : ""},
            {"semantic_result",
             options.writeImages ? "images/" + semanticFileName : ""},
            {"decision_nodes",
             options.writeImages ? "images/" + nodesFileName : ""},
            {"pick_map", options.writeImages ? "images/" + pickFileName : ""},
            {"region_pick_map",
             options.writeImages ? "images/" + regionPickFileName : ""}}},
          {"source_bounds_pixels",
           {{"min_x", diagnosticMetadata.sourceBounds.minX},
            {"min_y", diagnosticMetadata.sourceBounds.minY},
            {"max_x", diagnosticMetadata.sourceBounds.maxX},
            {"max_y", diagnosticMetadata.sourceBounds.maxY}}},
          {"image_size_pixels",
           {{"width", diagnosticMetadata.width},
            {"height", diagnosticMetadata.height}}},
          {"pick_map_encoding", "component_id_plus_one_rgb24"},
          {"region_pick_map_encoding", "semantic_region_selection_id_rgb24"},
          {"node_id_labels", nlohmann::json::array()}}},
    };
    for (const auto* decision : decisionsByLayer[layer]) {
      if (decision != nullptr) {
        auto serialized = decisionJson(*decision);
        serialized["selection_id"] = decision->componentId + 1u;
        layerJson["decisions"].push_back(std::move(serialized));
        if (shouldLabelNode(*decision)) {
          layerJson["diagnostic"]["node_id_labels"].push_back(decision->nodeId);
        }
      }
    }
    for (const auto& region : diagnosticMetadata.semanticRegions) {
      auto serialized = semanticRegionJson(region, diagnosticMetadata, layer + 1u);
      layerJson["semantic_regions"].push_back(serialized);
      regionsDocument["regions"].push_back(std::move(serialized));
    }
    if (!writeJsonFile(layersDirectory / jsonFileName, layerJson, error)) {
      return false;
    }

    manifest["images"].push_back({
        {"layer", layer + 1u},
        {"path", options.writeImages ? "images/" + nodesFileName : ""},
        {"raw_path", options.writeImages ? "images/" + rawFileName : ""},
        {"semantic_path",
         options.writeImages ? "images/" + semanticFileName : ""},
        {"nodes_path", options.writeImages ? "images/" + nodesFileName : ""},
        {"pick_path", options.writeImages ? "images/" + pickFileName : ""},
        {"region_pick_path",
         options.writeImages ? "images/" + regionPickFileName : ""},
        {"layer_json", "layers/" + jsonFileName},
        {"decision_count", decisionsByLayer[layer].size()},
        {"semantic_region_count", diagnosticMetadata.semanticRegions.size()},
    });
    previousRegions = diagnosticMetadata.semanticRegions;
    if (callbacks.progress) {
      callbacks.progress(layer + 1u, source.layerCount());
    }
  }

  return writeJsonFile(outputDirectory / "regions.json", regionsDocument, error)
         && writeJsonFile(outputDirectory / "manifest.json", manifest, error);
}

} // namespace accloud::render3d
