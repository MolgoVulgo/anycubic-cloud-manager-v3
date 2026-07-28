#pragma once

#include "infra/cloud/CloudClient.h"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace accloud::usecases::cloud {

struct LoadCloudFilesResult {
    bool ok{false};
    std::string message;
    std::vector<accloud::cloud::CloudFileInfo> files;
};

struct LoadAllCloudFilesResult {
    bool ok{false};
    bool complete{false};
    std::string message;
    std::vector<accloud::cloud::CloudFileInfo> files;
    int pagesLoaded{0};
};

using CloudFilesPageLoader = std::function<LoadCloudFilesResult(int page, int limit)>;

namespace detail {

inline std::string cloudFileIdentity(const accloud::cloud::CloudFileInfo& file) {
    if (!file.id.empty()) {
        return std::string{"id:"} + file.id;
    }
    return std::string{"fallback:"} + file.name + ':' + std::to_string(file.createTime)
         + ':' + std::to_string(file.sizeBytes);
}

} // namespace detail

[[nodiscard]] inline LoadAllCloudFilesResult collectAllCloudFilePages(
    const CloudFilesPageLoader& loadPage,
    int pageSize = 100,
    int maxPages = 100) {
    LoadAllCloudFilesResult out;
    if (!loadPage) {
        out.message = "Chargeur de pages cloud absent";
        return out;
    }

    pageSize = std::clamp(pageSize, 1, 1000);
    maxPages = std::clamp(maxPages, 1, 1000);
    std::unordered_set<std::string> seen;

    for (int page = 1; page <= maxPages; ++page) {
        LoadCloudFilesResult current = loadPage(page, pageSize);
        if (!current.ok) {
            out.message = current.message.empty()
                              ? "Lecture incomplète des fichiers cloud"
                              : current.message;
            return out;
        }

        out.pagesLoaded = page;
        const std::size_t pageCount = current.files.size();
        std::size_t added = 0;
        for (auto& file : current.files) {
            if (seen.insert(detail::cloudFileIdentity(file)).second) {
                out.files.push_back(std::move(file));
                ++added;
            }
        }

        if (pageCount == 0) {
            out.ok = true;
            out.complete = true;
            out.message = std::to_string(out.files.size()) + " fichier(s)";
            return out;
        }
        if (added == 0) {
            out.message = "Pagination cloud sans progression; inventaire complet non confirmé";
            return out;
        }
    }

    out.message = "Limite de pagination cloud atteinte; inventaire complet non confirmé";
    return out;
}

class LoadCloudFilesUseCase {
public:
    LoadCloudFilesResult execute(int page, int limit) const;
    LoadAllCloudFilesResult executeAll(int pageSize = 100, int maxPages = 100) const;
};

} // namespace accloud::usecases::cloud
