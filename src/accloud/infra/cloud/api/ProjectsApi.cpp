#include "ProjectsApi.h"

#include "ApiSupport.h"

#ifdef ACCLOUD_WITH_QT
#include <QUrlQuery>
#endif

namespace accloud::cloud::api {
namespace {

CloudPrinterProjectItem parseProjectItem(const nlohmann::json& entry) {
    CloudPrinterProjectItem item;
    const nlohmann::json deviceMessage = support::objectOrParsedString(entry, "device_message");
    const nlohmann::json settingsMessage = support::objectOrParsedString(entry, "settings");

    item.taskId = support::firstString(entry, {"task_id", "taskid", "id"});
    item.cloudFileId = support::firstString(entry, {"model", "file_id"});
    item.gcodeId = support::firstString(entry, {"gcode_id"});
    item.gcodeName = support::firstString(entry, {"gcode_name", "old_filename", "filename", "file_name"});
    item.printerId = support::firstString(entry, {"printer_id"});
    item.printerName = support::firstString(entry, {"printer_name", "machine_name"});
    item.printStatus = support::firstInt(entry, {"print_status"}, 0);
    item.progress = support::firstInt(entry, {"progress"}, -1);
    if (item.progress < 0) item.progress = support::firstInt(deviceMessage, {"progress"}, -1);
    if (item.progress < 0) item.progress = support::firstInt(settingsMessage, {"progress"}, -1);

    item.remainingSec = support::durationSecondsFromObject(entry,
                                                            {"remaining_sec"},
                                                            {"remain_time", "remaining_time"});
    if (item.remainingSec < 0) {
        item.remainingSec = support::durationSecondsFromObject(deviceMessage,
                                                                {"remaining_sec"},
                                                                {"remain_time", "remaining_time"});
    }
    if (item.remainingSec < 0) {
        item.remainingSec = support::durationSecondsFromObject(settingsMessage,
                                                                {"remaining_sec"},
                                                                {"remain_time", "remaining_time"});
    }

    item.elapsedSec = support::durationSecondsFromObject(entry,
                                                          {"elapsed_sec"},
                                                          {"elapsed_time", "time_elapsed", "print_time"});
    if (item.elapsedSec < 0) {
        item.elapsedSec = support::durationSecondsFromObject(deviceMessage,
                                                              {"elapsed_sec"},
                                                              {"elapsed_time", "time_elapsed", "print_time"});
    }
    if (item.elapsedSec < 0) {
        item.elapsedSec = support::durationSecondsFromObject(settingsMessage,
                                                              {"elapsed_sec"},
                                                              {"elapsed_time", "time_elapsed", "print_time"});
    }

    item.currentLayer = support::firstInt(entry,
                                          {"curr_layer", "current_layer", "currLayer", "currentLayer"},
                                          -1);
    if (item.currentLayer < 0) {
        item.currentLayer = support::firstInt(deviceMessage,
                                              {"curr_layer", "current_layer", "currLayer", "currentLayer"},
                                              -1);
    }
    if (item.currentLayer < 0) {
        item.currentLayer = support::firstInt(settingsMessage,
                                              {"curr_layer", "current_layer", "currLayer", "currentLayer"},
                                              -1);
    }

    item.totalLayers = support::firstInt(entry,
                                         {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"},
                                         -1);
    if (item.totalLayers < 0) {
        item.totalLayers = support::firstInt(deviceMessage,
                                             {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"},
                                             -1);
    }
    if (item.totalLayers < 0) {
        item.totalLayers = support::firstInt(settingsMessage,
                                             {"total_layers", "total_layer", "totalLayers", "layers", "layer_count", "layerCount"},
                                             -1);
    }

    item.currentFile = support::firstString(entry, {"old_filename", "filename", "file_name", "gcode_name"});
    if (item.currentFile.empty()) item.currentFile = support::firstString(deviceMessage, {"old_filename", "filename", "file_name"});
    if (item.currentFile.empty()) item.currentFile = support::firstString(settingsMessage, {"old_filename", "filename", "file_name"});
    item.reason = support::firstString(entry, {"reason"});
    if (entry.contains("create_time") && entry["create_time"].is_number()) {
        item.createTime = entry["create_time"].get<long long>();
    }
    if (entry.contains("end_time") && entry["end_time"].is_number()) {
        item.endTime = entry["end_time"].get<long long>();
    }
    item.img = support::firstString(entry, {"img", "image_id"});
    return item;
}

} // namespace

CloudPrinterProjectsResult ProjectsApi::listByPrinter(const std::string& accessToken,
                                                      const std::string& xxToken,
                                                      const std::string& printerId,
                                                      int page,
                                                      int limit) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};
    if (printerId.empty()) return {false, "printer_id requis"};

    QUrlQuery query;
    query.addQueryItem("printer_id", QString::fromStdString(printerId));
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("limit", QString::number(limit));
    const auto response = support::workbenchGet(core::EndpointId::ProjectsListByPrinter,
                                                 accessToken, xxToken, query.toString());
    if (!response.ok) return {false, "Erreur reseau: " + response.error};

    CloudPrinterProjectsResult result;
#if defined(ACCLOUD_DEBUG)
    result.rawJson = response.body;
#endif

    try {
        const auto json = nlohmann::json::parse(response.body);
        if (json.value("code", 0) != 1) {
            result.ok = false;
            result.message = json.value("msg", "Erreur projects");
            return result;
        }

        const auto& data = json.value("data", nlohmann::json::array());
        if (!data.is_array()) {
            result.ok = false;
            result.message = "data projects invalide";
            return result;
        }

        result.ok = true;
#if defined(ACCLOUD_DEBUG)
        result.rawJson = json.dump();
#endif
        result.items.reserve(data.size());
        for (const auto& entry : data) {
            if (!entry.is_object()) continue;
            result.items.push_back(parseProjectItem(entry));
        }
        result.message = std::to_string(result.items.size()) + " projet(s)";
        return result;
    } catch (const std::exception& error) {
        result.ok = false;
        result.message = std::string("Parse error: ") + error.what();
        return result;
    }
#endif
}

} // namespace accloud::cloud::api
