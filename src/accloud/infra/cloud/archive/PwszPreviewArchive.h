#pragma once

#include <filesystem>
#include <string>

namespace accloud::cloud::archive {

struct PwszPreviewInspection {
    bool ok{false};
    bool hasPreview1{false};
    bool hasPreview2{false};
    bool needsCompletion{false};
    std::string message;
};

struct PwszPreviewPreparation {
    bool ok{false};
    bool changed{false};
    std::filesystem::path preparedPath;
    std::string message;
};

struct PwszPreviewCommitResult {
    bool ok{false};
    std::string message;
};

[[nodiscard]] PwszPreviewInspection inspectPwszPreviewArchive(
    const std::filesystem::path& archivePath);

[[nodiscard]] PwszPreviewPreparation preparePwszPreview2Copy(
    const std::filesystem::path& archivePath);

[[nodiscard]] PwszPreviewCommitResult replaceOriginalWithPrepared(
    const std::filesystem::path& originalPath,
    const std::filesystem::path& preparedPath);

void discardPreparedFile(const std::filesystem::path& preparedPath) noexcept;

} // namespace accloud::cloud::archive
