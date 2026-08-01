#include "ReasonCatalogApi.h"

#include "ApiSupport.h"

namespace accloud::cloud::api {

CloudReasonCatalogResult ReasonCatalogApi::list(const std::string& accessToken,
                                                const std::string& xxToken) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    const auto response = support::workbenchGet(core::EndpointId::ReasonCatalog,
                                                 accessToken, xxToken);
    if (!response.ok) return {false, "Erreur reseau: " + response.error};

    try {
        const auto json = nlohmann::json::parse(response.body);
        if (json.value("code", 0) != 1) {
            return {false, json.value("msg", "Erreur reason catalog")};
        }
        const auto& data = json.value("data", nlohmann::json::array());
        if (!data.is_array()) return {false, "data reason catalog invalide"};

        CloudReasonCatalogResult result;
        result.ok = true;
        result.reasons.reserve(data.size());
        for (const auto& entry : data) {
            if (!entry.is_object()) continue;
            CloudReasonCatalogItem item;
            item.reason = support::firstInt(entry, {"reason"}, 0);
            item.desc = support::firstString(entry, {"desc"});
            item.helpUrl = support::firstString(entry, {"help_url"});
            item.type = support::firstString(entry, {"type"});
            item.push = support::firstInt(entry, {"push"}, 0);
            item.popup = support::firstInt(entry, {"popup"}, 0);
            result.reasons.push_back(std::move(item));
        }
        result.message = std::to_string(result.reasons.size()) + " raison(s) chargee(s)";
        return result;
    } catch (const std::exception& error) {
        return {false, std::string("Parse error: ") + error.what()};
    }
#endif
}

} // namespace accloud::cloud::api
