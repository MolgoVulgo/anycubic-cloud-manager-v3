#include "UploadsApi.h"

#include "infra/cloud/core/EndpointRegistry.h"
#include "infra/cloud/core/HttpClient.h"
#include "infra/cloud/core/ResponseEnvelopeParser.h"
#include "infra/cloud/core/WorkbenchRequestBuilder.h"
#include "infra/logging/JsonlLogger.h"

#ifdef ACCLOUD_WITH_QT

#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#endif

#include <algorithm>
#include <cmath>
#include <utility>

namespace accloud::cloud::api {
namespace {

#ifdef ACCLOUD_WITH_QT

struct WorkbenchCallResult {
    bool ok{false};
    int httpStatus{0};
    std::string body;
    std::string error;
};

constexpr int kPutMinTimeoutMs = 120000;
constexpr int kPutMaxTimeoutMs = 3600000;
constexpr double kPutAssumedMinThroughputBps = 256.0 * 1024.0;
constexpr double kPutSafetyFactor = 4.0;

int computePutTransferTimeoutMs(qint64 sizeBytes) {
    if (sizeBytes <= 0) {
        return kPutMinTimeoutMs;
    }
    const double estimatedSeconds = (static_cast<double>(sizeBytes) / kPutAssumedMinThroughputBps)
                                    * kPutSafetyFactor;
    const int estimatedMs = static_cast<int>(std::ceil(estimatedSeconds * 1000.0));
    return std::clamp(estimatedMs, kPutMinTimeoutMs, kPutMaxTimeoutMs);
}

std::string jsonToString(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_number_integer()) {
        return std::to_string(value.get<long long>());
    }
    if (value.is_number_unsigned()) {
        return std::to_string(value.get<unsigned long long>());
    }
    if (value.is_number_float()) {
        return std::to_string(value.get<double>());
    }
    return {};
}

QByteArray jsonWithId(const char* key, const std::string& id) {
    QJsonObject payload;
    bool asNumberOk = false;
    const qlonglong numericId = QString::fromStdString(id).toLongLong(&asNumberOk);
    if (asNumberOk) {
        payload.insert(QString::fromUtf8(key), static_cast<qint64>(numericId));
    } else {
        payload.insert(QString::fromUtf8(key), QString::fromStdString(id));
    }
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

WorkbenchCallResult callWorkbenchPost(core::EndpointId endpointId,
                                      const std::string& accessToken,
                                      const std::string& xxToken,
                                      const QByteArray& body) {
    const auto endpoint = core::EndpointRegistry::instance().find(endpointId);
    if (!endpoint.has_value()) {
        return {false, 0, {}, "Unknown endpoint id"};
    }

    const core::WorkbenchRequestBuilder builder;
    const auto built = builder.build(*endpoint, accessToken, xxToken, {}, body);
    if (!built.has_value()) {
        return {false, 0, {}, "Failed to build request"};
    }

    const core::HttpClient client;
    const core::HttpResponse response = client.execute(*built);
    return {response.ok, response.httpStatus, response.body, response.error};
}

std::string envelopeErrorMessage(const core::EnvelopeParseResult& envelope,
                                 int httpStatus,
                                 const std::string& fallback) {
    if (!envelope.message.empty()) {
        return envelope.message;
    }
    if (!envelope.error.empty()) {
        return envelope.error;
    }
    if (!fallback.empty()) {
        return fallback;
    }
    return "HTTP " + std::to_string(httpStatus);
}

#endif

} // namespace

CloudUploadLockResult UploadsApi::lockStorageSpace(const std::string& accessToken,
                                                   const std::string& xxToken,
                                                   const std::string& fileName,
                                                   std::uint64_t sizeBytes,
                                                   bool isTempFile) const {
#ifndef ACCLOUD_WITH_QT
    (void)accessToken;
    (void)xxToken;
    (void)fileName;
    (void)sizeBytes;
    (void)isTempFile;
    return {false, "Qt non disponible", {}, {}, {}};
#else
    if (accessToken.empty()) {
        return {false, "Pas d'access_token", {}, {}, {}};
    }
    if (fileName.empty()) {
        return {false, "Nom de fichier vide", {}, {}, {}};
    }
    if (sizeBytes == 0) {
        return {false, "Taille de fichier invalide", {}, {}, {}};
    }

    logging::info("cloud", "uploads_api", "lock_storage_start",
                  "Request lockStorageSpace",
                  {{"file_name", fileName},
                   {"size_bytes", std::to_string(sizeBytes)},
                   {"is_temp_file", isTempFile ? "1" : "0"}});

    QJsonObject payload;
    payload.insert("name", QString::fromStdString(fileName));
    payload.insert("size", static_cast<qint64>(sizeBytes));
    payload.insert("is_temp_file", isTempFile ? 1 : 0);

    const auto response = callWorkbenchPost(core::EndpointId::UploadLockStorage,
                                            accessToken,
                                            xxToken,
                                            QJsonDocument(payload).toJson(QJsonDocument::Compact));
    if (!response.ok) {
        logging::warn("app", "uploads_api", "lock_storage_network_error",
                      "lockStorageSpace network error",
                      {{"error", response.error},
                       {"http", std::to_string(response.httpStatus)}});
        return {false, "Erreur réseau: " + response.error, {}, {}, {}};
    }

    const core::ResponseEnvelopeParser parser;
    const core::EnvelopeParseResult envelope = parser.parse(response.body);
    if (!envelope.jsonValid || !envelope.envelopePresent || !envelope.success) {
        logging::warn("cloud", "uploads_api", "lock_storage_rejected",
                      "lockStorageSpace rejected",
                      {{"http", std::to_string(response.httpStatus)},
                       {"code", std::to_string(envelope.code)},
                       {"reason", envelope.error}});
        return {false, envelopeErrorMessage(envelope, response.httpStatus, "Echec lockStorageSpace"), {}, {}, {}};
    }
    if (!envelope.data.is_object()) {
        return {false, "Réponse lockStorageSpace invalide", {}, {}, {}};
    }

    const std::string lockId = jsonToString(envelope.data.value("id", nlohmann::json{}));
    const std::string preSignUrl = jsonToString(envelope.data.value("preSignUrl", nlohmann::json{}));
    const std::string uploadUrl = jsonToString(envelope.data.value("url", nlohmann::json{}));
    if (lockId.empty() || preSignUrl.empty()) {
        logging::warn("cloud", "uploads_api", "lock_storage_incomplete",
                      "lockStorageSpace response incomplete",
                      {{"http", std::to_string(response.httpStatus)}});
        return {false, "Réponse lockStorageSpace incomplète", {}, {}, {}};
    }

    logging::info("cloud", "uploads_api", "lock_storage_ok",
                  "lockStorageSpace succeeded",
                  {{"http", std::to_string(response.httpStatus)},
                   {"lock_id", lockId}});
    return {true, "Storage locked", lockId, preSignUrl, uploadUrl};
#endif
}

CloudUploadPutResult UploadsApi::putPresigned(const std::string& preSignUrl,
                                              const std::filesystem::path& localFilePath) const {
#ifndef ACCLOUD_WITH_QT
    (void)preSignUrl;
    (void)localFilePath;
    return {false, "Qt non disponible", 0};
#else
    if (preSignUrl.empty()) {
        return {false, "preSignUrl vide", 0};
    }
    if (localFilePath.empty()) {
        return {false, "Chemin de fichier vide", 0};
    }

    QFile uploadFile(QString::fromStdString(localFilePath.string()));
    if (!uploadFile.exists()) {
        return {false, "Fichier local introuvable", 0};
    }
    if (!uploadFile.open(QIODevice::ReadOnly)) {
        return {false, "Impossible d'ouvrir le fichier local", 0};
    }

    const qint64 uploadSizeBytes = uploadFile.size();
    const int transferTimeoutMs = computePutTransferTimeoutMs(uploadSizeBytes);
    logging::info("cloud", "uploads_api", "put_presigned_start",
                  "Start binary upload to presigned URL",
                  {{"file_name", localFilePath.filename().string()},
                   {"size_bytes", std::to_string(static_cast<std::uint64_t>(uploadSizeBytes))},
                   {"timeout_ms", std::to_string(transferTimeoutMs)}});

    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(QString::fromStdString(preSignUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    req.setTransferTimeout(transferTimeoutMs);

    QEventLoop loop;
    QNetworkReply* reply = nam.put(req, &uploadFile);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool networkOk = (reply->error() == QNetworkReply::NoError);
    const bool httpOk = (httpStatus >= 200 && httpStatus < 300);
    const bool ok = networkOk && httpOk;
    const std::string replyError = reply->errorString().toStdString();
    reply->deleteLater();

    if (!ok) {
        if (!networkOk) {
            logging::warn("cloud", "uploads_api", "put_presigned_network_error",
                          "Binary upload failed (network)",
                          {{"http", std::to_string(httpStatus)},
                           {"error", replyError}});
            return {false, "Erreur upload binaire: " + replyError, httpStatus};
        }
        logging::warn("cloud", "uploads_api", "put_presigned_http_error",
                      "Binary upload failed (http)",
                      {{"http", std::to_string(httpStatus)}});
        return {false, "HTTP upload binaire: " + std::to_string(httpStatus), httpStatus};
    }

    logging::info("cloud", "uploads_api", "put_presigned_ok",
                  "Binary upload completed",
                  {{"http", std::to_string(httpStatus)}});
    return {true, "Binary upload completed", httpStatus};
#endif
}

CloudUploadRegisterResult UploadsApi::registerUploadedFile(const std::string& accessToken,
                                                           const std::string& xxToken,
                                                           const std::string& lockId) const {
#ifndef ACCLOUD_WITH_QT
    (void)accessToken;
    (void)xxToken;
    (void)lockId;
    return {false, "Qt non disponible", {}};
#else
    if (accessToken.empty()) {
        return {false, "Pas d'access_token", {}};
    }
    if (lockId.empty()) {
        return {false, "lock_id requis", {}};
    }

    logging::info("cloud", "uploads_api", "register_upload_start",
                  "Request newUploadFile",
                  {{"lock_id", lockId}});

    QJsonObject payload;
    bool asNumberOk = false;
    const qlonglong numericId = QString::fromStdString(lockId).toLongLong(&asNumberOk);
    if (asNumberOk) {
        payload.insert("user_lock_space_id", static_cast<qint64>(numericId));
    } else {
        payload.insert("user_lock_space_id", QString::fromStdString(lockId));
    }

    const auto response = callWorkbenchPost(core::EndpointId::UploadRegisterFile,
                                            accessToken,
                                            xxToken,
                                            QJsonDocument(payload).toJson(QJsonDocument::Compact));
    if (!response.ok) {
        logging::warn("cloud", "uploads_api", "register_upload_network_error",
                      "newUploadFile network error",
                      {{"http", std::to_string(response.httpStatus)},
                       {"error", response.error}});
        return {false, "Erreur réseau: " + response.error, {}};
    }

    const core::ResponseEnvelopeParser parser;
    const core::EnvelopeParseResult envelope = parser.parse(response.body);
    if (!envelope.jsonValid || !envelope.envelopePresent || !envelope.success) {
        logging::warn("cloud", "uploads_api", "register_upload_rejected",
                      "newUploadFile rejected",
                      {{"http", std::to_string(response.httpStatus)},
                       {"code", std::to_string(envelope.code)},
                       {"reason", envelope.error}});
        return {false, envelopeErrorMessage(envelope, response.httpStatus, "Echec newUploadFile"), {}};
    }
    if (!envelope.data.is_object()) {
        return {false, "Réponse newUploadFile invalide", {}};
    }

    const std::string fileId = jsonToString(envelope.data.value("id", nlohmann::json{}));
    if (fileId.empty()) {
        logging::warn("cloud", "uploads_api", "register_upload_missing_file_id",
                      "newUploadFile returned without file id",
                      {{"http", std::to_string(response.httpStatus)},
                       {"lock_id", lockId}});
        return {false, "file_id manquant après upload", {}};
    }
    logging::info("cloud", "uploads_api", "register_upload_ok",
                  "newUploadFile succeeded",
                  {{"http", std::to_string(response.httpStatus)},
                   {"file_id", fileId}});
    return {true, "Upload registered", fileId};
#endif
}

CloudUploadStatusResult UploadsApi::getUploadStatus(const std::string& accessToken,
                                                    const std::string& xxToken,
                                                    const std::string& fileId) const {
#ifndef ACCLOUD_WITH_QT
    (void)accessToken;
    (void)xxToken;
    (void)fileId;
    return {false, "Qt non disponible", {}, 0, {}};
#else
    if (accessToken.empty()) {
        return {false, "Pas d'access_token", {}, 0, {}};
    }
    if (fileId.empty()) {
        return {false, "file_id requis", {}, 0, {}};
    }

    logging::info("cloud", "uploads_api", "upload_status_start",
                  "Request getUploadStatus",
                  {{"file_id", fileId}});

    const auto response = callWorkbenchPost(core::EndpointId::UploadStatus,
                                            accessToken,
                                            xxToken,
                                            jsonWithId("id", fileId));
    if (!response.ok) {
        logging::warn("cloud", "uploads_api", "upload_status_network_error",
                      "getUploadStatus network error",
                      {{"http", std::to_string(response.httpStatus)},
                       {"error", response.error},
                       {"file_id", fileId}});
        return {false, "Erreur réseau: " + response.error, {}, 0, {}};
    }

    const core::ResponseEnvelopeParser parser;
    const core::EnvelopeParseResult envelope = parser.parse(response.body);
    if (!envelope.jsonValid || !envelope.envelopePresent || !envelope.success) {
        logging::warn("cloud", "uploads_api", "upload_status_rejected",
                      "getUploadStatus rejected",
                      {{"http", std::to_string(response.httpStatus)},
                       {"code", std::to_string(envelope.code)},
                       {"reason", envelope.error},
                       {"file_id", fileId}});
        return {false, envelopeErrorMessage(envelope, response.httpStatus, "Echec getUploadStatus"), {}, 0, {}};
    }
    if (!envelope.data.is_object()) {
        return {false, "Réponse getUploadStatus invalide", {}, 0, {}};
    }

    CloudUploadStatusResult result;
    result.ok = true;
    result.message = "Upload status loaded";
    result.fileId = jsonToString(envelope.data.value("id", nlohmann::json{}));
    if (envelope.data.contains("status")) {
        if (envelope.data["status"].is_number_integer()) {
            result.status = envelope.data["status"].get<int>();
        } else if (envelope.data["status"].is_string()) {
            try {
                result.status = std::stoi(envelope.data["status"].get<std::string>());
            } catch (...) {
                result.status = 0;
            }
        }
    }
    result.gcodeId = jsonToString(envelope.data.value("gcode_id", nlohmann::json{}));
    if (result.gcodeId.empty()) {
        result.gcodeId = jsonToString(envelope.data.value("gcodeId", nlohmann::json{}));
    }
    logging::info("cloud", "uploads_api", "upload_status_ok",
                  "getUploadStatus succeeded",
                  {{"http", std::to_string(response.httpStatus)},
                   {"file_id", fileId},
                   {"status", std::to_string(result.status)},
                   {"gcode_id", result.gcodeId.empty() ? "0" : result.gcodeId}});
    return result;
#endif
}

CloudUploadUnlockResult UploadsApi::unlockStorageSpace(const std::string& accessToken,
                                                       const std::string& xxToken,
                                                       const std::string& lockId,
                                                       bool deleteCosOnFailure) const {
#ifndef ACCLOUD_WITH_QT
    (void)accessToken;
    (void)xxToken;
    (void)lockId;
    (void)deleteCosOnFailure;
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) {
        return {false, "Pas d'access_token"};
    }
    if (lockId.empty()) {
        return {false, "lock_id requis"};
    }

    logging::info("cloud", "uploads_api", "unlock_storage_start",
                  "Request unlockStorageSpace",
                  {{"lock_id", lockId},
                   {"is_delete_cos", deleteCosOnFailure ? "1" : "0"}});

    QJsonObject payload;
    bool asNumberOk = false;
    const qlonglong numericId = QString::fromStdString(lockId).toLongLong(&asNumberOk);
    if (asNumberOk) {
        payload.insert("id", static_cast<qint64>(numericId));
    } else {
        payload.insert("id", QString::fromStdString(lockId));
    }
    payload.insert("is_delete_cos", deleteCosOnFailure ? 1 : 0);

    const auto response = callWorkbenchPost(core::EndpointId::UploadUnlockStorage,
                                            accessToken,
                                            xxToken,
                                            QJsonDocument(payload).toJson(QJsonDocument::Compact));
    if (!response.ok) {
        logging::warn("cloud", "uploads_api", "unlock_storage_network_error",
                      "unlockStorageSpace network error",
                      {{"http", std::to_string(response.httpStatus)},
                       {"error", response.error},
                       {"lock_id", lockId}});
        return {false, "Erreur réseau: " + response.error};
    }

    const core::ResponseEnvelopeParser parser;
    const core::EnvelopeParseResult envelope = parser.parse(response.body);
    if (!envelope.jsonValid || !envelope.envelopePresent || !envelope.success) {
        logging::warn("cloud", "uploads_api", "unlock_storage_rejected",
                      "unlockStorageSpace rejected",
                      {{"http", std::to_string(response.httpStatus)},
                       {"code", std::to_string(envelope.code)},
                       {"reason", envelope.error},
                       {"lock_id", lockId}});
        return {false, envelopeErrorMessage(envelope, response.httpStatus, "Echec unlockStorageSpace")};
    }

    logging::info("cloud", "uploads_api", "unlock_storage_ok",
                  "unlockStorageSpace succeeded",
                  {{"http", std::to_string(response.httpStatus)},
                   {"lock_id", lockId}});
    return {true, "Storage unlocked"};
#endif
}

} // namespace accloud::cloud::api
