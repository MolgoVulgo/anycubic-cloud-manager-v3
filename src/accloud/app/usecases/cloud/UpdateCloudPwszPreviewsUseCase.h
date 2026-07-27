#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace accloud::usecases::cloud {

struct CloudPwszPreviewUpdateItem {
    std::string fileId;
    std::string fileName;
    std::uint64_t sizeBytes{0};
    std::int64_t createTime{0};
};

namespace detail {

inline bool isDecimalId(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    for (const char ch : value) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

inline std::string_view normalizedDecimalId(std::string_view value) {
    const auto firstNonZero = value.find_first_not_of('0');
    return firstNonZero == std::string_view::npos
               ? value.substr(value.size() - 1)
               : value.substr(firstNonZero);
}

inline bool cloudFileIdLess(std::string_view lhs, std::string_view rhs) {
    if (lhs == rhs) {
        return false;
    }
    if (isDecimalId(lhs) && isDecimalId(rhs)) {
        const std::string_view normalizedLhs = normalizedDecimalId(lhs);
        const std::string_view normalizedRhs = normalizedDecimalId(rhs);
        if (normalizedLhs.size() != normalizedRhs.size()) {
            return normalizedLhs.size() < normalizedRhs.size();
        }
        if (normalizedLhs != normalizedRhs) {
            return normalizedLhs < normalizedRhs;
        }
        return false;
    }
    return lhs < rhs;
}

} // namespace detail

[[nodiscard]] inline std::vector<CloudPwszPreviewUpdateItem>
orderCloudPwszPreviewUpdateItemsOldestFirst(
    const std::vector<CloudPwszPreviewUpdateItem>& items) {
    std::vector<CloudPwszPreviewUpdateItem> ordered = items;
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
        const bool lhsHasTime = lhs.createTime > 0;
        const bool rhsHasTime = rhs.createTime > 0;
        if (lhsHasTime != rhsHasTime) {
            return lhsHasTime;
        }
        if (lhsHasTime && lhs.createTime != rhs.createTime) {
            return lhs.createTime < rhs.createTime;
        }
        return detail::cloudFileIdLess(lhs.fileId, rhs.fileId);
    });
    return ordered;
}

struct CloudPwszPreviewUpdateItemResult {
    std::string originalFileId;
    std::string newFileId;
    std::string fileName;
    std::string status; // modified | skipped | failed | partial
    std::string message;
};

struct CloudPwszPreviewUpdateResult {
    bool ok{false};
    int modified{0};
    int skipped{0};
    int failed{0};
    int partial{0};
    std::vector<CloudPwszPreviewUpdateItemResult> items;
};

class UpdateCloudPwszPreviewsUseCase {
public:
    using ProgressCallback = std::function<void(int current,
                                                int total,
                                                const std::string& fileName,
                                                const std::string& phase)>;

    [[nodiscard]] CloudPwszPreviewUpdateResult execute(
        const std::vector<CloudPwszPreviewUpdateItem>& items,
        const ProgressCallback& onProgress = {}) const;
};

} // namespace accloud::usecases::cloud
