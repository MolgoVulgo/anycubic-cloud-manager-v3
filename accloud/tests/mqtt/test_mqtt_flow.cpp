#include "app/realtime/PrinterRealtimeStore.h"
#include "app/usecases/cloud/ApplyRealtimeOverlayUseCase.h"
#include "infra/mqtt/core/TlsMaterialProvider.h"
#include "infra/mqtt/observability/TelemetryObservationStore.h"
#include "infra/mqtt/routing/MqttMessageRouter.h"
#include "infra/mqtt/routing/MqttTopicBuilder.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

bool test_route_extracts_realtime_fields() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/app/m7/prod-key-01/print/report";
    const std::string payload = R"json(
{"type":"print","action":"printReport","state":"printing","msgid":"m-1","data":{
  "progress":42,
  "elapsed_time":120,
  "remaining_time":"360",
  "current_layer":7,
  "total_layers":100,
  "file_name":"demo.pwmb",
  "reason":"ok"
}}
)json";

    const auto routed = router.route(topic, payload);
    if (!expect(routed.event.has_value(), "route should produce an event")) {
        return false;
    }
    const auto& event = *routed.event;
    return expect(event.progress.has_value() && *event.progress == 42, "progress should be parsed")
        && expect(event.elapsedSec.has_value() && *event.elapsedSec == 120, "elapsed should be parsed")
        && expect(event.remainingSec.has_value() && *event.remainingSec == 360,
                  "remaining should be parsed")
        && expect(event.currentLayer.has_value() && *event.currentLayer == 7,
                  "current layer should be parsed")
        && expect(event.totalLayers.has_value() && *event.totalLayers == 100,
                  "total layers should be parsed")
        && expect(event.currentFile.has_value() && *event.currentFile == "demo.pwmb",
                  "current file should be parsed")
        && expect(event.reason.has_value() && *event.reason == "ok", "reason should be parsed");
}

bool test_store_applies_metrics() {
    auto& store = accloud::realtime::PrinterRealtimeStore::instance();
    store.clear();

    accloud::realtime::PrinterRealtimeEvent event;
    event.printerKey = "printer-42";
    event.type = accloud::realtime::MessageType::Print;
    event.action = "printReport";
    event.state = "printing";
    event.taskId = std::string("task-42");
    event.progress = 81;
    event.printProgress = 81;
    event.elapsedSec = 400;
    event.remainingSec = 120;
    event.currentLayer = 55;
    event.totalLayers = 80;
    event.currentFile = std::string("part.pwmb");
    event.reason = std::string("none");

    store.applyEvent(event);
    const auto snapshot = store.get("printer-42");
    const bool ok = expect(snapshot.has_value(), "snapshot should exist after apply")
        && expect(snapshot->state.has_value() && *snapshot->state == "PRINTING",
                  "state should map to PRINTING")
        && expect(snapshot->progress.has_value() && *snapshot->progress == 81,
                  "progress should be stored")
        && expect(snapshot->elapsedSec.has_value() && *snapshot->elapsedSec == 400,
                  "elapsed should be stored")
        && expect(snapshot->remainingSec.has_value() && *snapshot->remainingSec == 120,
                  "remaining should be stored")
        && expect(snapshot->currentLayer.has_value() && *snapshot->currentLayer == 55,
                  "current layer should be stored")
        && expect(snapshot->totalLayers.has_value() && *snapshot->totalLayers == 80,
                  "total layers should be stored")
        && expect(snapshot->currentFile.has_value() && *snapshot->currentFile == "part.pwmb",
                  "current file should be stored")
        && expect(snapshot->reason.has_value() && *snapshot->reason == "none",
                  "reason should be stored");
    store.clear();
    return ok;
}

bool test_router_routes_release_film_status() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/public/m7/prod-key-01/releaseFilm/report";
    const std::string payload = R"json(
{"type":"releaseFilm","action":"get","code":0,"data":{"layers":30001,"status":0,"times":60}}
)json";

    const auto routed = router.route(topic, payload);
    return expect(routed.disposition == accloud::mqtt::routing::RouteDisposition::Routed,
                  "releaseFilm messages should be routed")
        && expect(routed.event.has_value(), "releaseFilm should produce a realtime event")
        && expect(routed.event->type == accloud::realtime::MessageType::Peripheral,
                  "releaseFilm should map to peripheral event")
        && expect(routed.event->kind == accloud::realtime::EventKind::ReleaseFilmUpdate,
                  "releaseFilm event kind should be explicit")
        && expect(routed.event->releaseFilmLayers.has_value()
                  && *routed.event->releaseFilmLayers == 30001,
                  "releaseFilm layers should be exposed")
        && expect(routed.event->releaseFilmTimes.has_value()
                  && *routed.event->releaseFilmTimes == 60,
                  "releaseFilm times should be exposed")
        && expect(routed.event->releaseFilmStatusCode.has_value()
                  && *routed.event->releaseFilmStatusCode == -1,
                  "releaseFilm status should be forced to -1 over ACF/NFEP threshold");
}

bool test_router_accepts_documented_local_file_list_payload() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/public/m7/prod-key-01/file/report";
    const std::string payload = R"json(
{"type":"file","action":"listLocalFile","state":"done","data":{"files":[{"filename":"plate-a.pwmb","size":2048,"timestamp":1710000000,"is_dir":0}]}}
)json";

    const auto routed = router.route(topic, payload);
    return expect(routed.disposition == accloud::mqtt::routing::RouteDisposition::Routed,
                  "documented listLocalFile payload should route")
        && expect(routed.envelope.type == "file", "file type should be preserved")
        && expect(routed.envelope.action == "listLocalFile", "listLocalFile action should be preserved")
        && expect(routed.envelope.data.contains("files"), "documented data.files array should be preserved")
        && expect(routed.envelope.data["files"].is_array(), "data.files should remain an array")
        && expect(routed.envelope.data["files"].size() == 1, "data.files should contain one record");
}

bool test_store_applies_release_film_status() {
    auto& store = accloud::realtime::PrinterRealtimeStore::instance();
    store.clear();

    accloud::realtime::PrinterRealtimeEvent event;
    event.printerKey = "printer-film";
    event.type = accloud::realtime::MessageType::Peripheral;
    event.action = "report";
    event.releaseFilmStatus = std::string("warn");
    event.releaseFilmLayers = 25805;
    event.releaseFilmTimes = 60;
    event.releaseFilmStatusCode = 0;

    store.applyEvent(event);
    const auto snapshot = store.get("printer-film");
    const bool ok = expect(snapshot.has_value(), "releaseFilm snapshot should exist")
        && expect(snapshot->releaseFilmStatus.has_value()
                  && *snapshot->releaseFilmStatus == "warn",
                  "releaseFilm status should be stored")
        && expect(snapshot->releaseFilmLayers.has_value()
                  && *snapshot->releaseFilmLayers == 25805,
                  "releaseFilm layers should be stored")
        && expect(snapshot->releaseFilmTimes.has_value()
                  && *snapshot->releaseFilmTimes == 60,
                  "releaseFilm times should be stored")
        && expect(snapshot->releaseFilmStatusCode.has_value()
                  && *snapshot->releaseFilmStatusCode == 0,
                  "releaseFilm status code should be stored");
    store.clear();
    return ok;
}

bool test_router_infers_type_from_topic_when_payload_omits_type() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/public/m7/101001/status/report";
    const std::string payload = R"json(
{"action":"workReport","state":"busy","data":{"progress":9}}
)json";

    const auto routed = router.route(topic, payload);
    if (!expect(routed.disposition == accloud::mqtt::routing::RouteDisposition::Routed,
                "Topic-based type inference should route message")) {
        return false;
    }
    if (!expect(routed.event.has_value(), "Inferred message should create realtime event")) {
        return false;
    }
    return expect(routed.event->type == accloud::realtime::MessageType::Status,
                  "Inferred type from /status/report should map to status");
}

bool test_router_promotes_wifi_resin_video_types() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string wifiPayload = R"json({"type":"wifi","action":"report","state":"ok","data":{}})json";
    const std::string resinPayload = R"json({"type":"resin","action":"report","state":"low","data":{}})json";
    const std::string videoPayload = R"json({"type":"video","action":"report","state":"ready","data":{}})json";

    const auto wifi = router.route("anycubic/anycubicCloud/v1/printer/public/m7/101001/wifi/report", wifiPayload);
    const auto resin = router.route("anycubic/anycubicCloud/v1/printer/public/m7/101001/resin/report", resinPayload);
    const auto video = router.route("anycubic/anycubicCloud/v1/server/printer/m7/101001/video", videoPayload);

    return expect(wifi.event.has_value(), "wifi should be promoted to routed event")
        && expect(resin.event.has_value(), "resin should be promoted to routed event")
        && expect(video.event.has_value(), "video should be promoted to routed event")
        && expect(wifi.event->type == accloud::realtime::MessageType::Peripheral,
                  "wifi should map to Peripheral type")
        && expect(resin.event->type == accloud::realtime::MessageType::Peripheral,
                  "resin should map to Peripheral type")
        && expect(video.event->type == accloud::realtime::MessageType::Peripheral,
                  "video should map to Peripheral type");
}

bool test_router_extracts_resin_feed_message() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/public/m7/101001/resin/report";
    const std::string payload = R"json(
{"type":"resin","action":"feedResin","code":1501,"msg":"resin bottle empty","data":{}}
)json";

    const auto routed = router.route(topic, payload);
    return expect(routed.disposition == accloud::mqtt::routing::RouteDisposition::Routed,
                  "feedResin should route")
        && expect(routed.event.has_value(), "feedResin should produce realtime event")
        && expect(routed.event->wireType == "resin", "resin wire type should be preserved")
        && expect(routed.event->action == "feedResin", "feedResin action should be preserved")
        && expect(routed.event->code.has_value() && *routed.event->code == 1501,
                  "feedResin code should be parsed from root")
        && expect(routed.event->reason.has_value()
                  && *routed.event->reason == "resin bottle empty",
                  "feedResin root msg should be exposed as reason");
}

bool test_router_extracts_m7_print_workflow_fields() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/public/128/prod-key-01/print/report";
    const std::string payload = R"json(
{"type":"print","action":"start","state":"preheating","msgid":"m-heat","data":{
  "taskid":"87153347",
  "filename":"B3_B4.pwsz",
  "curr_layer":0,
  "total_layers":1977,
  "progress":0,
  "print_time":0,
  "remain_time":180,
  "task_mode":1,
  "slicer":"Lychee Slicer",
  "heating_remain_time":-1,
  "heating_skip_allowed":true
}}
)json";

    const auto routed = router.route(topic, payload);
    if (!expect(routed.event.has_value(), "M7 start/preheating should route to an event")) {
        return false;
    }
    const auto& event = *routed.event;
    return expect(event.taskId.has_value() && *event.taskId == "87153347", "taskid should be parsed")
        && expect(event.printProgress.has_value() && *event.printProgress == 0,
                  "print progress should be parsed from start")
        && expect(event.currentLayer.has_value() && *event.currentLayer == 0,
                  "curr_layer should be parsed")
        && expect(event.totalLayers.has_value() && *event.totalLayers == 1977,
                  "total_layers should be parsed")
        && expect(event.taskMode.has_value() && *event.taskMode == 1,
                  "task_mode should be parsed")
        && expect(event.slicer.has_value() && *event.slicer == "Lychee Slicer",
                  "slicer should be parsed")
        && expect(event.heatingSkipAllowed.has_value() && *event.heatingSkipAllowed,
                  "heating_skip_allowed should be parsed")
        && expect(!event.heatingRemainingSec.has_value(),
                  "heating_remain_time=-1 should be treated as unknown");
}

bool test_router_extracts_m7_check_status_maps() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/public/128/prod-key-01/print/report";
    const std::string monitorPayload = R"json(
{"type":"print","action":"monitor","state":"monitoring","data":{
  "taskid":"87153347",
  "checkStatus":[{"name":"pullForce","status":0},{"name":"wifiDev","status":0},{"name":"motor","status":-2}]
}}
)json";
    const std::string autoPayload = R"json(
{"type":"print","action":"autoOperation","state":"monitoring","data":{
  "taskid":"87153347",
  "checkStatus":[{"name":"platform","status":0},{"name":"resin","status":-1}]
}}
)json";

    const auto monitor = router.route(topic, monitorPayload);
    const auto automatic = router.route(topic, autoPayload);
    return expect(monitor.event.has_value(), "monitor should route to an event")
        && expect(automatic.event.has_value(), "autoOperation should route to an event")
        && expect(monitor.event->hardwareChecks.size() == 3,
                  "monitor checkStatus should populate hardware checks")
        && expect(monitor.event->hardwareChecks.at("motor") == -2,
                  "hardware check status should be preserved")
        && expect(automatic.event->autoChecks.size() == 2,
                  "autoOperation checkStatus should populate auto checks")
        && expect(automatic.event->autoChecks.at("resin") == -1,
                  "auto check status should be preserved");
}

bool test_store_tracks_resin_fill_and_runtime_message() {
    auto& store = accloud::realtime::PrinterRealtimeStore::instance();
    store.clear();

    accloud::realtime::PrinterRealtimeEvent loaded;
    loaded.printerKey = "printer-resin";
    loaded.type = accloud::realtime::MessageType::Print;
    loaded.action = "start";
    loaded.state = "printing";
    loaded.taskId = std::string("task-resin");
    loaded.currentLayer = 0;
    loaded.totalLayers = 100;
    loaded.printProgress = 0;
    store.applyEvent(loaded);

    accloud::realtime::PrinterRealtimeEvent autoCheck;
    autoCheck.printerKey = "printer-resin";
    autoCheck.type = accloud::realtime::MessageType::Print;
    autoCheck.action = "autoOperation";
    autoCheck.state = "monitoring";
    autoCheck.taskId = std::string("task-resin");
    autoCheck.autoChecks = {{"resin", 0}};
    store.applyEvent(autoCheck);

    accloud::realtime::PrinterRealtimeEvent preheat = loaded;
    preheat.state = "preheating";
    store.applyEvent(preheat);

    auto snapshot = store.get("printer-resin");
    if (!expect(snapshot.has_value(), "resin fill snapshot should exist")
        || !expect(snapshot->resin.prePrintFillStatus.has_value()
                      && *snapshot->resin.prePrintFillStatus == "done",
                  "preheating should infer resin pre-print fill success")
        || !expect(snapshot->resin.uiStatus.has_value()
                      && *snapshot->resin.uiStatus == "done",
                  "resin UI status should be done after inferred success")) {
        store.clear();
        return false;
    }

    accloud::realtime::PrinterRealtimeEvent printing = loaded;
    printing.currentLayer = 5;
    printing.printProgress = 5;
    store.applyEvent(printing);

    accloud::realtime::PrinterRealtimeEvent resin;
    resin.printerKey = "printer-resin";
    resin.type = accloud::realtime::MessageType::Peripheral;
    resin.wireType = "resin";
    resin.action = "feedResin";
    resin.code = 1501;
    resin.reason = std::string("resin bottle empty");
    store.applyEvent(resin);

    snapshot = store.get("printer-resin");
    if (!expect(snapshot.has_value(), "runtime resin snapshot should exist")
        || !expect(snapshot->resin.runtimeTopupStatus.has_value()
                      && *snapshot->resin.runtimeTopupStatus == "bottle_empty_or_unavailable",
                  "runtime feedResin 1501 should mark bottle/source issue")
        || !expect(snapshot->resin.vatStatus.has_value()
                      && *snapshot->resin.vatStatus == "assumed_ok",
                  "runtime feedResin 1501 alone should not mark vat empty")
        || !expect(snapshot->resin.blockingPrint.has_value()
                      && !*snapshot->resin.blockingPrint,
                  "runtime feedResin 1501 alone should not block print")
        || !expect(snapshot->resin.message.has_value()
                      && *snapshot->resin.message == "resin bottle empty",
                  "runtime resin message should be stored for UI modal")) {
        store.clear();
        return false;
    }

    accloud::cloud::CloudPrinterInfo printer;
    printer.id = "printer-resin";
    printer.state = "READY";
    accloud::usecases::cloud::ApplyRealtimeOverlayUseCase overlay;
    auto merged = overlay.execute(std::vector<accloud::cloud::CloudPrinterInfo>{printer});
    const bool ok = expect(merged.size() == 1, "resin UI overlay should keep one printer")
        && expect(merged[0].mqttResinStatus == "warning",
                  "resin UI overlay should expose warning status")
        && expect(merged[0].mqttResinMessage == "resin bottle empty",
                  "resin UI overlay should expose modal message")
        && expect(merged[0].mqttResinLastFeedCode == 1501,
                  "resin UI overlay should expose feedResin code")
        && expect(!merged[0].mqttResinBlocking,
                  "resin UI overlay should expose non-blocking runtime topup issue");
    store.clear();
    return ok;
}

bool test_store_tracks_m7_print_workflow_by_taskid() {
    auto& store = accloud::realtime::PrinterRealtimeStore::instance();
    store.clear();

    accloud::realtime::PrinterRealtimeEvent status;
    status.printerKey = "printer-m7";
    status.type = accloud::realtime::MessageType::Status;
    status.action = "workReport";
    status.state = "busy";
    store.applyEvent(status);
    auto snapshot = store.get("printer-m7");
    if (!expect(snapshot.has_value(), "busy status should create printer snapshot")) {
        store.clear();
        return false;
    }
    if (!expect(snapshot->availability.has_value()
                   && *snapshot->availability == accloud::realtime::PrinterAvailability::Busy,
               "workReport/busy should update availability")) {
        store.clear();
        return false;
    }
    if (!expect(snapshot->state.has_value() && *snapshot->state == "BUSY",
                "workReport/busy alone should not become PRINTING")) {
        store.clear();
        return false;
    }

    accloud::realtime::PrinterRealtimeEvent downloading;
    downloading.printerKey = "printer-m7";
    downloading.type = accloud::realtime::MessageType::Print;
    downloading.action = "update";
    downloading.state = "downloading";
    downloading.taskId = std::string("87153347");
    downloading.downloadProgress = 100;
    store.applyEvent(downloading);

    accloud::realtime::PrinterRealtimeEvent loaded;
    loaded.printerKey = "printer-m7";
    loaded.type = accloud::realtime::MessageType::Print;
    loaded.action = "start";
    loaded.state = "printing";
    loaded.taskId = std::string("87153347");
    loaded.currentLayer = 0;
    loaded.totalLayers = 1977;
    loaded.printProgress = 0;
    loaded.currentFile = std::string("B3_B4.pwsz");
    store.applyEvent(loaded);

    snapshot = store.get("printer-m7");
    if (!expect(snapshot.has_value(), "loaded job should keep snapshot")) {
        store.clear();
        return false;
    }
    if (!expect(snapshot->activeTaskId.has_value() && *snapshot->activeTaskId == "87153347",
                "active taskid should be tracked")) {
        store.clear();
        return false;
    }
    if (!expect(snapshot->jobStage.has_value()
                   && *snapshot->jobStage == accloud::realtime::PrintJobStage::Loaded,
                "curr_layer=0 start/printing should be loaded")) {
        store.clear();
        return false;
    }

    accloud::realtime::PrinterRealtimeEvent printing = loaded;
    printing.currentLayer = 1;
    printing.printProgress = 1;
    store.applyEvent(printing);
    snapshot = store.get("printer-m7");
    if (!expect(snapshot.has_value()
                   && snapshot->jobStage.has_value()
                   && *snapshot->jobStage == accloud::realtime::PrintJobStage::Printing,
                "curr_layer>=1 should become printing")) {
        store.clear();
        return false;
    }
    if (!expect(snapshot->printStateText.has_value() && *snapshot->printStateText == "printing",
                "raw MQTT print state should be exposed")
        || !expect(snapshot->jobStageText.has_value() && *snapshot->jobStageText == "printing",
                   "normalized MQTT job stage should be exposed")) {
        store.clear();
        return false;
    }

    accloud::realtime::PrinterRealtimeEvent waiting = printing;
    waiting.state = "waiting";
    store.applyEvent(waiting);
    snapshot = store.get("printer-m7");
    if (!expect(snapshot.has_value()
                   && snapshot->state.has_value()
                   && *snapshot->state == "PRINTING",
                "waiting should keep the printer busy")
        || !expect(snapshot->printStateText.has_value() && *snapshot->printStateText == "waiting",
                   "waiting state should be exposed without replacing legacy busy state")) {
        store.clear();
        return false;
    }

    accloud::realtime::PrinterRealtimeEvent finished = loaded;
    finished.state = "finished";
    finished.printProgress = 100;
    store.applyEvent(finished);
    status.state = "free";
    store.applyEvent(status);
    snapshot = store.get("printer-m7");
    const bool ok = expect(snapshot.has_value()
                              && snapshot->jobStage.has_value()
                              && *snapshot->jobStage == accloud::realtime::PrintJobStage::Finished,
                           "start/finished should finish the active job")
        && expect(snapshot->availability.has_value()
                      && *snapshot->availability == accloud::realtime::PrinterAvailability::Free,
                  "workReport/free should update availability")
        && expect(snapshot->state.has_value() && *snapshot->state == "READY",
                  "finished job plus free availability should expose READY");
    store.clear();
    return ok;
}

bool test_store_links_command_sent_to_first_mqtt_taskid() {
    auto& store = accloud::realtime::PrinterRealtimeStore::instance();
    store.clear();

    store.recordPrintCommandSent("printer-m7", "", "file-123", "msg-123");
    auto snapshot = store.get("printer-m7");
    if (!expect(snapshot.has_value(), "command_sent should create a printer snapshot")) {
        store.clear();
        return false;
    }
    if (!expect(snapshot->jobStage.has_value()
                   && *snapshot->jobStage == accloud::realtime::PrintJobStage::CommandSent,
                "HTTPS accepted order should create command_sent stage")) {
        store.clear();
        return false;
    }
    if (!expect(snapshot->state.has_value() && *snapshot->state == "PENDING",
                "command_sent should not expose PRINTING")) {
        store.clear();
        return false;
    }

    accloud::realtime::PrinterRealtimeEvent downloading;
    downloading.printerKey = "printer-m7";
    downloading.type = accloud::realtime::MessageType::Print;
    downloading.action = "update";
    downloading.state = "downloading";
    downloading.msgid = "msg-456";
    downloading.taskId = std::string("87153347");
    downloading.downloadProgress = 45;
    store.applyEvent(downloading);

    snapshot = store.get("printer-m7");
    const bool ok = expect(snapshot.has_value(), "MQTT update should keep printer snapshot")
        && expect(snapshot->activeTaskId.has_value() && *snapshot->activeTaskId == "87153347",
                  "first MQTT taskid should replace pending task key")
        && expect(snapshot->jobs.size() == 1, "pending job should be migrated, not duplicated")
        && expect(snapshot->jobs.contains("87153347"), "real taskid job should exist")
        && expect(snapshot->jobs.at("87153347").fileId.has_value()
                      && *snapshot->jobs.at("87153347").fileId == "file-123",
                  "migrated job should keep HTTP file id")
        && expect(snapshot->jobStage.has_value()
                      && *snapshot->jobStage == accloud::realtime::PrintJobStage::Downloading,
                  "MQTT update should advance command_sent to downloading")
        && expect(snapshot->downloadProgress.has_value() && *snapshot->downloadProgress == 45,
                  "download progress should be exposed")
        && expect(!snapshot->progress.has_value(),
                  "download progress should not feed print progress");
    store.clear();
    return ok;
}

bool routeAndApply(accloud::mqtt::routing::MqttMessageRouter& router,
                   const std::string& topic,
                   const std::string& payload) {
    const auto routed = router.route(topic, payload);
    if (!expect(routed.disposition == accloud::mqtt::routing::RouteDisposition::Routed,
                "documented workflow payload should route")) {
        return false;
    }
    if (!expect(routed.event.has_value(), "documented workflow payload should produce realtime event")) {
        return false;
    }
    accloud::realtime::PrinterRealtimeStore::instance().applyEvent(*routed.event);
    return true;
}

bool test_nominal_m7_workflow_routes_store_and_replicates_ui_fields() {
    auto& store = accloud::realtime::PrinterRealtimeStore::instance();
    store.clear();

    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string printerKey = "printer-m7";
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/public/128/" + printerKey + "/print/report";
    const std::string statusTopic =
        "anycubic/anycubicCloud/v1/printer/public/128/" + printerKey + "/status/report";
    const std::string taskId = "89470925";

    store.recordPrintCommandSent(printerKey, "", "file-known-from-https", "msg-http-1");
    auto snapshot = store.get(printerKey);
    if (!expect(snapshot.has_value(), "HTTPS command_sent should create local job")
        || !expect(snapshot->state.has_value() && *snapshot->state == "PENDING",
                   "command_sent should expose pending state")) {
        store.clear();
        return false;
    }

    if (!routeAndApply(router, statusTopic,
                       R"json({"type":"status","action":"workReport","state":"busy","data":null})json")) {
        store.clear();
        return false;
    }
    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"update","state":"downloading","msgid":"msg-update-0","data":{"taskid":"89470925","progress":0,"task_mode":1}})json")) {
        store.clear();
        return false;
    }
    snapshot = store.get(printerKey);
    if (!expect(snapshot.has_value()
                   && snapshot->jobStage.has_value()
                   && *snapshot->jobStage == accloud::realtime::PrintJobStage::Downloading,
                "update/downloading progress=0 should enter downloading")
        || !expect(snapshot->downloadProgress.has_value() && *snapshot->downloadProgress == 0,
                   "download progress 0 should be exposed")
        || !expect(!snapshot->progress.has_value(),
                   "download progress 0 should not feed print progress")
        || !expect(snapshot->jobs.size() == 1 && snapshot->jobs.contains(taskId),
                   "first MQTT taskid should migrate the pending command job")
        || !expect(snapshot->jobs.at(taskId).fileId.has_value()
                       && *snapshot->jobs.at(taskId).fileId == "file-known-from-https",
                   "migrated job should preserve the HTTPS file reference")) {
        store.clear();
        return false;
    }

    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"update","state":"downloading","msgid":"msg-update-100","data":{"taskid":"89470925","progress":100,"task_mode":1}})json")) {
        store.clear();
        return false;
    }
    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"start","state":"printing","msgid":"msg-start-loaded","data":{"taskid":"89470925","filename":"C1(1).pwsz","curr_layer":0,"total_layers":1210,"progress":0,"print_time":0,"remain_time":113,"task_mode":1,"slicer":"Lychee Slicer"}})json")) {
        store.clear();
        return false;
    }
    snapshot = store.get(printerKey);
    if (!expect(snapshot.has_value()
                   && snapshot->jobStage.has_value()
                   && *snapshot->jobStage == accloud::realtime::PrintJobStage::Loaded,
                "start/printing curr_layer=0 should expose loaded")
        || !expect(snapshot->currentFile.has_value() && *snapshot->currentFile == "C1(1).pwsz",
                   "start payload should expose filename for UI")
        || !expect(snapshot->totalLayers.has_value() && *snapshot->totalLayers == 1210,
                   "start payload should expose total layers")) {
        store.clear();
        return false;
    }

    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"monitor","state":"monitoring","msgid":"msg-monitor","data":{"taskid":"89470925","checkStatus":[{"name":"pullForce","status":0},{"name":"motor","status":-2}]}})json")) {
        store.clear();
        return false;
    }
    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"autoOperation","state":"monitoring","msgid":"msg-auto","data":{"taskid":"89470925","checkStatus":[{"name":"platform","status":0},{"name":"resin","status":-1}]}})json")) {
        store.clear();
        return false;
    }
    snapshot = store.get(printerKey);
    if (!expect(snapshot.has_value()
                   && snapshot->jobs.at(taskId).hardwareChecks.at("motor") == -2,
                "monitor/monitoring checks should attach to the task")
        || !expect(snapshot->jobs.at(taskId).autoChecks.at("resin") == -1,
                   "autoOperation/monitoring checks should attach to the task")) {
        store.clear();
        return false;
    }

    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"start","state":"preheating","msgid":"msg-preheat","data":{"taskid":"89470925","filename":"C1(1).pwsz","curr_layer":0,"total_layers":1210,"progress":0,"print_time":0,"remain_time":113,"heating_skip_allowed":true,"heating_remain_time":90}})json")) {
        store.clear();
        return false;
    }
    snapshot = store.get(printerKey);
    if (!expect(snapshot.has_value()
                   && snapshot->jobStage.has_value()
                   && *snapshot->jobStage == accloud::realtime::PrintJobStage::Preheating,
                "start/preheating should expose preheating")
        || !expect(snapshot->jobs.at(taskId).heatingSkipAllowed.has_value()
                       && *snapshot->jobs.at(taskId).heatingSkipAllowed,
                   "preheating should preserve heating skip flag")
        || !expect(snapshot->jobs.at(taskId).heatingRemainingSec.has_value()
                       && *snapshot->jobs.at(taskId).heatingRemainingSec == 90,
                   "preheating should preserve heating remaining time")) {
        store.clear();
        return false;
    }

    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"start","state":"printing","msgid":"msg-print-1","data":{"taskid":"89470925","filename":"C1(1).pwsz","curr_layer":1,"total_layers":1210,"progress":1,"print_time":60,"remain_time":112,"task_mode":1,"slicer":"Lychee Slicer"}})json")) {
        store.clear();
        return false;
    }
    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"start","state":"printing","msgid":"msg-print-2","data":{"taskid":"89470925","filename":"C1(1).pwsz","curr_layer":450,"total_layers":1210,"progress":37,"print_time":1800,"remain_time":7200,"task_mode":1,"slicer":"Lychee Slicer"}})json")) {
        store.clear();
        return false;
    }
    snapshot = store.get(printerKey);
    if (!expect(snapshot.has_value()
                   && snapshot->jobStage.has_value()
                   && *snapshot->jobStage == accloud::realtime::PrintJobStage::Printing,
                "start/printing curr_layer>=1 should expose printing")
        || !expect(snapshot->progress.has_value() && *snapshot->progress == 37,
                   "repeated start/printing should update UI progress")
        || !expect(snapshot->currentLayer.has_value() && *snapshot->currentLayer == 450,
                   "repeated start/printing should update current layer")
        || !expect(snapshot->remainingSec.has_value() && *snapshot->remainingSec == 7200,
                   "repeated start/printing should update remaining time")
        || !expect(snapshot->elapsedSec.has_value() && *snapshot->elapsedSec == 1800,
                   "repeated start/printing should update elapsed time")
        || !expect(snapshot->printStateText.has_value() && *snapshot->printStateText == "printing",
                   "raw MQTT print state should be exposed for UI")
        || !expect(snapshot->jobStageText.has_value() && *snapshot->jobStageText == "printing",
                   "normalized job stage should be exposed for UI")) {
        store.clear();
        return false;
    }

    accloud::cloud::CloudPrinterInfo printer;
    printer.id = "cloud-printer-id";
    printer.printerKey = printerKey;
    printer.state = "READY";
    printer.progress = -1;

    accloud::usecases::cloud::ApplyRealtimeOverlayUseCase overlay;
    auto merged = overlay.execute(std::vector<accloud::cloud::CloudPrinterInfo>{printer});
    if (!expect(merged.size() == 1, "UI overlay should keep one printer")
        || !expect(merged[0].state == "PRINTING", "UI overlay should expose busy print state")
        || !expect(merged[0].progress == 37, "UI overlay should expose progress")
        || !expect(merged[0].elapsedSec == 1800, "UI overlay should expose elapsed seconds")
        || !expect(merged[0].remainingSec == 7200, "UI overlay should expose remaining seconds")
        || !expect(merged[0].currentLayer == 450, "UI overlay should expose current layer")
        || !expect(merged[0].totalLayers == 1210, "UI overlay should expose total layers")
        || !expect(merged[0].currentFile == "C1(1).pwsz", "UI overlay should expose current file")
        || !expect(merged[0].mqttActiveTaskId == taskId, "UI overlay should expose active taskid")
        || !expect(merged[0].mqttDownloadProgress == 100, "UI overlay should expose download progress")
        || !expect(merged[0].mqttPrintState == "printing", "UI overlay should expose raw MQTT print state")
        || !expect(merged[0].mqttJobStage == "printing", "UI overlay should expose normalized MQTT job stage")
        || !expect(merged[0].mqttHardwareChecks.at("motor") == -2,
                   "UI overlay should expose hardware checks")
        || !expect(merged[0].mqttAutoChecks.at("resin") == -1,
                   "UI overlay should expose autoOperation checks")) {
        store.clear();
        return false;
    }

    if (!routeAndApply(router, topic,
                       R"json({"type":"print","action":"start","state":"finished","msgid":"msg-finished","data":{"taskid":"89470925","filename":"C1(1).pwsz","curr_layer":1210,"total_layers":1210,"progress":100,"print_time":9000,"remain_time":0,"task_mode":1}})json")) {
        store.clear();
        return false;
    }
    if (!routeAndApply(router, statusTopic,
                       R"json({"type":"status","action":"workReport","state":"free","data":null})json")) {
        store.clear();
        return false;
    }

    merged = overlay.execute(std::vector<accloud::cloud::CloudPrinterInfo>{printer});
    const bool ok = expect(merged.size() == 1, "Final UI overlay should keep one printer")
        && expect(merged[0].state == "READY", "workReport/free after finished should expose READY")
        && expect(merged[0].progress == 100, "finished should expose 100 percent progress")
        && expect(merged[0].remainingSec == 0, "finished should expose zero remaining time")
        && expect(merged[0].currentLayer == 1210, "finished should expose final layer")
        && expect(merged[0].mqttPrintState == "finished", "finished raw MQTT state should reach UI")
        && expect(merged[0].mqttJobStage == "finished", "finished normalized job stage should reach UI");
    store.clear();
    return ok;
}

bool test_overlay_matches_printer_key_fallback() {
    auto& store = accloud::realtime::PrinterRealtimeStore::instance();
    store.clear();

    accloud::realtime::PrinterRealtimeSnapshot rt;
    rt.state = std::string("PRINTING");
    rt.progress = 66;
    store.upsert("printer-key-A", rt);

    accloud::cloud::CloudPrinterInfo printer;
    printer.id = "printer-id-A";
    printer.printerKey = "printer-key-A";
    printer.state = "READY";
    printer.progress = 0;

    accloud::usecases::cloud::ApplyRealtimeOverlayUseCase overlay;
    std::vector<accloud::cloud::CloudPrinterInfo> printers;
    printers.push_back(printer);
    auto merged = overlay.execute(std::move(printers));

    const bool ok = expect(merged.size() == 1, "merged list size should stay 1")
        && expect(merged[0].state == "PRINTING", "overlay should apply state using printerKey fallback")
        && expect(merged[0].progress == 66, "overlay should apply progress using printerKey fallback");
    store.clear();
    return ok;
}

std::optional<std::string> envValue(const char* key) {
    const char* value = std::getenv(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

void restoreEnv(const char* key, const std::optional<std::string>& value) {
    if (value.has_value()) {
        setenv(key, value->c_str(), 1);
    } else {
        unsetenv(key);
    }
}

bool test_tls_provider_local_fallback_paths() {
    const auto prevCa = envValue("ACCLOUD_MQTT_TLS_CA_PATH");
    const auto prevCert = envValue("ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH");
    const auto prevKey = envValue("ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH");
    const auto prevDevFallback = envValue("ACCLOUD_MQTT_TLS_DEV_FALLBACK");

    unsetenv("ACCLOUD_MQTT_TLS_CA_PATH");
    unsetenv("ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH");
    unsetenv("ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH");
    unsetenv("ACCLOUD_MQTT_TLS_DEV_FALLBACK");

    accloud::mqtt::core::TlsMaterialProvider provider;
    const auto result = provider.loadFromEnvironment();

    restoreEnv("ACCLOUD_MQTT_TLS_CA_PATH", prevCa);
    restoreEnv("ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH", prevCert);
    restoreEnv("ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH", prevKey);
    restoreEnv("ACCLOUD_MQTT_TLS_DEV_FALLBACK", prevDevFallback);

    if (!expect(result.ok, "TLS provider should resolve local fallback materials")) {
        return false;
    }
    const auto ca = result.paths.caCertificatePath;
    const auto cert = result.paths.clientCertificatePath;
    const auto key = result.paths.clientKeyPath;
    return expect(std::filesystem::exists(ca), "CA fallback file must exist")
        && expect(std::filesystem::exists(cert), "Client cert fallback file must exist")
        && expect(std::filesystem::exists(key), "Client key fallback file must exist")
        && expect(ca.string().find("accloud/resources/mqtt/tls") != std::string::npos,
                  "Fallback path should target accloud/resources/mqtt/tls");
}

bool test_printer_subscription_topics_match_spec() {
    const auto topics = accloud::mqtt::routing::MqttTopicBuilder::buildPrinterSubscriptionTopics(
        "m7", "101001");
    if (!expect(topics.size() == 2, "Printer topics should contain the nominal wildcards")) {
        return false;
    }
    std::set<std::string> unique(topics.begin(), topics.end());
    return expect(unique.size() == topics.size(), "Printer topics should stay unique")
        && expect(unique.contains("anycubic/anycubicCloud/v1/printer/public/m7/101001/#"),
                  "Public wildcard topic should be present")
        && expect(unique.contains("anycubic/anycubicCloud/v1/server/printer/m7/101001/#"),
                  "Server printer wildcard topic should be present");
}

bool test_subscription_profile_has_6_topics_for_two_printers_fixture() {
    // Baseline contract as of 2026-05-01:
    // 2 user topics + (2 printer topics x 2 printers) = 6.
    // If Anycubic changes topic contract, update this assertion in the same commit.
    const std::string userId = "u-123";
    const std::string userIdMd5 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    std::vector<std::string> topics =
        accloud::mqtt::routing::MqttTopicBuilder::buildUserReportTopics(userId, userIdMd5);
    const auto p1 =
        accloud::mqtt::routing::MqttTopicBuilder::buildPrinterSubscriptionTopics("m7", "101001");
    const auto p2 =
        accloud::mqtt::routing::MqttTopicBuilder::buildPrinterSubscriptionTopics("m7pro", "101002");
    topics.insert(topics.end(), p1.begin(), p1.end());
    topics.insert(topics.end(), p2.begin(), p2.end());

    std::set<std::string> unique(topics.begin(), topics.end());
    return expect(topics.size() == 6, "Fixture with 2 printers must produce 6 subscribed topics")
        && expect(unique.size() == 6, "Subscription topics must stay unique in fixture")
        && expect(unique.contains("anycubic/anycubicCloud/v1/server/app/u-123/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/slice/report"),
                  "User slice report topic must be present")
        && expect(unique.contains("anycubic/anycubicCloud/v1/server/app/u-123/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/fdmslice/report"),
                  "User fdm slice report topic must be present")
        && expect(unique.contains("anycubic/anycubicCloud/v1/printer/public/m7/101001/#"),
                  "Printer 1 public wildcard topic must be present")
        && expect(unique.contains("anycubic/anycubicCloud/v1/server/printer/m7/101001/#"),
                  "Printer 1 server printer wildcard topic must be present")
        && expect(unique.contains("anycubic/anycubicCloud/v1/printer/public/m7pro/101002/#"),
                  "Printer 2 public wildcard topic must be present")
        && expect(unique.contains("anycubic/anycubicCloud/v1/server/printer/m7pro/101002/#"),
                  "Printer 2 server printer wildcard topic must be present");
}

bool test_subscription_profile_keeps_expected_topic_families() {
    const std::string userId = "u-123";
    const std::string userIdMd5 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    std::vector<std::string> topics =
        accloud::mqtt::routing::MqttTopicBuilder::buildUserReportTopics(userId, userIdMd5);
    const auto p1 =
        accloud::mqtt::routing::MqttTopicBuilder::buildPrinterSubscriptionTopics("m7", "101001");
    topics.insert(topics.end(), p1.begin(), p1.end());

    bool hasSliceReport = false;
    bool hasFdmSliceReport = false;
    bool hasPublicWildcardFamily = false;
    bool hasServerPrinterWildcardFamily = false;
    for (const auto& topic : topics) {
        if (topic.find("/slice/report") != std::string::npos) {
            hasSliceReport = true;
        }
        if (topic.find("/fdmslice/report") != std::string::npos) {
            hasFdmSliceReport = true;
        }
        if (topic.find("/v1/printer/public/") != std::string::npos
            && topic.size() >= 2 && topic.substr(topic.size() - 2) == "/#") {
            hasPublicWildcardFamily = true;
        }
        if (topic.find("/v1/server/printer/") != std::string::npos
            && topic.size() >= 2 && topic.substr(topic.size() - 2) == "/#") {
            hasServerPrinterWildcardFamily = true;
        }
    }

    return expect(hasSliceReport, "User slice/report family must exist")
        && expect(hasFdmSliceReport, "User fdmslice/report family must exist")
        && expect(hasPublicWildcardFamily, "v1 printer/public wildcard family must exist")
        && expect(hasServerPrinterWildcardFamily, "v1 server/printer wildcard family must exist");
}

bool test_router_extracts_printer_key_across_topic_families() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string payload = R"json({"type":"status","action":"workReport","state":"busy","data":{}})json";
    const auto r1 = router.route("anycubic/anycubicCloud/v1/printer/app/m7/key-app/status/report", payload);
    const auto r2 = router.route("anycubic/anycubicCloud/v1/printer/public/m7/key-public/status/report", payload);
    const auto r3 = router.route("anycubic/anycubicCloud/v1/x/public/m7/key-plus/status/report", payload);
    const auto r4 = router.route("anycubic/anycubicCloud/v1/slicer/printer/m7/key-slicer/status/report", payload);
    const auto r5 = router.route("anycubic/anycubicCloud/v1/server/printer/m7/101001/status", payload);
    const auto r6 = router.route("anycubic/anycubicCloud/v1/+/printer/m7/101002/print", payload);
    const auto r7 = router.route("anycubic/anycubicCloud/printer/public/m7/101003/online/status", payload);
    const auto r8 = router.route("anycubic/anycubicCloud/+/printer/m7/101004/print", payload);
    return expect(r1.printerKey == "key-app", "printer/app key extraction failed")
        && expect(r2.printerKey == "key-public", "printer/public key extraction failed")
        && expect(r3.printerKey == "key-plus", "+/public key extraction failed")
        && expect(r4.printerKey == "key-slicer", "slicer/printer key extraction failed")
        && expect(r5.printerKey == "101001", "v1 server/printer key extraction failed")
        && expect(r6.printerKey == "101002", "v1 +/printer key extraction failed")
        && expect(r7.printerKey == "101003", "legacy printer/public key extraction failed")
        && expect(r8.printerKey == "101004", "legacy +/printer key extraction failed");
}

bool test_telemetry_observation_store_tracks_unknown_signatures() {
    auto& store = accloud::mqtt::observability::TelemetryObservationStore::instance();
    store.clear();

    store.observe("releaseFilm|report|-",
                  "anycubic/anycubicCloud/v1/printer/public/m7/prod-key-01/releaseFilm/report",
                  "prod-key-01",
                  R"json({"type":"releaseFilm","state":"warn"})json",
                  "UnknownMessage",
                  "unknown_type");
    store.observe("releaseFilm|report|-",
                  "anycubic/anycubicCloud/v1/printer/public/m7/prod-key-01/releaseFilm/report",
                  "prod-key-01",
                  R"json({"type":"releaseFilm","state":"warn"})json",
                  "UnknownMessage",
                  "unknown_type");
    store.observe("wifi|status|-",
                  "anycubic/anycubicCloud/v1/printer/public/m7/prod-key-01/wifi/report",
                  "prod-key-01",
                  R"json({"type":"wifi","state":"ok"})json",
                  "UnknownMessage",
                  "unknown_type");

    const auto top = store.topByCount(2);
    if (!expect(top.size() == 2, "Discovery store top list should return two signatures")) {
        return false;
    }
    if (!expect(top[0].signature == "releaseFilm|report|-", "Most frequent signature should be first")) {
        return false;
    }
    if (!expect(top[0].count == 2, "Most frequent signature count should be 2")) {
        return false;
    }
    const auto releaseFilm = store.getBySignature("releaseFilm|report|-");
    if (!expect(releaseFilm.has_value(), "releaseFilm signature must be retrievable")) {
        return false;
    }
    return expect(releaseFilm->topic.find("/releaseFilm/") != std::string::npos,
                  "Stored observation should keep topic")
        && expect(releaseFilm->payload.find("\"releaseFilm\"") != std::string::npos,
                  "Stored observation should keep payload sample");
}

} // namespace

int main() {
    bool ok = true;
    ok = test_route_extracts_realtime_fields() && ok;
    ok = test_store_applies_metrics() && ok;
    ok = test_router_routes_release_film_status() && ok;
    ok = test_router_accepts_documented_local_file_list_payload() && ok;
    ok = test_store_applies_release_film_status() && ok;
    ok = test_router_infers_type_from_topic_when_payload_omits_type() && ok;
    ok = test_router_promotes_wifi_resin_video_types() && ok;
    ok = test_router_extracts_resin_feed_message() && ok;
    ok = test_router_extracts_m7_print_workflow_fields() && ok;
    ok = test_router_extracts_m7_check_status_maps() && ok;
    ok = test_store_tracks_resin_fill_and_runtime_message() && ok;
    ok = test_store_tracks_m7_print_workflow_by_taskid() && ok;
    ok = test_store_links_command_sent_to_first_mqtt_taskid() && ok;
    ok = test_nominal_m7_workflow_routes_store_and_replicates_ui_fields() && ok;
    ok = test_overlay_matches_printer_key_fallback() && ok;
    ok = test_tls_provider_local_fallback_paths() && ok;
    ok = test_printer_subscription_topics_match_spec() && ok;
    ok = test_subscription_profile_has_6_topics_for_two_printers_fixture() && ok;
    ok = test_subscription_profile_keeps_expected_topic_families() && ok;
    ok = test_router_extracts_printer_key_across_topic_families() && ok;
    ok = test_telemetry_observation_store_tracks_unknown_signatures() && ok;
    if (!ok) {
        return 1;
    }
    std::cout << "MQTT flow tests passed\n";
    return 0;
}
