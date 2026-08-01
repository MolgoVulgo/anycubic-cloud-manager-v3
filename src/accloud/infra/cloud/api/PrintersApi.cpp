#include "PrintersApi.h"

#include "ApiSupport.h"

#ifdef ACCLOUD_WITH_QT
#include <QUrlQuery>
#endif

namespace accloud::cloud::api {
namespace {

CloudPrinterInfo parsePrinterEntry(const nlohmann::json& e) {
    CloudPrinterInfo p;
    p.id = support::jsonString(e.value("id", nlohmann::json{}));
    p.printerKey = support::firstString(e, {"printer_key", "device_key", "key"});
    p.machineType = support::firstString(e, {"machine_type", "machineType", "machine_type_id", "model_id"});
    p.name = support::firstString(e, {"name", "printer_name", "device_name"});
    p.model = support::firstString(e, {"model", "model_name", "machine_name", "machineType"});
    p.type = support::firstString(e, {"type", "machine_type_name", "printer_type"});
    p.lastSeen = support::firstString(e, {"last_seen", "lastSeen", "last_online_time", "last_report_time", "last_active_time", "updated_at"});
    p.reason = support::firstString(e, {"reason", "status_text"});
    p.available = support::firstInt(e, {"available"}, -1);

    int deviceStatus = support::firstInt(e, {"device_status"}, -1);
    const int readyStatus = support::firstInt(e, {"ready_status"}, -1);
    const int isPrinting = support::firstInt(e, {"is_printing"}, -1);
    const int onlineFlag = support::firstInt(e, {"online", "isOnline", "connected"}, -1);

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
        p.type = support::firstString(base, {"type", "machine_type_name", "printer_type"});
    if (p.printerKey.empty())
        p.printerKey = support::firstString(base, {"printer_key", "device_key", "key"});
    if (p.machineType.empty())
        p.machineType = support::firstString(base, {"machine_type", "machineType", "machine_type_id", "model_id"});
    if (p.printerKey.empty())
        p.printerKey = p.id;
    if (p.machineType.empty())
        p.machineType = p.type;
    if (p.lastSeen.empty())
        p.lastSeen = support::firstString(base, {"last_seen", "lastSeen", "last_online_time", "last_report_time", "last_active_time", "updated_at"});
    if (p.lastSeen.empty())
        p.lastSeen = support::firstString(deviceMessage, {"last_seen", "lastSeen", "report_time", "update_time", "timestamp"});
    if (deviceStatus < 0)
        deviceStatus = support::firstInt(base, {"device_status"}, -1);
    if (deviceStatus < 0)
        deviceStatus = support::firstInt(deviceMessage, {"device_status"}, -1);

    p.progress = support::firstInt(deviceMessage, {"progress"}, -1);
    if (p.progress < 0) p.progress = support::firstInt(project, {"progress"}, -1);
    p.remainingSec = support::durationSecondsFromObject(deviceMessage,
                                               {"remaining_sec"},
                                               {"remain_time", "remaining_time"});
    if (p.remainingSec < 0) {
        p.remainingSec = support::durationSecondsFromObject(project,
                                                   {"remaining_sec"},
                                                   {"remain_time", "remaining_time"});
    }
    p.elapsedSec = support::durationSecondsFromObject(project,
                                             {"elapsed_sec"},
                                             {"elapsed_time", "time_elapsed", "print_time"});
    if (p.elapsedSec < 0) {
        p.elapsedSec = support::durationSecondsFromObject(deviceMessage,
                                                 {"elapsed_sec"},
                                                 {"elapsed_time", "time_elapsed", "print_time"});
    }
    p.currentLayer = support::firstInt(project, {"curr_layer", "current_layer", "currLayer", "currentLayer"}, -1);
    if (p.currentLayer < 0) {
        p.currentLayer = support::firstInt(deviceMessage, {"curr_layer", "current_layer", "currLayer", "currentLayer"}, -1);
    }
    p.totalLayers = support::firstInt(project, {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"}, -1);
    if (p.totalLayers < 0) {
        p.totalLayers = support::firstInt(deviceMessage, {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"}, -1);
    }
    p.currentFile = support::firstString(project, {"old_filename", "filename", "file_name"});
    if (p.currentFile.empty()) p.currentFile = support::firstString(deviceMessage, {"old_filename", "file_name", "filename"});

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
    } else if (!support::containsNoCase(p.reason, "offline")) {
        // Best-effort when payload is partial and no explicit offline signal.
        isOnline = true;
    }

    const int projectPrintStatus = support::firstInt(project, {"print_status", "printStatus", "status"}, -1);
    const int projectProgress = support::firstInt(project, {"progress"}, -1);
    const std::string projectStateText = support::firstString(project, {"state", "print_state", "reason"});
    const bool hasActiveProject = project.is_object()
                               && !project.empty()
                               && (projectPrintStatus == 1
                                   || (projectProgress >= 0 && projectProgress < 100)
                                   || support::containsNoCase(projectStateText, "print")
                                   || support::containsNoCase(projectStateText, "busy")
                                   || support::containsNoCase(projectStateText, "progress"));
    const bool reasonSaysPrinting = support::containsNoCase(p.reason, "printing")
                                 || support::containsNoCase(p.reason, "in progress")
                                 || support::containsNoCase(p.reason, "busy");
    bool isBusyPrinting = false;
    if (hasActiveProject) {
        isBusyPrinting = true;
    } else if (isPrinting >= 0) {
        // Some getPrinters payloads report is_printing=1 while reason=free.
        // Treat it as printing only when another signal agrees.
        isBusyPrinting = (isPrinting > 0) && (reasonSaysPrinting || readyStatus == 2);
    } else {
        isBusyPrinting = (p.progress >= 0 && p.progress < 100)
                      || support::containsNoCase(support::firstString(deviceMessage, {"action"}), "start");
    }

    if (!isOnline) {
        p.state = "OFFLINE";
    } else if (isBusyPrinting) {
        p.state = "PRINTING";
    } else if (support::containsNoCase(p.reason, "error")) {
        p.state = "ERROR";
    } else {
        p.state = "READY";
    }

    return p;
}

CloudPrinterCompatItem parsePrinterCompatEntry(const nlohmann::json& e) {
    CloudPrinterCompatItem item;
    item.id = support::jsonString(e.value("id", nlohmann::json{}));
    item.available = support::firstInt(e, {"available"}, 0);
    item.reason = support::firstString(e, {"reason"});
    return item;
}


} // namespace

CloudPrintersResult PrintersApi::list(const std::string& accessToken,
                                      const std::string& xxToken) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    const auto r = support::workbenchGet(core::EndpointId::PrintersList, accessToken, xxToken);
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
            const std::string printerId = support::firstString(e, {"id", "printer_id", "printerId"});
            if (!printerId.empty()) {
                QUrlQuery projectQuery;
                projectQuery.addQueryItem("printer_id", QString::fromStdString(printerId));
                projectQuery.addQueryItem("print_status", "1");
                const auto projectResp = support::workbenchGet(core::EndpointId::ProjectsListByPrinter,
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

CloudPrinterCompatResult PrintersApi::compatibilityByExt(const std::string& accessToken,
                                                         const std::string& xxToken,
                                                         const std::string& fileExt) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (fileExt.empty()) return {false, "file_ext requis"};

    QUrlQuery query;
    query.addQueryItem("file_ext", QString::fromStdString(fileExt));
    const auto r = support::workbenchGet(core::EndpointId::PrintersStatus,
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

CloudPrinterCompatResult PrintersApi::compatibilityByFileId(const std::string& accessToken,
                                                            const std::string& xxToken,
                                                            const std::string& fileId) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (fileId.empty()) return {false, "file_id requis"};

    QUrlQuery query;
    query.addQueryItem("file_id", QString::fromStdString(fileId));
    const auto r = support::workbenchGet(core::EndpointId::PrintersStatus,
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

CloudPrinterDetailsResult PrintersApi::details(const std::string& accessToken,
                                               const std::string& xxToken,
                                               const std::string& printerId) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (printerId.empty()) return {false, "printer_id requis"};

    QUrlQuery query;
    query.addQueryItem("id", QString::fromStdString(printerId));
    const auto r = support::workbenchGet(core::EndpointId::PrintersDetails,
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
            int value = support::firstInt(data, keys, -1);
            if (value >= 0) return value;
            value = support::firstInt(deviceMessage, keys, -1);
            if (value >= 0) return value;
            value = support::firstInt(settingsMessage, keys, -1);
            if (value >= 0) return value;
            return support::firstInt(sliceResultMessage, keys, -1);
        };

        auto firstLiveText = [&](std::initializer_list<const char*> keys) -> std::string {
            std::string value = support::firstString(data, keys);
            if (!value.empty()) return value;
            value = support::firstString(deviceMessage, keys);
            if (!value.empty()) return value;
            value = support::firstString(settingsMessage, keys);
            if (!value.empty()) return value;
            return support::firstString(sliceResultMessage, keys);
        };

        auto firstLiveDurationSeconds = [&](std::initializer_list<const char*> secondKeys,
                                            std::initializer_list<const char*> minuteKeys) -> int {
            int value = support::durationSecondsFromObject(data, secondKeys, minuteKeys);
            if (value >= 0) return value;
            value = support::durationSecondsFromObject(deviceMessage, secondKeys, minuteKeys);
            if (value >= 0) return value;
            value = support::durationSecondsFromObject(settingsMessage, secondKeys, minuteKeys);
            if (value >= 0) return value;
            return support::durationSecondsFromObject(sliceResultMessage, secondKeys, minuteKeys);
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
        out.firmwareVersion = support::firstString(base, {"firmware_version"});
        out.printCount = support::firstString(base, {"print_count"});
        out.printTotalTime = support::firstString(base, {"print_totaltime"});
        out.materialType = support::firstString(base, {"material_type"});
        out.materialUsed = support::firstString(base, {"material_used"});
        out.machineMac = support::firstString(base, {"machine_mac"});
        out.helpUrl = support::firstString(data, {"help_url"});
        out.quickStartUrl = support::firstString(data, {"quick_start_url"});

        const auto& releaseFilm = data.value("releaseFilm", nlohmann::json::object());
        out.releaseFilmStatus = support::firstString(releaseFilm,
                                       {"status", "state", "desc", "name", "label", "release_film_status"});
        if (out.releaseFilmStatus.empty()) {
            out.releaseFilmStatus = support::firstString(data,
                                           {"releaseFilmStatus", "release_film_status", "fepStatus", "fep_status"});
        }
        out.releaseFilmLayers = support::firstString(releaseFilm, {"layers"});

        const auto& tools = data.value("tools", nlohmann::json::array());
        if (tools.is_array()) {
            out.tools.reserve(tools.size());
            for (const auto& t : tools) {
                if (!t.is_object()) continue;
                const std::string name = support::firstString(t, {"function_name", "name"});
                if (!name.empty()) out.tools.push_back(name);
            }
        }

        const auto& advances = data.value("advance", nlohmann::json::array());
        if (advances.is_array()) {
            out.advances.reserve(advances.size());
            for (const auto& a : advances) {
                if (!a.is_object()) continue;
                const std::string name = support::firstString(a, {"function_name", "name"});
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


} // namespace accloud::cloud::api
