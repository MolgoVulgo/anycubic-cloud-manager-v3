#include "DownloadsApi.h"

#include "ApiSupport.h"
#include "infra/logging/JsonlLogger.h"

#ifdef ACCLOUD_WITH_QT
#include <QJsonDocument>
#include <QJsonObject>
#endif

namespace accloud::cloud::api {

CloudDownloadResult DownloadsApi::getSignedUrl(const std::string& accessToken,
                                               const std::string& xxToken,
                                               const std::string& fileId) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    long long numericId = 0;
    try {
        numericId = std::stoll(fileId);
    } catch (...) {
    }

    const QJsonObject bodyObject{{"id", numericId}};
    const auto response = support::workbenchPost(
        core::EndpointId::FilesDownloadUrl,
        accessToken,
        xxToken,
        QJsonDocument(bodyObject).toJson(QJsonDocument::Compact));
    if (!response.ok) return {false, "Erreur réseau: " + response.error};

    try {
        const auto json = nlohmann::json::parse(response.body);
        if (json.value("code", 0) != 1) {
            return {false, json.value("msg", "Erreur URL download")};
        }

        std::string url;
        const auto& data = json["data"];
        if (data.is_string()) {
            url = data.get<std::string>();
        } else if (data.is_object() && data.contains("url")) {
            url = data["url"].get<std::string>();
        }
        if (url.empty()) return {false, "URL non trouvée dans la réponse"};

        logging::info("app", "cloud_client", "get_download_url_ok",
                      "URL de téléchargement obtenue");
        return {true, "URL obtenue", url};
    } catch (...) {
        return {false, "Réponse invalide"};
    }
#endif
}

} // namespace accloud::cloud::api
