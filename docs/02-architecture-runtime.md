# Architecture and active runtime

## In brief

The application follows a layered C++ architecture. QML renders state and sends user intent; C++ owns use cases, protocols, storage and security.

```text
QML pages and dialogs
        ↓
Qt bridges and UI models
        ↓
application use cases and realtime store
        ↓
cloud / MQTT / cache / logging / formats
```

## Module boundaries

| Path | Responsibility |
| --- | --- |
| `src/accloud/app/` | bootstrap, Qt bridges, UI models and use-case coordination |
| `src/accloud/domain/` | stable business vocabulary and contracts |
| `src/accloud/infra/` | HTTP cloud, MQTT, storage, cache, logs and file formats |
| `src/accloud/render3d/` | OpenGL and Qt Quick rendering foundation |
| `src/accloud/ui/qml/` | visual shell, pages, dialogs and controls |
| `tests/` | C++ and QML regression tests |

Responsibilities are corrected in their current owner. A cloud issue is not moved to QML, and a cloud-only fix does not modify MQTT or render3d.

## Executable and entry points

CMake builds the shared executable `accloud_cli`.

`src/accloud/app/main.cpp` selects:

```text
--smoke or --import-har
-> CLI execution through App

no CLI flag and Qt available
-> QGuiApplication
-> bridge creation
-> qrc:/qml/MainWindow.qml
```

The desktop bootstrap exposes `SessionImportBridge`, `CloudBridge`, `MqttBridge`, `UiSettingsBridge`, `AppI18nBridge` and registered UI models to QML. Debug-only objects are exposed only when `ACCLOUD_DEBUG` is enabled.

## QML resources

Production resources are declared in `src/accloud/app/resources.qrc`. Debug pages are declared separately in `resources_debug.qrc` and compiled only in debug-enabled builds.

A QML correction is valid only when the file is included in the resource set used by the target preset.

## Build modes

| Preset | Build | Debug tooling |
| --- | --- | --- |
| `default` | Debug | excluded |
| `dev-debug` | Debug | included |
| `prod` | Release | excluded |

Production must never depend on `LogBridge`, `LogTailModel`, `UiClickTracer` or debug-only QML resources.

## State authority

```text
HTTP/cloud  = full resynchronisation authority
MQTT        = live transition authority
cache       = explicitly labelled fallback only
```

A stale MQTT update must not overwrite a newer HTTP resync. HTTP acceptance must not erase a later MQTT failure.

## Threading rule

Long operations must be asynchronous from the GUI thread. QML must not perform network requests, large payload parsing, SQLite transactions, credential generation or retry loops.

## Experimental boundary

Photon/PWMB parsing and render3d are partial or experimental. See [the viewer appendix](appendices/photon-viewer-formats.md). They must not become implicit dependencies of cloud or MQTT fixes.
