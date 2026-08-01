#pragma once

#include <nlohmann/json.hpp>

#include <initializer_list>
#include <string>

#ifdef ACCLOUD_WITH_QT
#include "infra/cloud/core/EndpointRegistry.h"

#include <QByteArray>
#include <QString>
#endif

namespace accloud::cloud::api::support {

#ifdef ACCLOUD_WITH_QT

struct RawResponse {
    bool ok{false};
    int http{0};
    std::string body;
    std::string error;
};

RawResponse workbenchCall(core::EndpointId endpointId,
                          const std::string& accessToken,
                          const std::string& xxToken,
                          const QByteArray& body = {},
                          const QString& queryString = {});
RawResponse workbenchPost(core::EndpointId endpointId,
                          const std::string& accessToken,
                          const std::string& xxToken,
                          const QByteArray& jsonBody);
RawResponse workbenchPostForm(core::EndpointId endpointId,
                              const std::string& accessToken,
                              const std::string& xxToken,
                              const QByteArray& formBody);
RawResponse workbenchGet(core::EndpointId endpointId,
                         const std::string& accessToken,
                         const std::string& xxToken,
                         const QString& queryString = {});

#endif

std::string jsonString(const nlohmann::json& value);
std::string firstString(const nlohmann::json& object,
                        std::initializer_list<const char*> keys);
int jsonInt(const nlohmann::json& value, int fallback = 0);
int firstInt(const nlohmann::json& object,
             std::initializer_list<const char*> keys,
             int fallback = 0);
long long jsonLong(const nlohmann::json& value, long long fallback = 0);
long long firstLong(const nlohmann::json& object,
                    std::initializer_list<const char*> keys,
                    long long fallback = 0);
long long normalizeEpochSeconds(long long epoch);
std::string joinJsonStringArray(const nlohmann::json& value);
bool containsNoCase(const std::string& text, const std::string& needle);
std::string formatSeconds(long long seconds);
int durationSecondsFromObject(const nlohmann::json& object,
                              std::initializer_list<const char*> secondKeys,
                              std::initializer_list<const char*> minuteKeys);
nlohmann::json objectOrParsedString(const nlohmann::json& parent, const char* key);

} // namespace accloud::cloud::api::support
