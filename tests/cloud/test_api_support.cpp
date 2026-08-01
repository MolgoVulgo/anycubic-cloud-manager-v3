#include "infra/cloud/api/ApiSupport.h"

#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

bool testScalarConversions() {
    using namespace accloud::cloud::api::support;
    const nlohmann::json object = {
        {"id", 42},
        {"enabled", true},
        {"count", "17"},
        {"epoch_ms", 1700000000123LL},
    };

    return expect(firstString(object, {"missing", "id"}) == "42",
                  "firstString should accept integer identifiers")
        && expect(firstInt(object, {"enabled"}, 0) == 1,
                  "firstInt should accept booleans")
        && expect(firstInt(object, {"count"}, 0) == 17,
                  "firstInt should accept numeric strings")
        && expect(firstLong(object, {"epoch_ms"}, 0) == 1700000000123LL,
                  "firstLong should preserve 64-bit values")
        && expect(normalizeEpochSeconds(1700000000123LL) == 1700000000LL,
                  "millisecond epochs should normalize to seconds");
}

bool testStructuredHelpers() {
    using namespace accloud::cloud::api::support;
    const nlohmann::json object = {
        {"embedded", R"json({"progress":55,"remain_time":"2"})json"},
        {"items", nlohmann::json::array({"A", 2, "B"})},
    };

    const auto embedded = objectOrParsedString(object, "embedded");
    return expect(embedded.is_object(), "embedded JSON strings should be parsed as objects")
        && expect(firstInt(embedded, {"progress"}, -1) == 55,
                  "parsed object should expose progress")
        && expect(durationSecondsFromObject(embedded,
                                            {"remaining_sec"},
                                            {"remain_time"}) == 120,
                  "minute durations should convert to seconds")
        && expect(joinJsonStringArray(object["items"]) == "A, 2, B",
                  "mixed scalar arrays should join predictably")
        && expect(containsNoCase("Printer OFFLINE", "offline"),
                  "containsNoCase should be case insensitive");
}

} // namespace

int main() {
    const bool ok = testScalarConversions() && testStructuredHelpers();
    if (!ok) return 1;
    std::cout << "Cloud API support tests passed\n";
    return 0;
}
