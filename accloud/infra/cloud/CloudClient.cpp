#include "CloudClient.h"
#include "infra/cloud/core/EndpointRegistry.h"
#include "infra/cloud/core/HttpClient.h"
#include "infra/cloud/core/ResponseEnvelopeParser.h"
#include "infra/cloud/core/WorkbenchRequestBuilder.h"
#include "infra/logging/JsonlLogger.h"

#ifdef ACCLOUD_WITH_QT
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

RawResp workbenchCall(core::EndpointId endpointId,
                      const std::string& accessToken,
                      const std::string& xxToken,
                      const QByteArray& body = {},
                      const QString& queryString = {}) {
    const auto endpoint = core::EndpointRegistry::instance().find(endpointId);
    if (!endpoint.has_value()) {
        return RawResp{false, 0, {}, "Unknown endpoint id"};
    }

    const core::WorkbenchRequestBuilder builder;
    const auto built = builder.build(*endpoint, accessToken, xxToken, queryString, body);
    if (!built.has_value()) {
        return RawResp{false, 0, {}, "Failed to build request"};
    }

    const core::HttpClient client;
    const core::HttpResponse response = client.execute(*built);
    return RawResp{response.ok, response.httpStatus, response.body, response.error};
}

RawResp workbenchPost(core::EndpointId endpointId, const std::string& accessToken,
                      const std::string& xxToken, const QByteArray& jsonBody) {
    return workbenchCall(endpointId, accessToken, xxToken, jsonBody);
}

RawResp workbenchPostForm(core::EndpointId endpointId, const std::string& accessToken,
                          const std::string& xxToken, const QByteArray& formBody) {
    return workbenchCall(endpointId, accessToken, xxToken, formBody);
}

RawResp workbenchGet(core::EndpointId endpointId, const std::string& accessToken,
                     const std::string& xxToken, const QString& queryString = {}) {
    return workbenchCall(endpointId, accessToken, xxToken, {}, queryString);
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

long long jLong(const nlohmann::json& v, long long fallback = 0) {
    if (v.is_number_integer()) return v.get<long long>();
    if (v.is_number_float())   return static_cast<long long>(v.get<double>());
    if (v.is_boolean())        return v.get<bool>() ? 1 : 0;
    if (v.is_string()) {
        try { return std::stoll(v.get<std::string>()); }
        catch (...) { return fallback; }
    }
    return fallback;
}

long long jFirstLong(const nlohmann::json& obj,
                     std::initializer_list<const char*> keys,
                     long long fallback = 0) {
    for (const char* k : keys) {
        if (!obj.contains(k)) continue;
        return jLong(obj[k], fallback);
    }
    return fallback;
}

long long normalizeEpochSeconds(long long epoch) {
    if (epoch <= 0) return 0;
    if (epoch > 1000000000000LL)  // epoch ms -> epoch s
        return epoch / 1000;
    return epoch;
}

std::string joinJsonStringArray(const nlohmann::json& value) {
    if (!value.is_array()) return {};
    std::string out;
    for (const auto& item : value) {
        const std::string part = jStr(item);
        if (part.empty()) continue;
        if (!out.empty()) out += ", ";
        out += part;
    }
    return out;
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

int durationSecondsFromObject(const nlohmann::json& obj,
                              std::initializer_list<const char*> secondKeys,
                              std::initializer_list<const char*> minuteKeys) {
    if (!obj.is_object())
        return -1;
    int secValue = jFirstInt(obj, secondKeys, -1);
    if (secValue >= 0)
        return secValue;
    int minValue = jFirstInt(obj, minuteKeys, -1);
    if (minValue >= 0)
        return minValue * 60;
    return -1;
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
    f.thumbnailUrl = jFirst(e, {"printer_image_id", "thumbnail", "img", "image"});
    f.downloadUrl  = jFirst(e, {"url", "download_url", "downloadUrl"});
    f.region       = jFirst(e, {"region"});
    f.bucket       = jFirst(e, {"bucket", "bucket_id"});
    f.path         = jFirst(e, {"path"});
    f.md5          = jFirst(e, {"md5", "origin_file_md5"});
    f.status       = e.value("status", 0);

    const nlohmann::json sp = parseSliceParam(e);
    f.createTime = jFirstLong(e, {"create_time", "createTime", "upload_time", "uploadTime", "time"}, 0);
    if (f.createTime <= 0 && sp.is_object())
        f.createTime = jFirstLong(sp, {"create_time", "createTime", "time", "timestamp"}, 0);
    f.createTime = normalizeEpochSeconds(f.createTime);

    f.updateTime = jFirstLong(e, {"update_time", "updateTime", "last_update_time", "lastUpdateTime"}, 0);
    if (f.updateTime <= 0)
        f.updateTime = jFirstLong(e, {"create_time", "createTime", "upload_time", "uploadTime"}, 0);
    if (f.updateTime <= 0 && sp.is_object())
        f.updateTime = jFirstLong(sp, {"update_time", "updateTime", "timestamp", "time", "create_time", "createTime"}, 0);
    f.updateTime = normalizeEpochSeconds(f.updateTime);

    if (sp.is_object()) {
        f.machine = jFirst(sp, {"machineName", "machine_name", "machineType"});
        if (f.machine.empty())
            f.machine = jFirst(e, {"machine_name", "machineName", "model", "printer_name"});

        if (f.printers.empty() && e.contains("printer_names"))
            f.printers = joinJsonStringArray(e["printer_names"]);
        if (f.printers.empty())
            f.printers = jFirst(sp, {"printer_names", "printerNames"});
        if (f.printers.empty())
            f.printers = f.machine;

        f.material = jFirst(sp, {"materialName", "material_name", "resinType", "material"});
        if (f.material.empty())
            f.material = jFirst(e, {"material_name", "materialName", "material"});

        const auto ptRaw = jFirst(sp, {"printTime", "print_time", "estimate", "totalTime"});
        if (!ptRaw.empty()) {
            try { f.printTime = formatSeconds(std::stoll(ptRaw)); }
            catch (...) { f.printTime = ptRaw; }
        }
        if (f.printTime.empty()) {
            const auto topEstimate = jFirst(e, {"estimate", "print_time"});
            if (!topEstimate.empty()) {
                try { f.printTime = formatSeconds(std::stoll(topEstimate)); }
                catch (...) { f.printTime = topEstimate; }
            }
        }

        auto lh = jFirst(sp, {"layerHeight", "layer_height", "sliceHeight", "normalLayerHeight", "zthick", "z_thick"});
        if (lh.empty())
            lh = jFirst(e, {"layer_height"});
        if (!lh.empty()) {
            if (lh.find("mm") == std::string::npos) lh += " mm";
            f.layerHeight = lh;
        }

        f.layers = jFirst(sp, {"layerCount", "layer_count", "layers", "totalLayers", "total_layers"});
        if (f.layers.empty())
            f.layers = jFirst(e, {"layers", "total_layers"});

        auto rv = jFirst(sp, {"resinVolume", "resin_volume", "resinUsage", "weight", "supplies_usage"});
        if (rv.empty())
            rv = jFirst(e, {"supplies_usage", "material"});
        if (!rv.empty()) {
            if (rv.find("ml") == std::string::npos && rv.find("g") == std::string::npos)
                rv += " ml";
            f.resinUsage = rv;
        }

        f.bottomLayers = jFirst(sp, {"bottomLayers", "bottom_layers", "bott_layers"});

        f.exposureTime = jFirst(sp, {"exposureTime", "exposure_time", "on_time"});
        if (!f.exposureTime.empty() && !containsNoCase(f.exposureTime, "s"))
            f.exposureTime += " s";

        f.offTime = jFirst(sp, {"offTime", "off_time"});
        if (!f.offTime.empty() && !containsNoCase(f.offTime, "s"))
            f.offTime += " s";

        if (f.md5.empty())
            f.md5 = jFirst(sp, {"sliced_md5", "md5"});
        if (f.bucket.empty())
            f.bucket = jFirst(sp, {"bucket_id", "bucket"});

        auto sx = jFirst(sp, {"sizeX", "size_x", "x", "width"});
        auto sy = jFirst(sp, {"sizeY", "size_y", "y", "height"});
        auto sz = jFirst(sp, {"sizeZ", "size_z", "z", "depth"});
        if (sx.empty()) sx = jFirst(e, {"size_x"});
        if (sy.empty()) sy = jFirst(e, {"size_y"});
        if (sz.empty()) sz = jFirst(e, {"size_z"});
        if (!sx.empty() && !sy.empty() && !sz.empty())
            f.dimensions = sx + "x" + sy + "x" + sz;
        else
            f.dimensions = jFirst(sp, {"dimensions", "boundingBox"});
    }
    if (f.machine.empty())
        f.machine = jFirst(e, {"machine_name", "machineName", "model", "printer_name"});
    if (f.printers.empty() && e.contains("printer_names"))
        f.printers = joinJsonStringArray(e["printer_names"]);
    if (f.printers.empty())
        f.printers = f.machine;
    if (f.material.empty())
        f.material = jFirst(e, {"material_name", "materialName", "material"});
    if (f.dimensions.empty()) {
        const auto sx = jFirst(e, {"size_x", "sizeX"});
        const auto sy = jFirst(e, {"size_y", "sizeY"});
        const auto sz = jFirst(e, {"size_z", "sizeZ"});
        if (!sx.empty() && !sy.empty() && !sz.empty())
            f.dimensions = sx + "x" + sy + "x" + sz;
    }
    if (f.bottomLayers.empty())
        f.bottomLayers = jFirst(e, {"bottom_layers", "bott_layers"});
    if (f.exposureTime.empty())
        f.exposureTime = jFirst(e, {"exposure_time"});
    if (f.offTime.empty())
        f.offTime = jFirst(e, {"off_time"});
    if (!f.exposureTime.empty() && !containsNoCase(f.exposureTime, "s"))
        f.exposureTime += " s";
    if (!f.offTime.empty() && !containsNoCase(f.offTime, "s"))
        f.offTime += " s";
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
    p.type = jFirst(e, {"type", "machine_type_name", "printer_type"});
    p.lastSeen = jFirst(e, {"last_seen", "lastSeen", "last_online_time", "last_report_time", "last_active_time", "updated_at"});
    p.reason = jFirst(e, {"reason", "status_text"});
    p.available = jFirstInt(e, {"available"}, -1);

    int deviceStatus = jFirstInt(e, {"device_status"}, -1);
    const int readyStatus = jFirstInt(e, {"ready_status"}, -1);
    const int isPrinting = jFirstInt(e, {"is_printing"}, -1);
    const int onlineFlag = jFirstInt(e, {"online", "isOnline", "connected"}, -1);

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

    nlohmann::json base;
    if (e.contains("base")) {
        const auto& b = e["base"];
        if (b.is_object()) {
            base = b;
        } else if (b.is_string()) {
            auto parsed = nlohmann::json::parse(b.get<std::string>(), nullptr, false);
            if (!parsed.is_discarded() && parsed.is_object()) base = parsed;
        }
    }

    if (p.type.empty())
        p.type = jFirst(base, {"type", "machine_type_name", "printer_type"});
    if (p.lastSeen.empty())
        p.lastSeen = jFirst(base, {"last_seen", "lastSeen", "last_online_time", "last_report_time", "last_active_time", "updated_at"});
    if (p.lastSeen.empty())
        p.lastSeen = jFirst(deviceMessage, {"last_seen", "lastSeen", "report_time", "update_time", "timestamp"});
    if (deviceStatus < 0)
        deviceStatus = jFirstInt(base, {"device_status"}, -1);
    if (deviceStatus < 0)
        deviceStatus = jFirstInt(deviceMessage, {"device_status"}, -1);

    p.progress = jFirstInt(deviceMessage, {"progress"}, -1);
    if (p.progress < 0) p.progress = jFirstInt(project, {"progress"}, -1);
    p.remainingSec = durationSecondsFromObject(deviceMessage,
                                               {"remaining_sec"},
                                               {"remain_time", "remaining_time"});
    if (p.remainingSec < 0) {
        p.remainingSec = durationSecondsFromObject(project,
                                                   {"remaining_sec"},
                                                   {"remain_time", "remaining_time"});
    }
    p.elapsedSec = durationSecondsFromObject(project,
                                             {"elapsed_sec"},
                                             {"elapsed_time", "time_elapsed", "print_time"});
    if (p.elapsedSec < 0) {
        p.elapsedSec = durationSecondsFromObject(deviceMessage,
                                                 {"elapsed_sec"},
                                                 {"elapsed_time", "time_elapsed", "print_time"});
    }
    p.currentLayer = jFirstInt(project, {"curr_layer", "current_layer", "currLayer", "currentLayer"}, -1);
    if (p.currentLayer < 0) {
        p.currentLayer = jFirstInt(deviceMessage, {"curr_layer", "current_layer", "currLayer", "currentLayer"}, -1);
    }
    p.totalLayers = jFirstInt(project, {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"}, -1);
    if (p.totalLayers < 0) {
        p.totalLayers = jFirstInt(deviceMessage, {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"}, -1);
    }
    p.currentFile = jFirst(project, {"old_filename", "filename", "file_name"});
    if (p.currentFile.empty()) p.currentFile = jFirst(deviceMessage, {"old_filename", "file_name", "filename"});

    // Règle prioritaire demandée:
    // - device_status == 1 => online
    // - device_status == 2 => offline
    // Puis fallback available / online flag.
    bool isOnline = false;
    if (deviceStatus == 1) {
        isOnline = true;
    } else if (deviceStatus == 2) {
        isOnline = false;
    } else if (p.available >= 0) {
        isOnline = (p.available == 1);
    } else if (onlineFlag >= 0) {
        isOnline = (onlineFlag == 1);
    } else if (!containsNoCase(p.reason, "offline")) {
        // Best-effort when payload is partial and no explicit offline signal.
        isOnline = true;
    }

    const bool hasProject = project.is_object() && !project.empty();
    const bool reasonSaysPrinting = containsNoCase(p.reason, "printing")
                                 || containsNoCase(p.reason, "in progress")
                                 || containsNoCase(p.reason, "busy");
    bool isBusyPrinting = false;
    if (hasProject) {
        isBusyPrinting = true;
    } else if (isPrinting >= 0) {
        // Some getPrinters payloads report is_printing=1 while reason=free.
        // Treat it as printing only when another signal agrees.
        isBusyPrinting = (isPrinting > 0) && (reasonSaysPrinting || readyStatus == 2);
    } else {
        isBusyPrinting = (p.progress >= 0 && p.progress < 100)
                      || containsNoCase(jFirst(deviceMessage, {"action"}), "start");
    }

    if (!isOnline) {
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

CloudPrinterProjectItem parsePrinterProjectItem(const nlohmann::json& e) {
    CloudPrinterProjectItem item;
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
    nlohmann::json settingsMessage;
    if (e.contains("settings")) {
        const auto& sm = e["settings"];
        if (sm.is_object()) {
            settingsMessage = sm;
        } else if (sm.is_string()) {
            auto parsed = nlohmann::json::parse(sm.get<std::string>(), nullptr, false);
            if (!parsed.is_discarded() && parsed.is_object()) settingsMessage = parsed;
        }
    }

    item.taskId = jFirst(e, {"task_id", "id"});
    item.gcodeName = jFirst(e, {"gcode_name", "old_filename", "filename", "file_name"});
    item.printerId = jFirst(e, {"printer_id"});
    item.printerName = jFirst(e, {"printer_name", "machine_name"});
    item.printStatus = jFirstInt(e, {"print_status"}, 0);
    item.progress = jFirstInt(e, {"progress"}, -1);
    if (item.progress < 0) item.progress = jFirstInt(deviceMessage, {"progress"}, -1);
    if (item.progress < 0) item.progress = jFirstInt(settingsMessage, {"progress"}, -1);
    item.remainingSec = durationSecondsFromObject(e,
                                                  {"remaining_sec"},
                                                  {"remain_time", "remaining_time"});
    if (item.remainingSec < 0) {
        item.remainingSec = durationSecondsFromObject(deviceMessage,
                                                      {"remaining_sec"},
                                                      {"remain_time", "remaining_time"});
    }
    if (item.remainingSec < 0) {
        item.remainingSec = durationSecondsFromObject(settingsMessage,
                                                      {"remaining_sec"},
                                                      {"remain_time", "remaining_time"});
    }
    item.elapsedSec = durationSecondsFromObject(e,
                                                {"elapsed_sec"},
                                                {"elapsed_time", "time_elapsed", "print_time"});
    if (item.elapsedSec < 0) {
        item.elapsedSec = durationSecondsFromObject(deviceMessage,
                                                    {"elapsed_sec"},
                                                    {"elapsed_time", "time_elapsed", "print_time"});
    }
    if (item.elapsedSec < 0) {
        item.elapsedSec = durationSecondsFromObject(settingsMessage,
                                                    {"elapsed_sec"},
                                                    {"elapsed_time", "time_elapsed", "print_time"});
    }
    item.currentLayer = jFirstInt(e, {"curr_layer", "current_layer", "currLayer", "currentLayer"}, -1);
    if (item.currentLayer < 0) {
        item.currentLayer = jFirstInt(deviceMessage, {"curr_layer", "current_layer", "currLayer", "currentLayer"}, -1);
    }
    if (item.currentLayer < 0) {
        item.currentLayer = jFirstInt(settingsMessage, {"curr_layer", "current_layer", "currLayer", "currentLayer"}, -1);
    }
    item.totalLayers = jFirstInt(e, {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"}, -1);
    if (item.totalLayers < 0) {
        item.totalLayers = jFirstInt(deviceMessage, {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"}, -1);
    }
    if (item.totalLayers < 0) {
        item.totalLayers = jFirstInt(settingsMessage, {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"}, -1);
    }
    item.currentFile = jFirst(e, {"old_filename", "filename", "file_name", "gcode_name"});
    if (item.currentFile.empty()) item.currentFile = jFirst(deviceMessage, {"old_filename", "filename", "file_name"});
    if (item.currentFile.empty()) item.currentFile = jFirst(settingsMessage, {"old_filename", "filename", "file_name"});
    item.reason = jFirst(e, {"reason"});
    if (e.contains("create_time") && e["create_time"].is_number())
        item.createTime = e["create_time"].get<long long>();
    if (e.contains("end_time") && e["end_time"].is_number())
        item.endTime = e["end_time"].get<long long>();
    item.img = jFirst(e, {"img", "image_id"});
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
    const auto validate = workbenchPost(core::EndpointId::AuthCheckSession,
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

    // Fallback (Docs §1.4): loginWithAccessToken
    const QJsonObject payload{{"access_token", QString::fromStdString(accessToken)},
                              {"accessToken",  QString::fromStdString(accessToken)},
                              {"device_type",  "web"}};
    const auto login = workbenchPost(core::EndpointId::AuthLoginWithAccessToken,
                                     accessToken, xxToken,
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
    auto r = workbenchPost(core::EndpointId::FilesList,
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
        r = workbenchPost(core::EndpointId::FilesListFallback,
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

    const auto r = workbenchPost(core::EndpointId::AuthCheckSession,
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
    const auto r = workbenchPost(core::EndpointId::FilesDelete,
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
    const auto r = workbenchPost(core::EndpointId::FilesDownloadUrl,
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

    const auto r = workbenchGet(core::EndpointId::PrintersList, accessToken, xxToken);
    if (!r.ok) return {false, "Erreur réseau: " + r.error};

    CloudPrintersResult out;
#if defined(ACCLOUD_DEBUG)
    nlohmann::json debugPayload = nlohmann::json::object();
#endif
    try {
        const auto j = nlohmann::json::parse(r.body);
#if defined(ACCLOUD_DEBUG)
        debugPayload["getPrinters"] = j;
#endif
        if (j.value("code", 0) != 1) {
            out.ok = false;
            out.message = j.value("msg", "Erreur imprimantes");
#if defined(ACCLOUD_DEBUG)
            out.rawJson = debugPayload.dump();
#endif
            return out;
        }

        const auto& data = j.value("data", nlohmann::json::array());
        if (!data.is_array()) {
            out.ok = false;
            out.message = "data imprimantes invalide";
#if defined(ACCLOUD_DEBUG)
            out.rawJson = debugPayload.dump();
#endif
            return out;
        }

        out.ok = true;
        out.printers.reserve(data.size());
#if defined(ACCLOUD_DEBUG)
        debugPayload["projects"] = nlohmann::json::object();
#endif
        for (const auto& e : data) {
            if (!e.is_object()) continue;
            nlohmann::json merged = e;
            const std::string printerId = jFirst(e, {"id", "printer_id", "printerId"});
            if (!printerId.empty()) {
                QUrlQuery projectQuery;
                projectQuery.addQueryItem("printer_id", QString::fromStdString(printerId));
                projectQuery.addQueryItem("print_status", "1");
                const auto projectResp = workbenchGet(core::EndpointId::ProjectsListByPrinter,
                                                      accessToken, xxToken, projectQuery.toString());
                if (projectResp.ok) {
                    auto projectJson = nlohmann::json::parse(projectResp.body, nullptr, false);
                    if (!projectJson.is_discarded()) {
#if defined(ACCLOUD_DEBUG)
                        debugPayload["projects"][printerId] = projectJson;
#endif
                        if (projectJson.value("code", 0) == 1) {
                            const auto projectData = projectJson.value("data", nlohmann::json::array());
                            if (projectData.is_array() && !projectData.empty() && projectData[0].is_object())
                                merged["project"] = projectData[0];
                            else if (projectData.is_object())
                                merged["project"] = projectData;
                        }
                    } else {
#if defined(ACCLOUD_DEBUG)
                        debugPayload["projects"][printerId] = nlohmann::json::object(
                            {{"parse_error", "invalid_json"}});
#endif
                    }
                } else {
#if defined(ACCLOUD_DEBUG)
                    debugPayload["projects"][printerId] = nlohmann::json::object(
                        {{"network_error", projectResp.error}});
#endif
                }
            }
            out.printers.push_back(parsePrinterEntry(merged));
        }
        out.message = std::to_string(out.printers.size()) + " imprimante(s)";
#if defined(ACCLOUD_DEBUG)
        out.rawJson = debugPayload.dump();
#endif
        return out;
    } catch (const std::exception& e) {
        out.ok = false;
        out.message = std::string("Parse error: ") + e.what();
#if defined(ACCLOUD_DEBUG)
        out.rawJson = debugPayload.empty() ? r.body : debugPayload.dump();
#endif
        return out;
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
    const auto r = workbenchGet(core::EndpointId::PrintersStatus,
                                accessToken, xxToken, query.toString());
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

CloudPrinterCompatResult fetchPrinterCompatibilityByFileId(const std::string& accessToken,
                                                           const std::string& xxToken,
                                                           const std::string& fileId) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (fileId.empty()) return {false, "file_id requis"};

    QUrlQuery query;
    query.addQueryItem("file_id", QString::fromStdString(fileId));
    const auto r = workbenchGet(core::EndpointId::PrintersStatus,
                                accessToken, xxToken, query.toString());
    if (!r.ok) return {false, "Erreur réseau: " + r.error};

    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1)
            return {false, j.value("msg", "Erreur compatibilite imprimantes")};

        const auto& data = j.value("data", nlohmann::json::array());
        if (!data.is_array())
            return {false, "data compatibilite invalide"};

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

CloudPrinterDetailsResult fetchPrinterDetails(const std::string& accessToken,
                                              const std::string& xxToken,
                                              const std::string& printerId) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (printerId.empty()) return {false, "printer_id requis"};

    QUrlQuery query;
    query.addQueryItem("id", QString::fromStdString(printerId));
    const auto r = workbenchGet(core::EndpointId::PrintersDetails,
                                accessToken, xxToken, query.toString());
    if (!r.ok) return {false, "Erreur reseau: " + r.error};

    CloudPrinterDetailsResult out;
    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1) {
            out.ok = false;
            out.message = j.value("msg", "Erreur printer info");
#if defined(ACCLOUD_DEBUG)
            out.rawJson = r.body;
#endif
            return out;
        }

        const auto& data = j.value("data", nlohmann::json::object());
        if (!data.is_object()) {
            out.ok = false;
            out.message = "data printer info invalide";
#if defined(ACCLOUD_DEBUG)
            out.rawJson = r.body;
#endif
            return out;
        }

        const auto& base = data.value("base", nlohmann::json::object());
        nlohmann::json deviceMessage;
        if (data.contains("device_message")) {
            const auto& dm = data["device_message"];
            if (dm.is_object()) {
                deviceMessage = dm;
            } else if (dm.is_string()) {
                auto parsed = nlohmann::json::parse(dm.get<std::string>(), nullptr, false);
                if (!parsed.is_discarded() && parsed.is_object()) deviceMessage = parsed;
            }
        }

        nlohmann::json settingsMessage;
        if (data.contains("settings")) {
            const auto& sm = data["settings"];
            if (sm.is_object()) {
                settingsMessage = sm;
            } else if (sm.is_string()) {
                auto parsed = nlohmann::json::parse(sm.get<std::string>(), nullptr, false);
                if (!parsed.is_discarded() && parsed.is_object()) settingsMessage = parsed;
            }
        }

        nlohmann::json sliceResultMessage;
        if (data.contains("slice_result")) {
            const auto& sr = data["slice_result"];
            if (sr.is_object()) {
                sliceResultMessage = sr;
            } else if (sr.is_string()) {
                auto parsed = nlohmann::json::parse(sr.get<std::string>(), nullptr, false);
                if (!parsed.is_discarded() && parsed.is_object()) sliceResultMessage = parsed;
            }
        }

        auto firstLiveInt = [&](std::initializer_list<const char*> keys) -> int {
            int value = jFirstInt(data, keys, -1);
            if (value >= 0) return value;
            value = jFirstInt(deviceMessage, keys, -1);
            if (value >= 0) return value;
            value = jFirstInt(settingsMessage, keys, -1);
            if (value >= 0) return value;
            return jFirstInt(sliceResultMessage, keys, -1);
        };

        auto firstLiveText = [&](std::initializer_list<const char*> keys) -> std::string {
            std::string value = jFirst(data, keys);
            if (!value.empty()) return value;
            value = jFirst(deviceMessage, keys);
            if (!value.empty()) return value;
            value = jFirst(settingsMessage, keys);
            if (!value.empty()) return value;
            return jFirst(sliceResultMessage, keys);
        };

        auto firstLiveDurationSeconds = [&](std::initializer_list<const char*> secondKeys,
                                            std::initializer_list<const char*> minuteKeys) -> int {
            int value = durationSecondsFromObject(data, secondKeys, minuteKeys);
            if (value >= 0) return value;
            value = durationSecondsFromObject(deviceMessage, secondKeys, minuteKeys);
            if (value >= 0) return value;
            value = durationSecondsFromObject(settingsMessage, secondKeys, minuteKeys);
            if (value >= 0) return value;
            return durationSecondsFromObject(sliceResultMessage, secondKeys, minuteKeys);
        };

        out.ok = true;
        out.message = "Details imprimante charges";
#if defined(ACCLOUD_DEBUG)
        out.rawJson = data.dump();
#endif
        out.progress = firstLiveInt({"progress"});
        out.remainingSec = firstLiveDurationSeconds({"remaining_sec"},
                                                    {"remain_time", "remaining_time"});
        out.elapsedSec = firstLiveDurationSeconds({"elapsed_sec"},
                                                  {"elapsed_time", "time_elapsed", "print_time"});
        out.currentLayer = firstLiveInt({"curr_layer", "current_layer", "currLayer", "currentLayer"});
        out.totalLayers = firstLiveInt({"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"});
        out.currentFile = firstLiveText({"old_filename", "file_name", "filename"});
        out.firmwareVersion = jFirst(base, {"firmware_version"});
        out.printCount = jFirst(base, {"print_count"});
        out.printTotalTime = jFirst(base, {"print_totaltime"});
        out.materialType = jFirst(base, {"material_type"});
        out.materialUsed = jFirst(base, {"material_used"});
        out.machineMac = jFirst(base, {"machine_mac"});
        out.helpUrl = jFirst(data, {"help_url"});
        out.quickStartUrl = jFirst(data, {"quick_start_url"});

        const auto& releaseFilm = data.value("releaseFilm", nlohmann::json::object());
        out.releaseFilmLayers = jFirst(releaseFilm, {"layers"});

        const auto& tools = data.value("tools", nlohmann::json::array());
        if (tools.is_array()) {
            out.tools.reserve(tools.size());
            for (const auto& t : tools) {
                if (!t.is_object()) continue;
                const std::string name = jFirst(t, {"function_name", "name"});
                if (!name.empty()) out.tools.push_back(name);
            }
        }

        const auto& advances = data.value("advance", nlohmann::json::array());
        if (advances.is_array()) {
            out.advances.reserve(advances.size());
            for (const auto& a : advances) {
                if (!a.is_object()) continue;
                const std::string name = jFirst(a, {"function_name", "name"});
                if (!name.empty()) out.advances.push_back(name);
            }
        }
        return out;
    } catch (const std::exception& e) {
        out.ok = false;
        out.message = std::string("Parse error: ") + e.what();
#if defined(ACCLOUD_DEBUG)
        out.rawJson = r.body;
#endif
        return out;
    }
#endif
}

CloudReasonCatalogResult fetchReasonCatalog(const std::string& accessToken,
                                            const std::string& xxToken) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    const auto r = workbenchGet(core::EndpointId::ReasonCatalog, accessToken, xxToken);
    if (!r.ok) return {false, "Erreur reseau: " + r.error};

    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1)
            return {false, j.value("msg", "Erreur reason catalog")};

        const auto& data = j.value("data", nlohmann::json::array());
        if (!data.is_array())
            return {false, "data reason catalog invalide"};

        CloudReasonCatalogResult out;
        out.ok = true;
        out.reasons.reserve(data.size());
        for (const auto& e : data) {
            if (!e.is_object()) continue;
            CloudReasonCatalogItem item;
            item.reason = jFirstInt(e, {"reason"}, 0);
            item.desc = jFirst(e, {"desc"});
            item.helpUrl = jFirst(e, {"help_url"});
            item.type = jFirst(e, {"type"});
            item.push = jFirstInt(e, {"push"}, 0);
            item.popup = jFirstInt(e, {"popup"}, 0);
            out.reasons.push_back(std::move(item));
        }
        out.message = std::to_string(out.reasons.size()) + " raison(s) chargee(s)";
        return out;
    } catch (const std::exception& e) {
        return {false, std::string("Parse error: ") + e.what()};
    }
#endif
}

CloudPrinterProjectsResult fetchPrinterProjects(const std::string& accessToken,
                                                const std::string& xxToken,
                                                const std::string& printerId,
                                                int page,
                                                int limit) {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (printerId.empty()) return {false, "printer_id requis"};

    QUrlQuery query;
    query.addQueryItem("printer_id", QString::fromStdString(printerId));
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("limit", QString::number(limit));
    const auto r = workbenchGet(core::EndpointId::ProjectsListByPrinter,
                                accessToken, xxToken, query.toString());
    if (!r.ok) return {false, "Erreur reseau: " + r.error};

    CloudPrinterProjectsResult out;
#if defined(ACCLOUD_DEBUG)
    out.rawJson = r.body;
#endif

    try {
        const auto j = nlohmann::json::parse(r.body);
        if (j.value("code", 0) != 1) {
            out.ok = false;
            out.message = j.value("msg", "Erreur projects");
            return out;
        }

        const auto& data = j.value("data", nlohmann::json::array());
        if (!data.is_array()) {
            out.ok = false;
            out.message = "data projects invalide";
            return out;
        }

        out.ok = true;
#if defined(ACCLOUD_DEBUG)
        out.rawJson = j.dump();
#endif
        out.items.reserve(data.size());
        for (const auto& e : data) {
            if (!e.is_object()) continue;
            out.items.push_back(parsePrinterProjectItem(e));
        }
        out.message = std::to_string(out.items.size()) + " projet(s)";
        return out;
    } catch (const std::exception& e) {
        out.ok = false;
        out.message = std::string("Parse error: ") + e.what();
        return out;
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
    const auto r = workbenchPostForm(core::EndpointId::OrdersSend,
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
