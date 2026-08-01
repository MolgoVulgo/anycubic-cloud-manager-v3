#include "FilesApi.h"

#include "ApiSupport.h"
#include "ThumbnailCandidateBuilder.h"
#include "infra/logging/JsonlLogger.h"

#ifdef ACCLOUD_WITH_QT
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#endif

namespace accloud::cloud::api {
namespace {

nlohmann::json parseSliceParam(const nlohmann::json& entry) {
    if (!entry.contains("slice_param")) return {};
    const auto& value = entry["slice_param"];
    if (value.is_object()) return value;
    if (value.is_string()) {
        auto parsed = nlohmann::json::parse(value.get<std::string>(), nullptr, false);
        if (!parsed.is_discarded()) return parsed;
    }
    return {};
}

CloudFileInfo parseFileEntry(const nlohmann::json& entry) {
    using support::containsNoCase;
    using support::firstLong;
    using support::firstString;
    using support::formatSeconds;
    using support::joinJsonStringArray;
    using support::jsonString;
    using support::normalizeEpochSeconds;

    CloudFileInfo file;
    file.id = jsonString(entry.value("id", nlohmann::json{}));
    file.name = entry.value("old_filename", std::string{});
    if (file.name.empty()) file.name = entry.value("filename", std::string{});
    file.sizeBytes = entry.value("size", uint64_t{0});
    file.gcodeId = jsonString(entry.value("gcode_id", nlohmann::json{}));
    if (file.gcodeId == "0") file.gcodeId.clear();

    const nlohmann::json sliceParam = parseSliceParam(entry);
    ThumbnailCandidateInput thumbnailInput;
    thumbnailInput.thumbnail = firstString(entry, {"thumbnail"});
    thumbnailInput.image = firstString(entry, {"img", "image"});
    thumbnailInput.imageId = sliceParam.is_object()
        ? firstString(sliceParam, {"image_id"}) : std::string{};
    thumbnailInput.printerImageId = firstString(entry, {"printer_image_id"});
    thumbnailInput.image0Id = sliceParam.is_object()
        ? firstString(sliceParam, {"image0_id"}) : std::string{};
    thumbnailInput.bucket = sliceParam.is_object()
        ? firstString(sliceParam, {"bucket_id", "bucket"}) : std::string{};
    if (thumbnailInput.bucket.empty()) {
        thumbnailInput.bucket = firstString(entry, {"bucket_id", "bucket"});
    }
    thumbnailInput.region = firstString(entry, {"region"});
    file.thumbnailCandidates = buildThumbnailCandidates(thumbnailInput);
    if (!file.thumbnailCandidates.empty()) file.thumbnailUrl = file.thumbnailCandidates.front();

    file.downloadUrl = firstString(entry, {"url", "download_url", "downloadUrl"});
    file.region = firstString(entry, {"region"});
    file.bucket = firstString(entry, {"bucket", "bucket_id"});
    file.path = firstString(entry, {"path"});
    file.md5 = firstString(entry, {"md5", "origin_file_md5"});
    file.status = entry.value("status", 0);

    file.createTime = firstLong(entry,
                                {"create_time", "createTime", "upload_time", "uploadTime", "time"},
                                0);
    if (file.createTime <= 0 && sliceParam.is_object()) {
        file.createTime = firstLong(sliceParam,
                                    {"create_time", "createTime", "time", "timestamp"},
                                    0);
    }
    file.createTime = normalizeEpochSeconds(file.createTime);

    file.updateTime = firstLong(entry,
                                {"update_time", "updateTime", "last_update_time", "lastUpdateTime"},
                                0);
    if (file.updateTime <= 0) {
        file.updateTime = firstLong(entry,
                                    {"create_time", "createTime", "upload_time", "uploadTime"},
                                    0);
    }
    if (file.updateTime <= 0 && sliceParam.is_object()) {
        file.updateTime = firstLong(sliceParam,
                                    {"update_time", "updateTime", "timestamp", "time", "create_time", "createTime"},
                                    0);
    }
    file.updateTime = normalizeEpochSeconds(file.updateTime);

    if (sliceParam.is_object()) {
        file.machine = firstString(sliceParam, {"machineName", "machine_name", "machineType"});
        if (file.machine.empty()) {
            file.machine = firstString(entry, {"machine_name", "machineName", "model", "printer_name"});
        }

        if (file.printers.empty() && entry.contains("printer_names")) {
            file.printers = joinJsonStringArray(entry["printer_names"]);
        }
        if (file.printers.empty()) file.printers = firstString(sliceParam, {"printer_names", "printerNames"});
        if (file.printers.empty()) file.printers = file.machine;

        file.material = firstString(sliceParam, {"materialName", "material_name", "resinType", "material"});
        if (file.material.empty()) file.material = firstString(entry, {"material_name", "materialName", "material"});

        const auto printTimeRaw = firstString(sliceParam, {"printTime", "print_time", "estimate", "totalTime"});
        if (!printTimeRaw.empty()) {
            try {
                file.printTime = formatSeconds(std::stoll(printTimeRaw));
            } catch (...) {
                file.printTime = printTimeRaw;
            }
        }
        if (file.printTime.empty()) {
            const auto topEstimate = firstString(entry, {"estimate", "print_time"});
            if (!topEstimate.empty()) {
                try {
                    file.printTime = formatSeconds(std::stoll(topEstimate));
                } catch (...) {
                    file.printTime = topEstimate;
                }
            }
        }

        auto layerHeight = firstString(sliceParam,
                                       {"layerHeight", "layer_height", "sliceHeight", "normalLayerHeight", "zthick", "z_thick"});
        if (layerHeight.empty()) layerHeight = firstString(entry, {"layer_height"});
        if (!layerHeight.empty()) {
            if (layerHeight.find("mm") == std::string::npos) layerHeight += " mm";
            file.layerHeight = layerHeight;
        }

        file.layers = firstString(sliceParam,
                                  {"layerCount", "layer_count", "layers", "totalLayers", "total_layers"});
        if (file.layers.empty()) file.layers = firstString(entry, {"layers", "total_layers"});

        auto resinVolume = firstString(sliceParam,
                                       {"resinVolume", "resin_volume", "resinUsage", "weight", "supplies_usage"});
        if (resinVolume.empty()) resinVolume = firstString(entry, {"supplies_usage", "material"});
        if (!resinVolume.empty()) {
            if (resinVolume.find("ml") == std::string::npos && resinVolume.find("g") == std::string::npos) {
                resinVolume += " ml";
            }
            file.resinUsage = resinVolume;
        }

        file.bottomLayers = firstString(sliceParam, {"bottomLayers", "bottom_layers", "bott_layers"});
        file.exposureTime = firstString(sliceParam, {"exposureTime", "exposure_time", "on_time"});
        if (!file.exposureTime.empty() && !containsNoCase(file.exposureTime, "s")) file.exposureTime += " s";
        file.offTime = firstString(sliceParam, {"offTime", "off_time"});
        if (!file.offTime.empty() && !containsNoCase(file.offTime, "s")) file.offTime += " s";

        if (file.md5.empty()) file.md5 = firstString(sliceParam, {"sliced_md5", "md5"});
        if (file.bucket.empty()) file.bucket = firstString(sliceParam, {"bucket_id", "bucket"});

        auto sizeX = firstString(sliceParam, {"sizeX", "size_x", "x", "width"});
        auto sizeY = firstString(sliceParam, {"sizeY", "size_y", "y", "height"});
        auto sizeZ = firstString(sliceParam, {"sizeZ", "size_z", "z", "depth"});
        if (sizeX.empty()) sizeX = firstString(entry, {"size_x"});
        if (sizeY.empty()) sizeY = firstString(entry, {"size_y"});
        if (sizeZ.empty()) sizeZ = firstString(entry, {"size_z"});
        if (!sizeX.empty() && !sizeY.empty() && !sizeZ.empty()) {
            file.dimensions = sizeX + "x" + sizeY + "x" + sizeZ;
        } else {
            file.dimensions = firstString(sliceParam, {"dimensions", "boundingBox"});
        }
    }

    if (file.machine.empty()) file.machine = firstString(entry, {"machine_name", "machineName", "model", "printer_name"});
    if (file.printers.empty() && entry.contains("printer_names")) file.printers = joinJsonStringArray(entry["printer_names"]);
    if (file.printers.empty()) file.printers = file.machine;
    if (file.material.empty()) file.material = firstString(entry, {"material_name", "materialName", "material"});
    if (file.dimensions.empty()) {
        const auto sizeX = firstString(entry, {"size_x", "sizeX"});
        const auto sizeY = firstString(entry, {"size_y", "sizeY"});
        const auto sizeZ = firstString(entry, {"size_z", "sizeZ"});
        if (!sizeX.empty() && !sizeY.empty() && !sizeZ.empty()) {
            file.dimensions = sizeX + "x" + sizeY + "x" + sizeZ;
        }
    }
    if (file.bottomLayers.empty()) file.bottomLayers = firstString(entry, {"bottom_layers", "bott_layers"});
    if (file.exposureTime.empty()) file.exposureTime = firstString(entry, {"exposure_time"});
    if (file.offTime.empty()) file.offTime = firstString(entry, {"off_time"});
    if (!file.exposureTime.empty() && !containsNoCase(file.exposureTime, "s")) file.exposureTime += " s";
    if (!file.offTime.empty() && !containsNoCase(file.offTime, "s")) file.offTime += " s";
    return file;
}

std::vector<CloudFileInfo> parseFileArray(const nlohmann::json& data) {
    if (!data.is_array()) return {};
    std::vector<CloudFileInfo> files;
    files.reserve(data.size());
    for (const auto& entry : data) {
        if (entry.is_object()) files.push_back(parseFileEntry(entry));
    }
    return files;
}

} // namespace

CloudFilesResult FilesApi::list(const std::string& accessToken,
                                const std::string& xxToken,
                                int page,
                                int limit) const {
#ifndef ACCLOUD_WITH_QT
    return {false, "Qt non disponible"};
#else
    if (accessToken.empty()) return {false, "Pas d'access_token"};

    const QJsonObject bodyObject{{"page", page}, {"limit", limit}};
    const auto bodyBytes = QJsonDocument(bodyObject).toJson(QJsonDocument::Compact);

    auto response = support::workbenchPost(core::EndpointId::FilesList,
                                           accessToken, xxToken, bodyBytes);
    if (!response.ok) {
        logging::warn("app", "cloud_client", "fetch_files_network_error",
                      "Erreur réseau listing", {{"error", response.error}});
        return {false, "Erreur réseau: " + response.error};
    }

    auto tryParse = [&](const std::string& body) -> CloudFilesResult {
        const auto json = nlohmann::json::parse(body);
        CloudFilesResult result;
        result.ok = (json.value("code", 0) == 1);
        if (!result.ok) {
            result.message = json.value("msg", "Erreur serveur");
            return result;
        }
        if (json.contains("data") && json["data"].is_array()) {
            result.files = parseFileArray(json["data"]);
            result.total = static_cast<int>(result.files.size());
            result.message = std::to_string(result.total) + " fichier(s)";
        } else {
            result.ok = false;
            result.message = "data invalide";
        }
        return result;
    };

    try {
        auto result = tryParse(response.body);
        if (result.ok) {
            logging::info("app", "cloud_client", "fetch_files_ok",
                          "Listing OK", {{"count", std::to_string(result.total)}});
            return result;
        }
        logging::info("app", "cloud_client", "fetch_files_fallback", "Fallback userFiles");
        response = support::workbenchPost(core::EndpointId::FilesListFallback,
                                          accessToken, xxToken, bodyBytes);
        if (!response.ok) return {false, "Erreur réseau: " + response.error};
        return tryParse(response.body);
    } catch (const std::exception& error) {
        return {false, std::string("Parse error: ") + error.what()};
    }
#endif
}

CloudOpResult FilesApi::remove(const std::string& accessToken,
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

    const QJsonObject bodyObject{{"idArr", QJsonArray{numericId}}};
    const auto response = support::workbenchPost(
        core::EndpointId::FilesDelete,
        accessToken,
        xxToken,
        QJsonDocument(bodyObject).toJson(QJsonDocument::Compact));
    if (!response.ok) return {false, "Erreur réseau: " + response.error};

    try {
        const auto json = nlohmann::json::parse(response.body);
        const bool ok = (json.value("code", 0) == 1);
        return {ok, ok ? "Fichier supprimé" : json.value("msg", "Erreur suppression")};
    } catch (...) {
        return {false, "Réponse invalide"};
    }
#endif
}

} // namespace accloud::cloud::api
