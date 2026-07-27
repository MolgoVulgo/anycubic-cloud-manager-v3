#pragma once

#include <string>
#include <vector>

namespace accloud::cloud::api {

struct ThumbnailCandidateInput {
    std::string thumbnail;
    std::string image;
    std::string imageId;
    std::string printerImageId;
    std::string image0Id;
    std::string bucket;
    std::string region;
};

std::vector<std::string> buildThumbnailCandidates(const ThumbnailCandidateInput& input);

} // namespace accloud::cloud::api
