#include "AuthApi.h"

#include "ApiSupport.h"
#include "infra/cloud/core/ResponseEnvelopeParser.h"
#include "infra/logging/JsonlLogger.h"

#ifdef ACCLOUD_WITH_QT
#include <QJsonDocument>
#include <QJsonObject>
#endif

namespace accloud::cloud::api {

CloudCheckResult AuthApi::checkAuth(const std::string& accessToken,
                                    const std::string& xxToken) const {
#ifndef ACCLOUD_WITH_QT
    logging::warn("app", "cloud_client", "check_auth_unavailable",
                  "Qt non disponible, vérification cloud ignorée");
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) {
        return {false, "Pas d'access_token dans la session"};
    }

    const auto validate = support::workbenchPost(core::EndpointId::AuthCheckSession,
                                                  accessToken, xxToken, "{}");
    if (!validate.ok) {
        logging::warn("app", "cloud_client", "check_auth_network_error",
                      "Erreur réseau auth (getUserStore)", {{"error", validate.error}});
        return {false, "Erreur réseau: " + validate.error};
    }

    const core::ResponseEnvelopeParser envelopeParser;
    const core::EnvelopeParseResult validateEnvelope = envelopeParser.parse(validate.body);
    if (validateEnvelope.jsonValid && validateEnvelope.envelopePresent) {
        if (validateEnvelope.success) {
            logging::info("app", "cloud_client", "check_auth_ok",
                          "Connexion cloud validée (getUserStore)",
                          {{"http", std::to_string(validate.http)}});
            return {true, "Connexion validée"};
        }
        logging::warn("app", "cloud_client", "check_auth_validate_rejected",
                      "getUserStore rejeté, fallback loginWithAccessToken",
                      {{"code", std::to_string(validateEnvelope.code)},
                       {"msg", validateEnvelope.message}});
    } else {
        logging::warn("app", "cloud_client", "check_auth_validate_parse_error",
                      "Réponse getUserStore invalide, fallback loginWithAccessToken",
                      {{"reason", validateEnvelope.error}});
    }

    const QJsonObject payload{{"access_token", QString::fromStdString(accessToken)},
                              {"accessToken", QString::fromStdString(accessToken)},
                              {"device_type", "web"}};
    const auto login = support::workbenchPost(
        core::EndpointId::AuthLoginWithAccessToken,
        accessToken,
        xxToken,
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    if (!login.ok) {
        logging::warn("app", "cloud_client", "check_auth_login_network_error",
                      "Erreur réseau auth (loginWithAccessToken)", {{"error", login.error}});
        return {false, "Erreur réseau: " + login.error};
    }

    const core::EnvelopeParseResult loginEnvelope = envelopeParser.parse(login.body);
    if (!loginEnvelope.jsonValid || !loginEnvelope.envelopePresent) {
        return {false, "Réponse JSON invalide (HTTP " + std::to_string(login.http) + ")"};
    }
    if (loginEnvelope.success) {
        logging::info("app", "cloud_client", "check_auth_ok",
                      "Connexion cloud validée (loginWithAccessToken)",
                      {{"http", std::to_string(login.http)}});
        return {true, "Connexion validée"};
    }
    logging::warn("app", "cloud_client", "check_auth_rejected",
                  "Token rejeté", {{"code", std::to_string(loginEnvelope.code)},
                                   {"msg", loginEnvelope.message}});
    return {false, "Token rejeté (code " + std::to_string(loginEnvelope.code) + "): "
                       + loginEnvelope.message};
#endif
}

} // namespace accloud::cloud::api
