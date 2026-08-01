#include "QuotaApi.h"

#include "ApiSupport.h"

namespace accloud::cloud::api {

CloudQuotaResult QuotaApi::fetch(const std::string& accessToken,
                                 const std::string& xxToken) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    const auto response = support::workbenchPost(core::EndpointId::AuthCheckSession,
                                                  accessToken, xxToken, "{}");
    if (!response.ok) return {false, "Erreur réseau: " + response.error};

    try {
        const auto json = nlohmann::json::parse(response.body);
        if (json.value("code", 0) != 1) return {false, json.value("msg", "Erreur quota")};
        const auto& data = json.value("data", nlohmann::json::object());
        CloudQuotaResult quota;
        quota.ok = true;
        quota.totalDisplay = data.value("total", std::string{"?"});
        quota.totalBytes = data.value("total_bytes", uint64_t{0});
        quota.usedDisplay = data.value("used", std::string{"?"});
        quota.usedBytes = data.value("used_bytes", uint64_t{0});
        quota.message = "Quota chargé";
        return quota;
    } catch (const std::exception& error) {
        return {false, std::string("Parse error: ") + error.what()};
    }
#endif
}

} // namespace accloud::cloud::api
