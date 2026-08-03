#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace accloud::photons {

class BinaryMask {
public:
  BinaryMask() = default;

  BinaryMask(std::uint32_t width, std::uint32_t height)
      : width_(width),
        height_(height),
        wordsPerRow_(wordCountFor(width)),
        words_(checkedWordCount(wordsPerRow_, height), 0u) {}

  [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
  [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
  [[nodiscard]] std::size_t wordsPerRow() const noexcept { return wordsPerRow_; }
  [[nodiscard]] std::size_t pixelCount() const noexcept {
    return static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
  }
  [[nodiscard]] bool empty() const noexcept { return pixelCount() == 0; }

  [[nodiscard]] bool test(std::uint32_t x, std::uint32_t y) const {
    if (x >= width_ || y >= height_) {
      throw std::out_of_range("BinaryMask coordinate outside raster");
    }
    return testUnchecked(x, y);
  }

  [[nodiscard]] bool testUnchecked(std::uint32_t x, std::uint32_t y) const noexcept {
    const std::size_t wordIndex = static_cast<std::size_t>(y) * wordsPerRow_
                                  + x / kWordBits;
    return (words_[wordIndex] & (std::uint64_t{1} << (x % kWordBits))) != 0u;
  }

  [[nodiscard]] bool testLinear(std::size_t index) const {
    if (index >= pixelCount()) {
      throw std::out_of_range("BinaryMask index outside raster");
    }
    return testLinearUnchecked(index);
  }

  [[nodiscard]] bool testLinearUnchecked(std::size_t index) const noexcept {
    const auto y = static_cast<std::uint32_t>(index / width_);
    const auto x = static_cast<std::uint32_t>(index % width_);
    return testUnchecked(x, y);
  }

  [[nodiscard]] std::uint64_t rowWord(
      std::uint32_t y,
      std::size_t wordIndex) const noexcept {
    return words_[static_cast<std::size_t>(y) * wordsPerRow_ + wordIndex];
  }

  void set(std::uint32_t x, std::uint32_t y, bool value = true) {
    if (x >= width_ || y >= height_) {
      throw std::out_of_range("BinaryMask coordinate outside raster");
    }
    setUnchecked(x, y, value);
  }

  void setUnchecked(std::uint32_t x, std::uint32_t y, bool value = true) noexcept {
    const std::size_t wordIndex = static_cast<std::size_t>(y) * wordsPerRow_
                                  + x / kWordBits;
    const std::uint64_t bit = std::uint64_t{1} << (x % kWordBits);
    if (value) {
      words_[wordIndex] |= bit;
    } else {
      words_[wordIndex] &= ~bit;
    }
  }

  void setLinear(std::size_t index, bool value = true) {
    if (index >= pixelCount()) {
      throw std::out_of_range("BinaryMask index outside raster");
    }
    const auto y = static_cast<std::uint32_t>(index / width_);
    const auto x = static_cast<std::uint32_t>(index % width_);
    setUnchecked(x, y, value);
  }

  void setRun(std::size_t first, std::size_t length) {
    if (length == 0) {
      return;
    }
    if (first > pixelCount() || length > pixelCount() - first) {
      throw std::out_of_range("BinaryMask run outside raster");
    }

    std::size_t cursor = first;
    std::size_t remaining = length;
    while (remaining > 0) {
      const auto y = static_cast<std::uint32_t>(cursor / width_);
      const auto x = static_cast<std::uint32_t>(cursor % width_);
      const std::size_t rowLength = std::min<std::size_t>(remaining, width_ - x);
      setRowRun(y, x, static_cast<std::uint32_t>(rowLength));
      cursor += rowLength;
      remaining -= rowLength;
    }
  }

  [[nodiscard]] std::size_t count() const noexcept {
    std::size_t result = 0;
    for (const std::uint64_t word : words_) {
      result += static_cast<std::size_t>(std::popcount(word));
    }
    return result;
  }

  [[nodiscard]] const std::vector<std::uint64_t>& words() const noexcept {
    return words_;
  }

private:
  static constexpr std::uint32_t kWordBits = 64;

  static std::size_t wordCountFor(std::uint32_t width) noexcept {
    return (static_cast<std::size_t>(width) + kWordBits - 1) / kWordBits;
  }

  static std::size_t checkedWordCount(std::size_t wordsPerRow, std::uint32_t height) {
    if (height != 0
        && wordsPerRow > std::numeric_limits<std::size_t>::max() / height) {
      throw std::overflow_error("BinaryMask raster is too large");
    }
    return wordsPerRow * height;
  }

  void setRowRun(
      std::uint32_t y,
      std::uint32_t firstX,
      std::uint32_t length) noexcept {
    std::uint32_t x = firstX;
    std::uint32_t remaining = length;
    while (remaining > 0) {
      const std::uint32_t bit = x % kWordBits;
      const std::uint32_t available = kWordBits - bit;
      const std::uint32_t count = std::min(remaining, available);
      const std::uint64_t lowMask = count == kWordBits
                                        ? std::numeric_limits<std::uint64_t>::max()
                                        : ((std::uint64_t{1} << count) - 1u);
      const std::size_t wordIndex = static_cast<std::size_t>(y) * wordsPerRow_
                                    + x / kWordBits;
      words_[wordIndex] |= lowMask << bit;
      x += count;
      remaining -= count;
    }
  }

  std::uint32_t width_{0};
  std::uint32_t height_{0};
  std::size_t wordsPerRow_{0};
  std::vector<std::uint64_t> words_;
};

} // namespace accloud::photons
