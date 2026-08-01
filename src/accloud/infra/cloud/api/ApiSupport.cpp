#include "ApiSupport.h"

#ifdef ACCLOUD_WITH_QT
#include "infra/cloud/core/HttpClient.h"
#include "infra/cloud/core/WorkbenchRequestBuilder.h"
#endif

#include <algorithm>
#include <cctype>
#include <sstream>

namespace accloud::cloud::api::support {

#ifdef ACCLOUD_WITH_QT

RawResponse workbenchCall(core::EndpointId endpointId,
                          const std::string& accessToken,
                          const std::string& xxToken,
                          const QByteArray& body,
                          const QString& queryString) {
    const auto endpoint = core::EndpointRegistry::instance().find(endpointId);
    if (!endpoint.has_value()) {
        return RawResponse{false, 0, {}, "Unknown endpoint id"};
    }

    const core::WorkbenchRequestBuilder builder;
    const auto built = builder.build(*endpoint, accessToken, xxToken, queryString, body);
    if (!built.has_value()) {
        return RawResponse{false, 0, {}, "Failed to build request"};
    }

    const core::HttpClient client;
    const core::HttpResponse response = client.execute(*built);
    return RawResponse{response.ok, response.httpStatus, response.body, response.error};
}

RawResponse workbenchPost(core::EndpointId endpointId,
                          const std::string& accessToken,
                          const std::string& xxToken,
                          const QByteArray& jsonBody) {
    return workbenchCall(endpointId, accessToken, xxToken, jsonBody);
}

RawResponse workbenchPostForm(core::EndpointId endpointId,
                              const std::string& accessToken,
                              const std::string& xxToken,
                              const QByteArray& formBody) {
    return workbenchCall(endpointId, accessToken, xxToken, formBody);
}

RawResponse workbenchGet(core::EndpointId endpointId,
                         const std::string& accessToken,
                         const std::string& xxToken,
                         const QString& queryString) {
    return workbenchCall(endpointId, accessToken, xxToken, {}, queryString);
}

#endif

std::string jsonString(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_float()) {
        std::ostringstream stream;
        stream << value.get<double>();
        return stream.str();
    }
    return {};
}

std::string firstString(const nlohmann::json& object,
                        std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        if (!object.contains(key)) continue;
        const auto value = jsonString(object[key]);
        if (!value.empty()) return value;
    }
    return {};
}

int jsonInt(const nlohmann::json& value, int fallback) {
    if (value.is_number_integer()) return value.get<int>();
    if (value.is_number_float()) return static_cast<int>(value.get<double>());
    if (value.is_boolean()) return value.get<bool>() ? 1 : 0;
    if (value.is_string()) {
        try {
            return std::stoi(value.get<std::string>());
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

int firstInt(const nlohmann::json& object,
             std::initializer_list<const char*> keys,
             int fallback) {
    for (const char* key : keys) {
        if (!object.contains(key)) continue;
        return jsonInt(object[key], fallback);
    }
    return fallback;
}

long long jsonLong(const nlohmann::json& value, long long fallback) {
    if (value.is_number_integer()) return value.get<long long>();
    if (value.is_number_float()) return static_cast<long long>(value.get<double>());
    if (value.is_boolean()) return value.get<bool>() ? 1 : 0;
    if (value.is_string()) {
        try {
            return std::stoll(value.get<std::string>());
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

long long firstLong(const nlohmann::json& object,
                    std::initializer_list<const char*> keys,
                    long long fallback) {
    for (const char* key : keys) {
        if (!object.contains(key)) continue;
        return jsonLong(object[key], fallback);
    }
    return fallback;
}

long long normalizeEpochSeconds(long long epoch) {
    if (epoch <= 0) return 0;
    if (epoch > 1000000000000LL) return epoch / 1000;
    return epoch;
}

std::string joinJsonStringArray(const nlohmann::json& value) {
    if (!value.is_array()) return {};
    std::string output;
    for (const auto& item : value) {
        const std::string part = jsonString(item);
        if (part.empty()) continue;
        if (!output.empty()) output += ", ";
        output += part;
    }
    return output;
}

bool containsNoCase(const std::string& text, const std::string& needle) {
    std::string left = text;
    std::string right = needle;
    std::transform(left.begin(), left.end(), left.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(right.begin(), right.end(), right.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return left.find(right) != std::string::npos;
}

std::string formatSeconds(long long seconds) {
    if (seconds <= 0) return {};
    const int hours = static_cast<int>(seconds / 3600);
    const int minutes = static_cast<int>((seconds % 3600) / 60);
    const int remainingSeconds = static_cast<int>(seconds % 60);
    if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    return std::to_string(minutes) + "m " + std::to_string(remainingSeconds) + "s";
}

int durationSecondsFromObject(const nlohmann::json& object,
                              std::initializer_list<const char*> secondKeys,
                              std::initializer_list<const char*> minuteKeys) {
    if (!object.is_object()) return -1;
    const int seconds = firstInt(object, secondKeys, -1);
    if (seconds >= 0) return seconds;
    const int minutes = firstInt(object, minuteKeys, -1);
    if (minutes >= 0) return minutes * 60;
    return -1;
}

nlohmann::json objectOrParsedString(const nlohmann::json& parent, const char* key) {
    if (!parent.is_object() || !parent.contains(key)) return {};
    const auto& value = parent[key];
    if (value.is_object()) return value;
    if (!value.is_string()) return {};
    auto parsed = nlohmann::json::parse(value.get<std::string>(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return {};
    return parsed;
}

} // namespace accloud::cloud::api::support
