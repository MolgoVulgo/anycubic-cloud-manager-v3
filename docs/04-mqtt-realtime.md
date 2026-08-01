# MQTT and realtime state

## In brief

HTTP provides a complete state snapshot. MQTT reports live transitions without a new full synchronisation. The local cache is only an explicitly labelled fallback.

The Anycubic broker is not treated as an interchangeable standard MQTT service. The settings below form a **frozen compatibility contract**: they describe the runtime known to work with the observed broker, not an idealised MQTT deployment.

## Runtime flow

```text
raw MQTT message
-> topic classification
-> JSON parsing
-> action/state/type/taskid extraction
-> domain event
-> PrinterRealtimeStore
-> UI overlay
```

Unknown messages must not break the store. They can be retained as redacted observations with topic, signature, disposition, frequency and last-seen timestamp.

## Frozen broker contract

| Parameter | Active value |
| --- | --- |
| Host | `mqtt-universe.anycubic.com` |
| Port | `8883` |
| MQTT protocol | `3.1.1` |
| Transport | encrypted TLS |
| TLS version | `1.2` |
| Client authentication | mTLS: CA + client certificate + private key |
| Credential mode | `slicer` |
| Keepalive | `1200` seconds |
| Clean session | `true` |

Compatibility behaviour currently required by the observed environment:

- `ACCLOUD_MQTT_TLS_ALLOW_INSECURE` defaults to `true`;
- Qt uses `VerifyNone` when that compatibility mode is active;
- the Qt cipher string may use `ALL:@SECLEVEL=0`;
- an `OPENSSL_CONF` profile with `DEFAULT:@SECLEVEL=0` can be generated;
- `ACCLOUD_MQTT_TLS_DEV_FALLBACK` only controls explicit local material lookup under `<repo>/resources/mqtt/tls`;
- no repository TLS material is selected silently when this flag is absent;
- the repository launcher `start.sh` explicitly defaults this flag to `1` for local runs, while preserving caller-provided values and explicit TLS paths;
- CA, client certificate and private key remain required for a normal connection.

`VerifyNone` and `SECLEVEL=0` are MQTT broker compatibility measures. They are not the HTTPS policy of the application and do not authorise disabling verification elsewhere.

Without explicit instruction and a live broker revalidation, do not change host, port, protocol version, keepalive, clean-session behaviour, mTLS, slicer auth, compatibility cipher policy or observed topic families.

## Connection

The runtime loads the session identity and MQTT auth token, derives slicer credentials, validates the external TLS material, configures the frozen session parameters, connects and subscribes to user and printer topic families.

The constructor can prepare the automatic connection asynchronously. A later UI request does not start a second preparation while that background preparation is active; once it finishes, a later request may retry if the profile was not ready.

An internal Android credential generator exists for compatibility analysis. The production profile remains `slicer`; it must not be switched to `android` or automatic selection by cleanup or standardisation work.


## Implementation ownership

`MqttBridge` is a stable Qt/QML facade, not a protocol monolith. Its implementation is compiled from `ACCLOUD_MQTT_BRIDGE_SOURCES`:

- `MqttBridge.cpp` owns QObject construction, public properties and small state setters;
- `MqttBridgeSession.cpp` owns SLICER profile preparation, the frozen broker/TLS/session parameters, connection lifecycle and dynamic topic subscriptions;
- `MqttBridgeMessages.cpp` owns payload redaction/capture, message routing, file-list/action signals, realtime-store updates and HTTP/MQTT order correlation;
- `MqttBridgeTelemetry.cpp` owns diagnostic buffers, telemetry snapshots and timeout-counter refresh.

This split must not duplicate the session manager or router, and it must not move broker configuration or raw MQTT parsing into QML.

## Topics and message context

The nominal resin runtime subscribes only to the account `slice/report` topic and the `v1/printer/public/<machineType>/<deviceId>/#` wildcard for each printer. The FDM-only `fdmslice/report` topic is excluded. The broker-rejected `v1/server/printer/.../#` wildcard is available only in explicit extended discovery mode. Important resin message families include `status`, `print`, `releaseFilm`, `autoOperation` and `wifi`.

A single key never defines an event. Interpretation keeps this context:

```text
topic + phase + action + state + type + taskid
```

See [MQTT topics](appendices/mqtt-topics.md) for the active builders and legacy variants, and [MQTT JSON structures](appendices/mqtt-json-structures.md) for payload vocabulary.

## State arbitration

1. HTTP is authoritative for a complete resync.
2. MQTT is authoritative for live transitions.
3. Cache is authoritative only as labelled fallback.
4. Older MQTT state cannot overwrite newer HTTP state.
5. MQTT failure is visible immediately.
6. HTTP acceptance cannot erase a later MQTT failure.

## Resin print interpretation

The same vocabulary can represent pre-print autoload or refill during printing. Phase and task correlation must be retained. See the MQTT capture appendix for detailed examples.

## Live validation

A broker test requires a valid session, network access, Qt6 MQTT, OpenSSL compatibility and external mTLS material. `acm.zip` intentionally contains no private key. Missing prerequisites are reported as **environment incomplete**, never as a successful broker validation.
