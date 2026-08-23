# MQTT topics used by the active runtime

> Status: ACTIVE technical appendix. The topic patterns below come from `MqttTopicBuilder.cpp`; they are not a generic MQTT convention.

## Reading the placeholders

- `<userId>`: Anycubic account identifier;
- `<md5(userId)>`: lower-case MD5 used by the observed account report topic;
- `<machineType>`: printer family reported by the cloud dashboard;
- `<deviceId>`: printer identifier;
- `<endpoint>`: observed command or report family.

Identifiers must be redacted in logs, reports and public reference data.

## User report subscriptions

The runtime subscribes to:

```text
anycubic/anycubicCloud/v1/server/app/<userId>/<md5(userId)>/slice/report
```

This is the resin slicing report associated with the account. The FDM-only `fdmslice/report` family is intentionally not subscribed by this application.

## Default printer subscriptions

For each printer returned by the cloud dashboard:

```text
anycubic/anycubicCloud/v1/printer/public/<machineType>/<deviceId>/#
```

This public wildcard is the normal resin runtime path. The observed broker rejects `v1/server/printer/<machineType>/<deviceId>/#` with SUBACK `0x80`, so that wildcard is excluded from the nominal profile.

## Extended discovery subscriptions

`ACCLOUD_MQTT_EXTENDED_TOPICS=1` enables additional observed variants for controlled diagnosis. It must not replace the default subscriptions. This mode also re-enables the rejected compatibility wildcard below, which may legitimately receive SUBACK `0x80` on resin accounts:

```text
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceId>/#
```

Command/report families:

```text
anycubic/anycubicCloud/v1/+/printer/<machineType>/<deviceId>/<endpoint>
```

Supported `<endpoint>` values in the active builder:

```text
airpure
autoOperation
axis
exposure
file
network
ota
print
releaseFilm
residual
resin
response
smartResinVat
wifi
```

Server families:

```text
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceId>/status
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceId>/user
anycubic/anycubicCloud/v1/server/printer/<machineType>/<deviceId>/video
```

Legacy variants still observed on some firmware branches:

```text
anycubic/anycubicCloud/+/printer/<machineType>/<deviceId>/print
anycubic/anycubicCloud/printer/public/<machineType>/<deviceId>/online/status
```

## Publish topic

Printer commands are published through:

```text
anycubic/anycubicCloud/v1/printer/public/<machineType>/<deviceId>/<endpoint>
```

The endpoint is selected by the owning command use case. QML must not build this topic.

## Routing rule

A key or payload field alone is not enough to identify an event. Routing keeps the combined context:

```text
topic + phase + action + state + type + taskid
```

Unknown messages remain non-fatal. They may be recorded as redacted observations containing a topic signature, disposition, frequency and bounded sample.

## Source of truth

- builder: `src/accloud/infra/mqtt/routing/MqttTopicBuilder.cpp`;
- router: `src/accloud/infra/mqtt/routing/MqttMessageRouter.cpp`;
- runtime subscription owner: `src/accloud/app/mqtt/MqttBridgeSession.cpp` (`MqttBridge.cpp` remains the QML-facing facade).
