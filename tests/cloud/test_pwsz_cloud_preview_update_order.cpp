#include "app/usecases/cloud/UpdateCloudPwszPreviewsUseCase.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using accloud::usecases::cloud::CloudPwszPreviewUpdateItem;
using accloud::usecases::cloud::orderCloudPwszPreviewUpdateItemsOldestFirst;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void requireId(const std::vector<CloudPwszPreviewUpdateItem>& items,
               std::size_t index,
               const std::string& expected) {
    require(index < items.size(), "ordered item index out of range");
    require(items[index].fileId == expected, "unexpected cloud update order");
}

} // namespace

int main() {
    const std::vector<CloudPwszPreviewUpdateItem> input{
        {"50", "recent.pwsz", 10, 300},
        {"11", "old-second.pwsz", 10, 100},
        {"9", "old-first.pwsz", 10, 100},
        {"12", "middle.pwsz", 10, 200},
        {"unknown-b", "unknown-b.pwsz", 10, 0},
        {"unknown-a", "unknown-a.pwsz", 10, 0},
    };

    const auto ordered = orderCloudPwszPreviewUpdateItemsOldestFirst(input);
    require(ordered.size() == input.size(), "all update candidates must be preserved");
    requireId(ordered, 0, "9");
    requireId(ordered, 1, "11");
    requireId(ordered, 2, "12");
    requireId(ordered, 3, "50");
    requireId(ordered, 4, "unknown-a");
    requireId(ordered, 5, "unknown-b");

    std::cout << "PWSZ cloud preview update ordering tests passed\n";
    return 0;
}
