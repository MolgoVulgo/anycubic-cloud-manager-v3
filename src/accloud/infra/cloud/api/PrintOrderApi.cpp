#include "PrintOrderApi.h"

#include "ApiSupport.h"

#ifdef ACCLOUD_WITH_QT
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#endif

namespace accloud::cloud::api {

CloudPrintOrderResult PrintOrderApi::send(const std::string& accessToken,
                                          const std::string& xxToken,
                                          const std::string& printerId,
                                          const std::string& fileId,
                                          bool deleteAfterPrint) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (printerId.empty()) return {false, "printer_id requis"};
    if (fileId.empty()) return {false, "file_id requis"};

    QJsonObject dataObject{
        {"file_id", QString::fromStdString(fileId)},
        {"matrix", ""},
        {"filetype", 0},
        {"project_type", 1},
        {"template_id", -2074360784}
    };

    QUrlQuery form;
    form.addQueryItem("printer_id", QString::fromStdString(printerId));
    form.addQueryItem("project_id", "0");
    form.addQueryItem("order_id", "1");
    form.addQueryItem("is_delete_file", deleteAfterPrint ? "1" : "0");
    form.addQueryItem("data",
                      QString::fromUtf8(QJsonDocument(dataObject).toJson(QJsonDocument::Compact)));

    const QByteArray body = form.query(QUrl::FullyEncoded).toUtf8();
    const auto response = support::workbenchPostForm(core::EndpointId::OrdersSend,
                                                      accessToken, xxToken, body);
    if (!response.ok) return {false, "Erreur réseau: " + response.error, {}};

    try {
        const auto json = nlohmann::json::parse(response.body);
        if (json.value("code", 0) != 1) {
            return {false, json.value("msg", "Erreur sendOrder"), {}};
        }

        std::string taskId;
        std::string msgId;
        const auto& data = json.value("data", nlohmann::json::object());
        if (data.is_object()) {
            taskId = support::jsonString(data.value("task_id", nlohmann::json{}));
            msgId = support::jsonString(data.value("msgid", nlohmann::json{}));
            if (msgId.empty()) msgId = support::jsonString(data.value("msg_id", nlohmann::json{}));
        }
        return {true, "Print order envoyée", taskId, msgId, {}, {}};
    } catch (...) {
        return {false, "Réponse invalide", {}};
    }
#endif
}

CloudPrintOrderResult PrintOrderApi::sendCommand(const std::string& accessToken,
                                                 const std::string& xxToken,
                                                 const std::string& printerId,
                                                 int orderId,
                                                 const std::string& projectId,
                                                 const std::string& dataJson) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (printerId.empty()) return {false, "printer_id requis"};
    if (orderId <= 0) return {false, "order_id invalide"};

    QUrlQuery form;
    form.addQueryItem("printer_id", QString::fromStdString(printerId));
    form.addQueryItem("project_id", QString::fromStdString(projectId.empty() ? "0" : projectId));
    form.addQueryItem("order_id", QString::number(orderId));
    form.addQueryItem("data", QString::fromStdString(dataJson));

    const QByteArray body = form.query(QUrl::FullyEncoded).toUtf8();
    const auto response = support::workbenchPostForm(core::EndpointId::OrdersSend,
                                                      accessToken, xxToken, body);
    if (!response.ok) return {false, "Erreur réseau: " + response.error, {}};

    try {
        const auto json = nlohmann::json::parse(response.body);
        if (json.value("code", 0) != 1) {
            return {false, json.value("msg", "Erreur sendOrder"), {}};
        }

        std::string taskId;
        std::string msgId;
        const auto& data = json.value("data", nlohmann::json::object());
        if (data.is_object()) {
            taskId = support::jsonString(data.value("task_id", nlohmann::json{}));
            msgId = support::jsonString(data.value("msgid", nlohmann::json{}));
            if (msgId.empty()) msgId = support::jsonString(data.value("msg_id", nlohmann::json{}));
        }
        return {true, json.value("msg", "Operation successful"), taskId, msgId, {}, {}};
    } catch (...) {
        return {false, "Réponse invalide", {}};
    }
#endif
}

} // namespace accloud::cloud::api
