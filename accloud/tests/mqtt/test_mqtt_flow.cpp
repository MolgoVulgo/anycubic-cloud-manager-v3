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
    event.progress = 81;
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

bool test_router_marks_unknown_message_for_discovery() {
    accloud::mqtt::routing::MqttMessageRouter router;
    const std::string topic =
        "anycubic/anycubicCloud/v1/printer/public/m7/prod-key-01/releaseFilm/report";
    const std::string payload = R"json(
{"type":"releaseFilm","action":"report","state":"warn","data":{"film_temp":38}}
)json";

    const auto routed = router.route(topic, payload);
    return expect(routed.disposition == accloud::mqtt::routing::RouteDisposition::UnknownMessage,
                  "Unknown releaseFilm messages should go to discovery disposition")
        && expect(!routed.signature.empty(), "Unknown message should still expose a signature")
        && expect(!routed.event.has_value(), "Unknown message should not produce a realtime event");
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
    if (!expect(topics.size() == 2, "Printer topics should contain the stable 2-topic wildcard matrix")) {
        return false;
    }
    std::set<std::string> unique(topics.begin(), topics.end());
    return expect(unique.size() == topics.size(), "Printer topics should stay unique")
        && expect(unique.contains("anycubic/anycubicCloud/v1/printer/public/m7/101001/#"),
                  "Public wildcard topic should be present")
        && expect(unique.contains("anycubic/anycubicCloud/v1/+/public/m7/101001/#"),
                  "+/public wildcard topic should be present");
}

bool test_subscription_profile_has_6_topics_for_two_printers_fixture() {
    // Baseline contract as of 2026-03-15:
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
        && expect(unique.contains("anycubic/anycubicCloud/v1/+/public/m7pro/101002/#"),
                  "Printer 2 +/public wildcard topic must be present");
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
    bool hasPlusPublicWildcardFamily = false;
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
        if (topic.find("/v1/+/public/") != std::string::npos
            && topic.size() >= 2 && topic.substr(topic.size() - 2) == "/#") {
            hasPlusPublicWildcardFamily = true;
        }
    }

    return expect(hasSliceReport, "User slice/report family must exist")
        && expect(hasFdmSliceReport, "User fdmslice/report family must exist")
        && expect(hasPublicWildcardFamily, "v1 printer/public wildcard family must exist")
        && expect(hasPlusPublicWildcardFamily, "v1 +/public wildcard family must exist");
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
    ok = test_router_marks_unknown_message_for_discovery() && ok;
    ok = test_router_infers_type_from_topic_when_payload_omits_type() && ok;
    ok = test_router_promotes_wifi_resin_video_types() && ok;
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
