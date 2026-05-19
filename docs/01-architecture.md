# Project architecture

Status: `IMPLEMENTED` for the current layout, `PARTIAL` for the long-term module separation.

## Role

Anycubic Cloud Manager V3 is a Linux desktop application built with C++20, Qt6 and QML. The project combines a desktop UI, an Anycubic Cloud client, MQTT realtime observation, local cache/logging, and the early technical base for Photon/PWMB file viewing.

The repository is not a generic scripting project. Python can exist for tooling, analysis or CI helpers, but the architectural reference remains C++ / Qt / QML with CMake presets.

## Module boundaries

```text
src/accloud/app/       Qt bootstrap, QML bridges, UI-facing models
src/accloud/domain/    stable types, business vocabulary, contracts
src/accloud/infra/     HTTP/cloud, MQTT, cache, logs, file formats, jobs
src/accloud/render3d/  rendering implementation, OpenGL/Qt Quick bridge
src/accloud/ui/qml/    QML shell, pages, dialogs and visual components
```

The boundary rule is direct: QML must render and delegate; it must not become the place where cloud sync, MQTT routing, cache policy or printer workflow logic is implemented.

## Current strengths

- The project has a real CMake-based C++ structure.
- Qt/QML is already the desktop frontend path.
- Cloud behavior is separated into infra APIs, request builders, session handling and use cases.
- MQTT has dedicated routing, credential/TLS handling and realtime-state integration.
- Logging, cache and jobs are represented by explicit infrastructure components.
- Debug tooling can be excluded from production builds through `ACCLOUD_DEBUG`.

## Areas still too concentrated

The historical documentation correctly identifies three concentration risks:

1. legacy cloud code can still hide too much endpoint behavior;
2. bridge classes can become orchestration hubs if UI-specific and domain-specific logic are mixed;
3. sync behavior exists, but its contract must stay explicit: scope, freshness, fallback and reconstruction rules.

The target structure is:

- domain types define what a file, printer, session, job or MQTT event means;
- infra modules speak protocols and storage;
- use cases coordinate actions;
- bridges expose stable asynchronous entry points to QML;
- QML keeps visual state and delegates operations.

## Build modes

| Mode | Intent |
| --- | --- |
| `default` | Debug build, Qt enabled, debug tooling disabled. |
| `dev-debug` | Debug build with debug UI, debug payloads and traces. |
| `prod` | Release build, debug tooling excluded. |

`start.sh` is a convenience wrapper around those presets. It is not the architecture; the CMake presets remain the build reference.

## Decision

The project keeps one active product axis: cloud manager first, MQTT runtime second, Photon viewer as a prepared but not yet complete product path. Documentation must not present a `SPEC` area as delivered behavior.
