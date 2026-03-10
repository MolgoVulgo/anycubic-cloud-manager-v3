#include "MqttTopicBuilder.h"

#include <algorithm>
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
                                                                          const std::string& printerKey) {
    const std::string mt = trimAscii(machineType);
    const std::string pk = trimAscii(printerKey);
    if (mt.empty() || pk.empty()) {
        return {};
    }

    return {
        "anycubic/anycubicCloud/v1/printer/app/" + mt + "/" + pk + "/#",
        "anycubic/anycubicCloud/v1/printer/public/" + mt + "/" + pk + "/#",
        "anycubic/anycubicCloud/v1/+/public/" + mt + "/" + pk + "/#",
    };
}

std::optional<std::string> MqttTopicBuilder::buildPrinterPublishTopic(const std::string& machineType,
                                                                      const std::string& printerKey,
                                                                      const std::string& endpoint) {
    const std::string mt = trimAscii(machineType);
    const std::string pk = trimAscii(printerKey);
    const std::string ep = trimAscii(endpoint);
    if (mt.empty() || pk.empty() || ep.empty()) {
        return std::nullopt;
    }
    return "anycubic/anycubicCloud/v1/printer/public/" + mt + "/" + pk + "/" + ep;
}

} // namespace accloud::mqtt::routing
