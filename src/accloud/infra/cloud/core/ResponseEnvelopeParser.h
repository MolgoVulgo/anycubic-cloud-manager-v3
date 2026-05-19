#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace accloud::cloud::core {

struct EnvelopeParseResult {
    bool jsonValid{false};
    bool envelopePresent{false};
    bool success{false};
    int code{0};
    std::string message;
    nlohmann::json data;
    std::string error;
};

class ResponseEnvelopeParser {
public:
    EnvelopeParseResult parse(const std::string& body) const;
};

} // namespace accloud::cloud::core
