#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys


def fail(errors: list[str]) -> int:
    print("MqttBridge architecture check failed:")
    for error in errors:
        print(f"- {error}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()
    root = args.repo_root.resolve()
    app = root / "src/accloud/app"
    cmake_path = root / "accloud/CMakeLists.txt"
    cmake = cmake_path.read_text(encoding="utf-8")
    errors: list[str] = []

    components = {
        "MqttBridge.cpp": 220,
        "mqtt/MqttBridgeSession.cpp": 700,
        "mqtt/MqttBridgeMessages.cpp": 550,
        "mqtt/MqttBridgeTelemetry.cpp": 250,
    }
    texts: dict[str, str] = {}
    for relative, budget in components.items():
        path = app / relative
        if not path.is_file():
            errors.append(f"missing MqttBridge implementation unit: {path.relative_to(root)}")
            continue
        text = path.read_text(encoding="utf-8")
        texts[relative] = text
        line_count = len(text.splitlines())
        if line_count > budget:
            errors.append(f"{relative} exceeds the {budget}-line ownership budget ({line_count})")

    facade = texts.get("MqttBridge.cpp", "")
    forbidden_facade_tokens = (
        "nlohmann/json",
        "MqttSessionManager",
        "MqttMessageRouter",
        "MqttCredentialProvider",
        "TlsMaterialProvider",
        "SessionProvider",
        "PrinterRealtimeStore",
        "OrderResponseTracker",
        "MqttTelemetry",
        "TelemetryObservationStore",
        "QCryptographicHash",
        "QRegularExpression",
        "std::filesystem",
        "std::ofstream",
        "mqtt-universe.anycubic.com",
    )
    for token in forbidden_facade_tokens:
        if token in facade:
            errors.append(f"MqttBridge.cpp still owns extracted responsibility token {token!r}")

    required_facade_tokens = (
        "initializeSessionCallbacks();",
        "startBackgroundAutoConnect();",
        "shutdownSession();",
    )
    for token in required_facade_tokens:
        if token not in facade:
            errors.append(f"MqttBridge.cpp missing delegation marker {token!r}")

    session = texts.get("mqtt/MqttBridgeSession.cpp", "")
    session_tokens = (
        "buildPreparedProfile",
        "initializeSessionCallbacks",
        "startBackgroundAutoConnect",
        "shutdownSession",
        "connectRaw",
        "disconnectRaw",
        "ensureAutoConnected",
        "attemptAutoConnect",
        "suggestedConnection",
        "refreshDynamicSubscriptions",
        'out.config.host = "mqtt-universe.anycubic.com"',
        "out.config.port = 8883",
        "out.config.keepAliveSeconds = 1200",
        "out.config.cleanSession = true",
        "MqttAuthMode::Slicer",
        "TlsMaterialProvider",
        "buildUserReportTopics",
        "buildPrinterSubscriptionTopics",
    )
    for token in session_tokens:
        if token not in session:
            errors.append(f"MqttBridgeSession.cpp missing session responsibility marker {token!r}")

    messages = texts.get("mqtt/MqttBridgeMessages.cpp", "")
    message_tokens = (
        "handleIncomingMessage",
        "redactPayloadForDebug",
        "appendMqttCaptureLine",
        "MqttMessageRouter",
        "PrinterRealtimeStore::instance().applyEvent",
        "resolveByMsgid",
        "resolveByFallback",
        "printerFileListReceived",
        "printerFileActionReceived",
    )
    for token in message_tokens:
        if token not in messages:
            errors.append(f"MqttBridgeMessages.cpp missing message responsibility marker {token!r}")

    telemetry = texts.get("mqtt/MqttBridgeTelemetry.cpp", "")
    telemetry_tokens = (
        "setUiDiagnosticsActive",
        "appendRawLine",
        "refreshTelemetrySnapshot",
        "MqttTelemetry::instance().snapshot",
        "TelemetryObservationStore::instance().topByCount",
        "OrderResponseTracker::instance().expireTimeouts",
    )
    for token in telemetry_tokens:
        if token not in telemetry:
            errors.append(f"MqttBridgeTelemetry.cpp missing telemetry responsibility marker {token!r}")

    header_path = app / "MqttBridge.h"
    if not header_path.is_file():
        errors.append("src/accloud/app/MqttBridge.h is missing")
    else:
        header = header_path.read_text(encoding="utf-8")
        for token in (
            "initializeSessionCallbacks",
            "startBackgroundAutoConnect",
            "shutdownSession",
            "handleIncomingMessage",
        ):
            if token not in header:
                errors.append(f"MqttBridge.h missing private decomposition method {token!r}")

    required_cmake_tokens = (
        "set(ACCLOUD_MQTT_BRIDGE_SOURCES",
        "${ACCLOUD_MQTT_BRIDGE_SOURCES}",
        "app/mqtt/MqttBridgeSession.cpp",
        "app/mqtt/MqttBridgeMessages.cpp",
        "app/mqtt/MqttBridgeTelemetry.cpp",
        "NAME accloud_mqtt_bridge_architecture",
        "check_mqtt_bridge_architecture.py",
    )
    for token in required_cmake_tokens:
        if token not in cmake:
            errors.append(f"CMake missing MqttBridge architecture token {token!r}")

    if cmake.count("${ACCLOUD_MQTT_BRIDGE_SOURCES}") != 1:
        errors.append("ACCLOUD_MQTT_BRIDGE_SOURCES must be consumed exactly once by the desktop runtime")
    if session.count("static accloud::mqtt::core::MqttSessionManager manager;") != 1:
        errors.append("MqttBridgeSession.cpp must own exactly one shared MqttSessionManager")
    if messages.count("static accloud::mqtt::routing::MqttMessageRouter router;") != 1:
        errors.append("MqttBridgeMessages.cpp must own exactly one shared MqttMessageRouter")

    if errors:
        return fail(errors)

    print("MqttBridge architecture check passed")
    for relative in components:
        print(f"- {relative}: {len(texts[relative].splitlines())} lines")
    print("- frozen broker/session contract remains in MqttBridgeSession.cpp")
    print("- routing, capture and correlation remain in MqttBridgeMessages.cpp")
    print("- diagnostics and telemetry remain in MqttBridgeTelemetry.cpp")
    return 0


if __name__ == "__main__":
    sys.exit(main())
