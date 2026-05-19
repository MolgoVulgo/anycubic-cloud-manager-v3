#include "ResponseEnvelopeParser.h"

namespace accloud::cloud::core {

EnvelopeParseResult ResponseEnvelopeParser::parse(const std::string& body) const {
    EnvelopeParseResult result;
    const auto parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) {
        result.error = "invalid_json";
        return result;
    }

    result.jsonValid = true;
    if (!parsed.is_object() || !parsed.contains("code")) {
        result.error = "missing_code";
        return result;
    }

    result.envelopePresent = true;
    const auto& code = parsed["code"];
    if (code.is_number_integer()) {
        result.code = code.get<int>();
    } else if (code.is_string()) {
        try {
            result.code = std::stoi(code.get<std::string>());
        } catch (...) {
            result.code = 0;
        }
    }

    if (parsed.contains("msg") && parsed["msg"].is_string()) {
        result.message = parsed["msg"].get<std::string>();
    }
    if (parsed.contains("message") && parsed["message"].is_string() && result.message.empty()) {
        result.message = parsed["message"].get<std::string>();
    }
    if (parsed.contains("data")) {
        result.data = parsed["data"];
    }
    result.success = (result.code == 1);
    return result;
}

} // namespace accloud::cloud::core
