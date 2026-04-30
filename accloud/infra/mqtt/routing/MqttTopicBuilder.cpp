#include "MqttTopicBuilder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace accloud::mqtt::routing {
namespace {

std::string trimAscii(std::string value) {
    auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string joinTopic(const std::string& prefix,
                      const std::string& machineType,
                      const std::string& deviceId,
                      const std::string& endpoint,
                      const std::string& suffix = {}) {
    std::string out = prefix;
    out += machineType;
    out += "/";
    out += deviceId;
    out += "/";
    out += endpoint;
    if (!suffix.empty()) {
        out += "/";
        out += suffix;
    }
    return out;
}

} // namespace

std::vector<std::string> MqttTopicBuilder::buildUserReportTopics(const std::string& userId,
                                                                 const std::string& userIdMd5) {
    const std::string uid = trimAscii(userId);
    const std::string uidMd5 = trimAscii(userIdMd5);
    if (uid.empty() || uidMd5.empty()) {
        return {};
    }
    const std::string root = "anycubic/anycubicCloud/v1/server/app/" + uid + "/" + uidMd5;
    return {
        root + "/slice/report",
        root + "/fdmslice/report",
    };
}

std::vector<std::string> MqttTopicBuilder::buildPrinterSubscriptionTopics(const std::string& machineType,
                                                                          const std::string& deviceId,
                                                                          bool includeExtendedTopics) {
    const std::string mt = trimAscii(machineType);
    const std::string did = trimAscii(deviceId);
    if (mt.empty() || did.empty()) {
        return {};
    }

    std::vector<std::string> topics = {
        "anycubic/anycubicCloud/v1/printer/public/" + mt + "/" + did + "/#",
    };
    if (!includeExtendedTopics) {
        return topics;
    }

    static constexpr std::array<const char*, 14> kCommandEndpoints = {
        "airpure",
        "autoOperation",
        "axis",
        "exposure",
        "file",
        "network",
        "ota",
        "print",
        "releaseFilm",
        "residual",
        "resin",
        "response",
        "smartResinVat",
        "wifi",
    };
    static constexpr std::array<const char*, 3> kServerEndpoints = {
        "status",
        "user",
        "video",
    };
    topics.reserve(topics.size() + kCommandEndpoints.size() + kServerEndpoints.size() + 2);

    for (const char* endpoint : kCommandEndpoints) {
        topics.push_back(joinTopic("anycubic/anycubicCloud/v1/+/printer/", mt, did, endpoint));
    }
    for (const char* endpoint : kServerEndpoints) {
        topics.push_back(joinTopic("anycubic/anycubicCloud/v1/server/printer/", mt, did, endpoint));
    }
    // Legacy topics still observed on some firmware branches.
    topics.push_back(joinTopic("anycubic/anycubicCloud/+/printer/", mt, did, "print"));
    topics.push_back(joinTopic("anycubic/anycubicCloud/printer/public/", mt, did, "online/status"));
    return topics;
}

std::optional<std::string> MqttTopicBuilder::buildPrinterPublishTopic(const std::string& machineType,
                                                                      const std::string& deviceId,
                                                                      const std::string& endpoint) {
    const std::string mt = trimAscii(machineType);
    const std::string did = trimAscii(deviceId);
    const std::string ep = trimAscii(endpoint);
    if (mt.empty() || did.empty() || ep.empty()) {
        return std::nullopt;
    }
    return "anycubic/anycubicCloud/v1/printer/public/" + mt + "/" + did + "/" + ep;
}

} // namespace accloud::mqtt::routing
