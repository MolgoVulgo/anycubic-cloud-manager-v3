#pragma once

#include <optional>
#include <string>
#include <vector>

namespace accloud::mqtt::routing {

class MqttTopicBuilder {
public:
    static std::vector<std::string> buildUserReportTopics(const std::string& userId,
                                                          const std::string& userIdMd5);

    static std::vector<std::string> buildPrinterSubscriptionTopics(const std::string& machineType,
                                                                   const std::string& deviceId);

    static std::optional<std::string> buildPrinterPublishTopic(const std::string& machineType,
                                                               const std::string& deviceId,
                                                               const std::string& endpoint);
};

} // namespace accloud::mqtt::routing
