#include "infra/cloud/api/ThumbnailCandidateBuilder.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

bool test_candidate_order_and_relative_paths() {
    accloud::cloud::api::ThumbnailCandidateInput input;
    input.thumbnail = "https://signed.example.test/main.jpg?X-Amz-Signature=abc";
    input.image = "https://cdn.example.test/image.jpg";
    input.imageId = "cloud/2026/07-26/jpg/main.jpg";
    input.printerImageId = "https://bucket.example.test/printer.jpg";
    input.image0Id = "/cloud/2026/07-26/jpg/printer.jpg";
    input.bucket = "cloud-slice-prod";
    input.region = "us-east-2";

    const std::vector<std::string> candidates =
        accloud::cloud::api::buildThumbnailCandidates(input);

    return expect(candidates.size() == 5, "five distinct candidates expected")
        && expect(candidates[0] == input.thumbnail, "thumbnail must be first")
        && expect(candidates[1] == input.image, "img/image must be second")
        && expect(candidates[2]
                      == "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026/07-26/jpg/main.jpg",
                  "slice_param.image_id must be expanded")
        && expect(candidates[3] == input.printerImageId,
                  "printer_image_id must remain a fallback")
        && expect(candidates[4]
                      == "https://cloud-slice-prod.s3.us-east-2.amazonaws.com/cloud/2026/07-26/jpg/printer.jpg",
                  "slice_param.image0_id must be expanded last");
}

bool test_duplicate_candidates_are_removed() {
    accloud::cloud::api::ThumbnailCandidateInput input;
    input.thumbnail = "https://cdn.example.test/same.jpg";
    input.image = input.thumbnail;
    input.imageId = input.thumbnail;
    input.printerImageId = input.thumbnail;
    input.image0Id = input.thumbnail;

    const auto candidates = accloud::cloud::api::buildThumbnailCandidates(input);
    return expect(candidates.size() == 1, "duplicate candidate URLs must be removed")
        && expect(candidates.front() == input.thumbnail, "first occurrence must be preserved");
}

} // namespace

int main() {
    if (!test_candidate_order_and_relative_paths()
        || !test_duplicate_candidates_are_removed()) {
        return 1;
    }
    std::cout << "Thumbnail candidate tests passed\n";
    return 0;
}
