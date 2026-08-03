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
| `src/accloud/render3d/` | experimental OpenGL/Qt Quick scaffold, excluded from the production runtime |
| `src/accloud/ui/qml/` | visual shell, pages, dialogs and controls |
| `tests/` | C++ and QML regression tests |

Responsibilities are corrected in their current owner. A cloud issue is not moved to QML, and a cloud-only fix does not modify MQTT or render3d.

The cloud infrastructure is split by API owner. `CloudClient` is the compatibility entry point used by application use cases, while `AuthApi`, `FilesApi`, `QuotaApi`, `DownloadsApi`, `PrintersApi`, `ProjectsApi`, `ReasonCatalogApi` and `PrintOrderApi` own their endpoint-specific payload construction and response parsing. `ApiSupport` is deliberately limited to Workbench transport and generic JSON conversion helpers. There is no shared `CloudLegacyImpl` backend.

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

The desktop bootstrap exposes `SessionImportBridge`, `CloudBridge`, `CloudFilesWorkflowBridge`, `PrintWorkflowBridge`, `MqttBridge`, `UiSettingsBridge`, `AppI18nBridge` and registered UI models to QML. It wires `MqttBridge::printerFileActionReceived` directly to `PrintWorkflowBridge::handlePrinterFileAction` so direct-print cleanup confirmation is processed in C++ rather than QML. It also routes printer-order completions, cloud-delete completions and remote-print compatibility completions from `CloudBridge` into `PrintWorkflowBridge`; transport intents emitted by the print workflow are forwarded back to `CloudBridge`. Cloud-file deletion is routed separately through `CloudFilesWorkflowBridge`: the bridge owns single-delete correlation and the sequential batch lifecycle backed by `DeleteCloudFilesUseCase`, while `CloudBridge` remains the transport/cache executor. QML therefore receives semantic progress and completion signals instead of correlating individual delete callbacks or maintaining a delete queue. Debug-only objects are exposed only when `ACCLOUD_DEBUG` is enabled.

`CloudBridge` is now a thin compatibility facade rather than the owner of every cloud-side mechanism. Model-to-`QVariant` mapping and message normalization live in `CloudBridgeSupport`; thumbnail resolution/cache and the thumbnail-only TLS exception live in `ThumbnailService`; signed user-file transfers live in `CloudDownloadController`; upload and PWSZ preview-update lifecycles live in `CloudUploadController`. `CloudBridge` retains the public QML contract, cache synchronization, asynchronous cloud refresh coordination and printer/file command delegation. This split keeps signed downloads free of Workbench headers and prevents image/TLS/PWSZ implementation details from leaking back into the UI-facing facade.

`LocalCacheStore` remains the compatibility facade used by the application and workflows, but its SQLite implementation is split by ownership. `LocalCacheSql` owns connection helpers, schema creation and migrations; `LocalCacheFiles`, `LocalCachePrinters`, `LocalCacheJobs` and `LocalCacheState` own their respective queries and transactions. Runtime and SQL regression tests compile the same `ACCLOUD_LOCAL_CACHE_SOURCES` set so the tested persistence implementation cannot drift from the desktop build.


`MqttBridge` remains the QML-facing compatibility facade, but its implementation is split by runtime ownership. `MqttBridgeSession` owns profile preparation, the frozen Anycubic broker configuration, connection lifecycle and dynamic subscriptions; `MqttBridgeMessages` owns redacted capture, routing, printer-file events, realtime-store application and HTTP/MQTT order correlation; `MqttBridgeTelemetry` owns diagnostics buffers, counters and telemetry snapshots. The facade keeps only QObject construction, public properties/signals and small state setters. CMake compiles these units through the single `ACCLOUD_MQTT_BRIDGE_SOURCES` set.

## QML resources

Production resources are declared in `src/accloud/app/resources.qrc`. Debug pages are declared separately in `resources_debug.qrc` and compiled only in debug-enabled builds. `VolumeViewerPage.qml` and `VolumeViewerDialog.qml` are packaged with the normal resource bundle. Development desktop presets register `Accloud.Render3D` and expose a 3D action on every PWSZ row. Production keeps `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`; the PWSZ action remains visible but disabled so the capability is not silently hidden.

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

Photon/PWMB parsing, viewer jobs/cache and `render3d` remain outside `accloud_infra`. Normal presets do not link them to `accloud_cli` and expose no viewer action.

`ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` still defaults to `OFF` for direct CMake configurations. The `default` preset explicitly enables it; `dev-debug` and `local-full` inherit that setting, while `prod` and `protected-core` explicitly disable it. The object target remains named `accloud_experimental_viewer`. `experimental-viewer-core` builds the backend-agnostic PWSZ/mesh core without Qt, and `experimental-viewer-qt` remains the explicit Qt/OpenGL validation preset. Enabled desktop builds link the core to `accloud_cli`, register `Accloud.Render3D/VolumeViewer`, force the Qt Quick scene graph to OpenGL and show a 3D button on every PWSZ file row. The action downloads the file to a temporary path and opens a viewer dialog. The desktop path performs PWSZ reading and meshing outside the GUI thread, feeds a bounded `UploadQueue`, uploads layer chunks to OpenGL buffers and clips the rendered mesh to the exact inclusive layer range selected in QML. Mesh chunks are processed by four workers by default. The persisted user setting `render3d.workerCount` accepts values from 1 through 16; PWSZ mask reads are concurrent because each archive entry is reopened independently, while sources that do not advertise concurrent reads are serialized by the mesher. The default preview samples one source layer out of two while preserving the exact first/last selected layers and original Z extent; the UI can rebuild at full per-layer fidelity. Structured generation diagnostics are written with source `render3d` to the dedicated `render3d.jsonl` sink, including requested/effective worker counts and per-worker statistics. Production remains excluded pending the performance and robustness gates.

See [the viewer appendix](appendices/photon-viewer-formats.md). Experimental viewer code must not become an implicit dependency of cloud, MQTT or production UI fixes.
