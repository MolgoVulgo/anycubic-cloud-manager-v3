#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace accloud::logging::raw {

using HeaderList = std::vector<std::pair<std::string, std::string>>;

[[nodiscard]] std::string nextCorrelationId(std::string_view prefix);

void logHttpRequest(std::string_view correlationId,
                    std::string_view method,
                    std::string_view url,
                    const HeaderList& headers,
                    std::string_view body);

void logHttpResponse(std::string_view correlationId,
                     int status,
                     std::string_view reason,
                     const HeaderList& headers,
                     std::string_view body,
                     std::string_view networkError = {});

void logMqttMessage(std::string_view direction,
                    std::string_view topic,
                    std::string_view payload);

} // namespace accloud::logging::raw
