#include "CloudClient.h"
#include "SignHeaders.h"
#include "infra/logging/JsonlLogger.h"

#ifdef ACCLOUD_WITH_QT
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#endif

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace accloud::cloud {
namespace {

#ifdef ACCLOUD_WITH_QT

// ── Helpers Qt / réseau ───────────────────────────────────────────────────

struct RawResp {
    bool        ok    = false;
    int         http  = 0;
    std::string body;
    std::string error;
};

void applyWorkbenchHeadersBestEffort(QNetworkRequest& req, const std::string& xxToken) {
    const WorkbenchHeaderStatus headerStatus = applyWorkbenchHeaders(req, QString::fromStdString(xxToken));
    if (!headerStatus.ok) {
        logging::warn("app", "cloud_client", "workbench_headers_missing",
                      "Workbench signature headers are not configured; request sent without XX-*",
                      {{"reason", headerStatus.error.toStdString()}});
    }
}

// Effectue un POST Workbench synchrone (QEventLoop)
RawResp workbenchPost(const char* path, const std::string& accessToken,
                      const std::string& xxToken, const QByteArray& jsonBody) {
    QUrl url(QString("https://cloud-universe.anycubic.com") + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + QByteArray::fromStdString(accessToken));
    req.setTransferTimeout(10000);
    applyWorkbenchHeadersBestEffort(req, xxToken);

    QNetworkAccessManager nam;
    QEventLoop loop;
    QNetworkReply* reply = nam.post(req, jsonBody);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    RawResp r;
    r.http  = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.error = reply->errorString().toStdString();
    r.body  = reply->readAll().toStdString();
    r.ok    = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();
    return r;
}

RawResp workbenchPostForm(const char* path, const std::string& accessToken,
                          const std::string& xxToken, const QByteArray& formBody) {
    QUrl url(QString("https://cloud-universe.anycubic.com") + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + QByteArray::fromStdString(accessToken));
    req.setTransferTimeout(10000);
    applyWorkbenchHeadersBestEffort(req, xxToken);

    QNetworkAccessManager nam;
    QEventLoop loop;
    QNetworkReply* reply = nam.post(req, formBody);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    RawResp r;
    r.http  = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.error = reply->errorString().toStdString();
    r.body  = reply->readAll().toStdString();
    r.ok    = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();
    return r;
}

RawResp workbenchGet(const char* path, const std::string& accessToken,
                     const std::string& xxToken) {
    QUrl url(QString("https://cloud-universe.anycubic.com") + path);
    QNetworkRequest req(url);
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + QByteArray::fromStdString(accessToken));
    req.setTransferTimeout(10000);
    applyWorkbenchHeadersBestEffort(req, xxToken);

    QNetworkAccessManager nam;
    QEventLoop loop;
    QNetworkReply* reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    RawResp r;
    r.http  = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.error = reply->errorString().toStdString();
    r.body  = reply->readAll().toStdString();
    r.ok    = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();
    return r;
}

// ── Helpers JSON ──────────────────────────────────────────────────────────

std::string jStr(const nlohmann::json& v) {
    if (v.is_string())         return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_number_float()) {
        std::ostringstream ss;
        ss << v.get<double>();
        return ss.str();
    }
    return {};
}

std::string jFirst(const nlohmann::json& obj,
                   std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        if (!obj.contains(k)) continue;
        const auto s = jStr(obj[k]);
        if (!s.empty()) return s;
    }
    return {};
}

int jInt(const nlohmann::json& v, int fallback = 0) {
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_number_float())   return static_cast<int>(v.get<double>());
    if (v.is_boolean())        return v.get<bool>() ? 1 : 0;
    if (v.is_string()) {
        try { return std::stoi(v.get<std::string>()); }
        catch (...) { return fallback; }
    }
    return fallback;
}

int jFirstInt(const nlohmann::json& obj,
              std::initializer_list<const char*> keys, int fallback = 0) {
    for (const char* k : keys) {
        if (!obj.contains(k)) continue;
        return jInt(obj[k], fallback);
    }
    return fallback;
}

bool containsNoCase(const std::string& text, const std::string& needle) {
    std::string left = text;
    std::string right = needle;
    std::transform(left.begin(), left.end(), left.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(right.begin(), right.end(), right.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return left.find(right) != std::string::npos;
}

std::string formatSeconds(long long secs) {
    if (secs <= 0) return {};
    const int h = static_cast<int>(secs / 3600);
    const int m = static_cast<int>((secs % 3600) / 60);
    const int s = static_cast<int>(secs % 60);
    if (h > 0)
        return std::to_string(h) + "h " + std::to_string(m) + "m";
    return std::to_string(m) + "m " + std::to_string(s) + "s";
}

// Parse slice_param (objet JSON ou string JSON)
nlohmann::json parseSliceParam(const nlohmann::json& entry) {
    if (!entry.contains("slice_param")) return {};
    const auto& sp = entry["slice_param"];
    if (sp.is_object()) return sp;
    if (sp.is_string()) {
        auto parsed = nlohmann::json::parse(sp.get<std::string>(), nullptr, false);
        if (!parsed.is_discarded()) return parsed;
    }
    return {};
}

CloudFileInfo parseFileEntry(const nlohmann::json& e) {
    CloudFileInfo f;
    f.id           = jStr(e.value("id", nlohmann::json{}));
    f.name         = e.value("old_filename", std::string{});
    if (f.name.empty())
        f.name     = e.value("filename", std::string{});
    f.sizeBytes    = e.value("size", uint64_t{0});
    f.gcodeId      = jStr(e.value("gcode_id", nlohmann::json{}));
    f.thumbnailUrl = e.value("thumbnail", std::string{});
    f.status       = e.value("status", 0);

    const nlohmann::json sp = parseSliceParam(e);
    if (sp.is_object()) {
        f.machine  = jFirst(sp, {"machineName", "machine_name", "machineType"});
        f.material = jFirst(sp, {"materialName", "material_name", "resinType", "material"});

        const auto ptRaw = jFirst(sp, {"printTime", "print_time", "estimate", "totalTime"});
        if (!ptRaw.empty()) {
            try { f.printTime = formatSeconds(std::stoll(ptRaw)); }
            catch (...) { f.printTime = ptRaw; }
        }

        auto lh = jFirst(sp, {"layerHeight", "layer_height", "sliceHeight", "normalLayerHeight"});
        if (!lh.empty()) {
            if (lh.find("mm") == std::string::npos) lh += " mm";
            f.layerHeight = lh;
        }

        f.layers = jFirst(sp, {"layerCount", "layer_count", "layers", "totalLayers"});

        auto rv = jFirst(sp, {"resinVolume", "resin_volume", "resinUsage", "weight"});
        if (!rv.empty()) {
            if (rv.find("ml") == std::string::npos && rv.find("g") == std::string::npos)
                rv += " ml";
            f.resinUsage = rv;
        }

        const auto sx = jFirst(sp, {"sizeX", "x", "width"});
        const auto sy = jFirst(sp, {"sizeY", "y", "height"});
        const auto sz = jFirst(sp, {"sizeZ", "z", "depth"});
        if (!sx.empty() && !sy.empty() && !sz.empty())
            f.dimensions = sx + "x" + sy + "x" + sz;
        else
            f.dimensions = jFirst(sp, {"dimensions", "boundingBox"});
    }
    return f;
}

std::vector<CloudFileInfo> parseFileArray(const nlohmann::json& data) {
    if (!data.is_array()) return {};
    std::vector<CloudFileInfo> out;
    out.reserve(data.size());
    for (const auto& e : data) {
        if (e.is_object()) out.push_back(parseFileEntry(e));
    }
    return out;
}

CloudPrinterInfo parsePrinterEntry(const nlohmann::json& e) {
    CloudPrinterInfo p;
    p.id = jStr(e.value("id", nlohmann::json{}));
    p.name = jFirst(e, {"name", "printer_name", "device_name"});
    p.model = jFirst(e, {"model", "model_name", "machine_name", "machineType"});
    p.reason = jFirst(e, {"reason", "status_text"});
    p.available = jFirstInt(e, {"available"}, 0);

    const int status = jFirstInt(e, {"status", "device_status"}, 0);
    const int isPrinting = jFirstInt(e, {"is_printing"}, 0);

    nlohmann::json deviceMessage;
    if (e.contains("device_message")) {
        const auto& dm = e["device_message"];
        if (dm.is_object()) {
            deviceMessage = dm;
        } else if (dm.is_string()) {
            auto parsed = nlohmann::json::parse(dm.get<std::string>(), nullptr, false);
            if (!parsed.is_discarded() && parsed.is_object()) deviceMessage = parsed;
        }
    }

    nlohmann::json project;
    if (e.contains("project")) {
        const auto& pr = e["project"];
        if (pr.is_object()) {
            project = pr;
        } else if (pr.is_string()) {
            auto parsed = nlohmann::json::parse(pr.get<std::string>(), nullptr, false);
            if (!parsed.is_discarded() && parsed.is_object()) project = parsed;
        }
    }

    p.progress = jFirstInt(deviceMessage, {"progress"}, -1);
    if (p.progress < 0) p.progress = jFirstInt(project, {"progress"}, -1);
    p.remainingSec = jFirstInt(deviceMessage, {"remaining_sec", "remain_time"}, -1);
    if (p.remainingSec < 0) p.remainingSec = jFirstInt(project, {"remain_time"}, -1);
    p.elapsedSec = jFirstInt(project, {"elapsed_time"}, -1);
    p.currentFile = jFirst(project, {"old_filename", "filename", "file_name"});
    if (p.currentFile.empty()) p.currentFile = jFirst(deviceMessage, {"file_name", "filename"});

    bool isOffline = false;
    if (status == 0 || status == 3 || p.available == 0) isOffline = true;
    if (containsNoCase(p.reason, "offline")) isOffline = true;

    bool isBusyPrinting = (isPrinting == 1)
                       || (p.progress >= 0 && p.progress < 100)
                       || containsNoCase(jFirst(deviceMessage, {"action"}), "start");

    if (isOffline) {
        p.state = "OFFLINE";
    } else if (isBusyPrinting) {
        p.state = "PRINTING";
    } else if (containsNoCase(p.reason, "error")) {
        p.state = "ERROR";
    } else {
        p.state = "READY";
    }

    return p;
}

CloudPrinterCompatItem parsePrinterCompatEntry(const nlohmann::json& e) {
    CloudPrinterCompatItem item;
    item.id = jStr(e.value("id", nlohmann::json{}));
    item.available = jFirstInt(e, {"available"}, 0);
    item.reason = jFirst(e, {"reason"});
    return item;
}

#endif // ACCLOUD_WITH_QT

} // namespace

// ── checkCloudAuth ────────────────────────────────────────────────────────

CloudCheckResult checkCloudAuth(const std::string& accessToken,
                                const std::string& xxToken) {
#ifndef ACCLOUD_WITH_QT
    logging::warn("app", "cloud_client", "check_auth_unavailable",
                  "Qt non disponible, vérification cloud ignorée");
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty())
        return {false, "Pas d'access_token dans la session"};

    // Vérification primaire (session validate): getUserStore
    const auto validate = workbenchPost("/p/p/workbench/api/work/index/getUserStore",
                                        accessToken, xxToken, "{}");
    if (!validate.ok) {
        logging::warn("app", "cloud_client", "check_auth_network_error",
                      "Erreur réseau auth (getUserStore)", {{"error", validate.error}});
        return {false, "Erreur réseau: " + validate.error};
    }

    try {
        const auto j    = nlohmann::json::parse(validate.body);
        const int  code = j.value("code", 0);
        const auto msg  = j.value("msg", std::string{});
        if (code == 1) {
            logging::info("app", "cloud_client", "check_auth_ok",
                          "Connexion cloud validée (getUserStore)",
                          {{"http", std::to_string(validate.http)}});
            return {true, "Connexion validée"};
        }
        logging::warn("app", "cloud_client", "check_auth_validate_rejected",
                      "getUserStore rejeté, fallback loginWithAccessToken",
                      {{"code", std::to_string(code)}, {"msg", msg}});
    } catch (const std::exception& e) {
        logging::warn("app", "cloud_client", "check_auth_validate_parse_error",
                      "Réponse getUserStore invalide, fallback loginWithAccessToken",
                      {{"reason", e.what()}});
    }

    // Fallback (Docs §1.4): loginWithAccessToken
    const QJsonObject payload{{"access_token", QString::fromStdString(accessToken)},
                              {"accessToken",  QString::fromStdString(accessToken)},
                              {"device_type",  "web"}};
    const auto login = workbenchPost(
        "/p/p/workbench/api/v3/public/loginWithAccessToken",
        accessToken, xxToken,
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    if (!login.ok) {
        logging::warn("app", "cloud_client", "check_auth_login_network_error",
                      "Erreur réseau auth (loginWithAccessToken)", {{"error", login.error}});
        return {false, "Erreur réseau: " + login.error};
    }

    try {
        const auto j    = nlohmann::json::parse(login.body);
        const int  code = j.value("code", 0);
        const auto msg  = j.value("msg", std::string{});
        if (code == 1) {
            logging::info("app", "cloud_client", "check_auth_ok",
                          "Connexion cloud validée (loginWithAccessToken)",
                          {{"http", std::to_string(login.http)}});
            return {true, "Connexion validée"};
        }
        logging::warn("app", "cloud_client", "check_auth_rejected",
                      "Token rejeté", {{"code", std::to_string(code)}, {"msg", msg}});
        return {false, "Token rejeté (code " + std::to_string(code) + "): " + msg};
    } catch (...) {
        return {false, "Réponse JSON invalide (HTTP " + std::to_string(login.http) + ")"};
    }
#endif
}

// ── fetchCloudFiles ───────────────────────────────────────────────────────

CloudFilesResult fetchCloudFiles(const std::string& accessToken,
                                 const std::string& xxToken,
                                 int page, int limit) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    const QJsonObject bodyObj{{"page", page}, {"limit", limit}};
    const auto bodyBytes = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    // Essai principal : /files
    auto r = workbenchPost("/p/p/workbench/api/work/index/files",
                           accessToken, xxToken, bodyBytes);
    if (!r.ok) {
        logging::warn("app", "cloud_client", "fetch_files_network_error",
                      "Erreur réseau listing", {{"error", r.error}});
        return {false, "Erreur réseau: " + r.error};
    }

    auto tryParse = [&](const std::string& body) -> CloudFilesResult {
        const auto j = nlohmann::json::parse(body);
        CloudFilesResult res;
        res.ok = (j.value("code", 0) == 1);
        if (!res.ok) { res.message = j.value("msg", "Erreur serveur"); return res; }
        if (j.contains("data") && j["data"].is_array()) {
            res.files   = parseFileArray(j["data"]);
            res.total   = static_cast<int>(res.files.size());
            res.message = std::to_string(res.total) + " fichier(s)";
        } else {
            res.ok = false; res.message = "data invalide";
        }
        return res;
    };

    try {
        auto res = tryParse(r.body);
        if (res.ok) {
            logging::info("app", "cloud_client", "fetch_files_ok",
                          "Listing OK", {{"count", std::to_string(res.total)}});
            return res;
        }
        // Fallback : /userFiles
        logging::info("app", "cloud_client", "fetch_files_fallback", "Fallback userFiles");
        r = workbenchPost("/p/p/workbench/api/work/index/userFiles",
                          accessToken, xxToken, bodyBytes);
        if (!r.ok) return {false, "Erreur réseau: " + r.error};
        return tryParse(r.body);
    } catch (const std::exception& e) {
        return {false, std::string("Parse error: ") + e.what()};
    }
#endif
}

// ── fetchCloudQuota ───────────────────────────────────────────────────────

CloudQuotaResult fetchCloudQuota(const std::string& accessToken,
                                 const std::string& xxToken) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    const auto r = workbenchPost(
        "/p/p/workbench/api/work/index/getUserStore",
        accessToken, xxToken, "{}");
    if (!r.ok) return {false, "Erreur réseau: " + r.error};

    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1)
            return {false, j.value("msg", "Erreur quota")};
        const auto& d = j.value("data", nlohmann::json::object());
        CloudQuotaResult q;
        q.ok           = true;
        q.totalDisplay = d.value("total",       std::string{"?"});
        q.totalBytes   = d.value("total_bytes", uint64_t{0});
        q.usedDisplay  = d.value("used",        std::string{"?"});
        q.usedBytes    = d.value("used_bytes",  uint64_t{0});
        q.message      = "Quota chargé";
        return q;
    } catch (const std::exception& e) {
        return {false, std::string("Parse error: ") + e.what()};
    }
#endif
}

// ── deleteCloudFile ───────────────────────────────────────────────────────

CloudOpResult deleteCloudFile(const std::string& accessToken,
                              const std::string& xxToken,
                              const std::string& fileId) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    long long idInt = 0;
    try { idInt = std::stoll(fileId); } catch (...) {}

    const QJsonObject bodyObj{{"idArr", QJsonArray{idInt}}};
    const auto r = workbenchPost(
        "/p/p/workbench/api/work/index/delFiles",
        accessToken, xxToken,
        QJsonDocument(bodyObj).toJson(QJsonDocument::Compact));
    if (!r.ok) return {false, "Erreur réseau: " + r.error};

    try {
        const auto j = nlohmann::json::parse(r.body);
        const bool ok = (j.value("code", 0) == 1);
        return {ok, ok ? "Fichier supprimé" : j.value("msg", "Erreur suppression")};
    } catch (...) { return {false, "Réponse invalide"}; }
#endif
}

// ── getCloudDownloadUrl ───────────────────────────────────────────────────

CloudDownloadResult getCloudDownloadUrl(const std::string& accessToken,
                                        const std::string& xxToken,
                                        const std::string& fileId) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    long long idInt = 0;
    try { idInt = std::stoll(fileId); } catch (...) {}

    const QJsonObject bodyObj{{"id", idInt}};
    const auto r = workbenchPost(
        "/p/p/workbench/api/work/index/getDowdLoadUrl",
        accessToken, xxToken,
        QJsonDocument(bodyObj).toJson(QJsonDocument::Compact));
    if (!r.ok) return {false, "Erreur réseau: " + r.error};

    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1)
            return {false, j.value("msg", "Erreur URL download")};

        std::string url;
        const auto& d = j["data"];
        if (d.is_string())
            url = d.get<std::string>();
        else if (d.is_object() && d.contains("url"))
            url = d["url"].get<std::string>();

        if (url.empty())
            return {false, "URL non trouvée dans la réponse"};

        // Ne pas logger l'URL signée (sécurité — section 0.4)
        logging::info("app", "cloud_client", "get_download_url_ok",
                      "URL de téléchargement obtenue");
        return {true, "URL obtenue", url};
    } catch (...) { return {false, "Réponse invalide"}; }
#endif
}

// ── fetchCloudPrinters ────────────────────────────────────────────────────

CloudPrintersResult fetchCloudPrinters(const std::string& accessToken,
                                       const std::string& xxToken) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    const auto r = workbenchGet(
        "/p/p/workbench/api/work/printer/getPrinters",
        accessToken, xxToken);
    if (!r.ok) return {false, "Erreur réseau: " + r.error};

    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1)
            return {false, j.value("msg", "Erreur imprimantes")};

        const auto& data = j.value("data", nlohmann::json::array());
        if (!data.is_array())
            return {false, "data imprimantes invalide"};

        CloudPrintersResult out;
        out.ok = true;
        out.printers.reserve(data.size());
        for (const auto& e : data) {
            if (!e.is_object()) continue;
            out.printers.push_back(parsePrinterEntry(e));
        }
        out.message = std::to_string(out.printers.size()) + " imprimante(s)";
        return out;
    } catch (const std::exception& e) {
        return {false, std::string("Parse error: ") + e.what()};
    }
#endif
}

// ── fetchPrinterCompatibilityByExt ───────────────────────────────────────

CloudPrinterCompatResult fetchPrinterCompatibilityByExt(const std::string& accessToken,
                                                        const std::string& xxToken,
                                                        const std::string& fileExt) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (fileExt.empty()) return {false, "file_ext requis"};

    QUrlQuery query;
    query.addQueryItem("file_ext", QString::fromStdString(fileExt));
    const std::string path =
        "/p/p/workbench/api/v2/printer/printersStatus?" + query.toString().toStdString();

    const auto r = workbenchGet(path.c_str(), accessToken, xxToken);
    if (!r.ok) return {false, "Erreur réseau: " + r.error};

    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1)
            return {false, j.value("msg", "Erreur compatibilité imprimantes")};

        const auto& data = j.value("data", nlohmann::json::array());
        if (!data.is_array())
            return {false, "data compatibilité invalide"};

        CloudPrinterCompatResult out;
        out.ok = true;
        out.printers.reserve(data.size());
        for (const auto& e : data) {
            if (!e.is_object()) continue;
            out.printers.push_back(parsePrinterCompatEntry(e));
        }
        out.message = std::to_string(out.printers.size()) + " imprimante(s) compatibles";
        return out;
    } catch (const std::exception& e) {
        return {false, std::string("Parse error: ") + e.what()};
    }
#endif
}

// ── sendCloudPrintOrder ───────────────────────────────────────────────────

CloudPrintOrderResult sendCloudPrintOrder(const std::string& accessToken,
                                          const std::string& xxToken,
                                          const std::string& printerId,
                                          const std::string& fileId,
                                          bool deleteAfterPrint) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (printerId.empty()) return {false, "printer_id requis"};
    if (fileId.empty()) return {false, "file_id requis"};

    QJsonObject dataObj{
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
                      QString::fromUtf8(QJsonDocument(dataObj).toJson(QJsonDocument::Compact)));

    const QByteArray body = form.query(QUrl::FullyEncoded).toUtf8();
    const auto r = workbenchPostForm(
        "/p/p/workbench/api/work/operation/sendOrder",
        accessToken, xxToken, body);
    if (!r.ok) return {false, "Erreur réseau: " + r.error, {}};

    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1)
            return {false, j.value("msg", "Erreur sendOrder"), {}};

        std::string taskId;
        const auto& d = j.value("data", nlohmann::json::object());
        if (d.is_object()) taskId = jStr(d.value("task_id", nlohmann::json{}));
        return {true, "Print order envoyée", taskId};
    } catch (...) {
        return {false, "Réponse invalide", {}};
    }
#endif
}

} // namespace accloud::cloud
