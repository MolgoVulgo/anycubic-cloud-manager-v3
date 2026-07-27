#include "infra/cloud/api/ThumbnailCandidateBuilder.h"

#include <algorithm>

namespace accloud::cloud::api {
namespace {

bool isAbsoluteHttpUrl(const std::string& value) {
    return value.rfind("http://", 0) == 0 || value.rfind("https://", 0) == 0;
}

std::string imagePathToUrl(const std::string& value,
                           const std::string& bucket,
                           const std::string& region) {
    if (value.empty()) return {};
    if (isAbsoluteHttpUrl(value)) return value;
    if (bucket.empty() || region.empty()) return {};

    std::size_t first = 0;
    while (first < value.size() && value[first] == '/') {
        ++first;
    }
    if (first == value.size()) return {};
    return "https://" + bucket + ".s3." + region + ".amazonaws.com/"
        + value.substr(first);
}

void appendUnique(std::vector<std::string>& candidates, const std::string& value) {
    if (value.empty()) return;
    if (std::find(candidates.begin(), candidates.end(), value) == candidates.end()) {
        candidates.push_back(value);
    }
}

} // namespace

std::vector<std::string> buildThumbnailCandidates(const ThumbnailCandidateInput& input) {
    std::vector<std::string> candidates;
    appendUnique(candidates, input.thumbnail);
    appendUnique(candidates, input.image);
    appendUnique(candidates, imagePathToUrl(input.imageId, input.bucket, input.region));
    appendUnique(candidates, input.printerImageId);
    appendUnique(candidates, imagePathToUrl(input.image0Id, input.bucket, input.region));
    return candidates;
}

} // namespace accloud::cloud::api
