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
| `src/accloud/render3d/` | functional experimental PWSZ development viewer, excluded from the production runtime |
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

| Preset | Build | Purpose |
| --- | --- | --- |
| `default` | Debug + Qt | normal desktop development; viewer enabled |
| `dev-debug` | Debug + Qt | desktop development with debug resources and bridges |
| `prod` | Release + Qt | production runtime; debug tooling and viewer disabled |
| `protected-core` | Debug, no Qt | offline portable core gate |
| `local-full` | Debug + strict Qt | complete local non-live Qt/QML/SQL/MQTT gate |
| `experimental-viewer-core` | Debug, no Qt | isolated PWSZ/mesh/viewer-core gate |
| `experimental-viewer-qt` | Debug + strict Qt/OpenGL | complete desktop viewer gate |

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

Photon/PWMB parsing, viewer jobs/cache and `render3d` remain outside `accloud_infra`. Development desktop presets link the isolated viewer target to `accloud_cli` and expose the per-file PWSZ action; `prod` and `protected-core` exclude it.

`ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` still defaults to `OFF` for direct CMake configurations. The `default` preset explicitly enables it; `dev-debug` and `local-full` inherit that setting, while `prod` and `protected-core` explicitly disable it. The object target remains named `accloud_experimental_viewer`. `experimental-viewer-core` builds the backend-agnostic PWSZ/mesh core without Qt, and `experimental-viewer-qt` remains the explicit Qt/OpenGL validation preset. Enabled desktop builds link the core to `accloud_cli`, register `Accloud.Render3D/VolumeViewer`, force the Qt Quick scene graph to OpenGL and show a 3D button on every PWSZ file row. The action downloads the file to a temporary path and opens a viewer dialog. The desktop path performs PWSZ reading and meshing outside the GUI thread, feeds a bounded `UploadQueue`, uploads layer chunks to OpenGL buffers and clips the rendered mesh to the exact inclusive layer range selected in QML. Mesh chunks are processed by four workers by default. The persisted user setting `render3d.workerCount` accepts values from 1 through 16 and is also passed to `SupportAnalyzer` when semantic support analysis is enabled. `SupportAnalyzer` uses one persistent priority scheduler for native-layer preparation and independent semantic work. P6.3 removes the old fixed four-mask preparation cap: the preparation window is the minimum of configured workers, remaining layers and a native-mask memory budget (256 MiB by default), so a 16-worker workstation can prepare up to 16 layers concurrently when the raster size fits that budget. PWSZ mask reads are concurrent because each archive entry is reopened independently; sources that do not advertise concurrent reads serialize only `loadMask()` while the shared scheduler still parallelizes independent component extraction. P6.5 separates immutable geometry from semantic reconciliation: every native `LayerDescription` is prepared once, then all adjacent semantic layer pairs are partitioned into contiguous lots and their sparse geometric evidence graph (overlap, material distance and centre distance) is built concurrently. The forward and reverse semantic commits still reconcile that graph in deterministic layer/component order, so P6.5 is the first graph/lot stage rather than a speculative independent classifier; lot boundaries are connected explicitly by the adjacent-layer edges and need no duplicated halo in this stage. P5 keeps the sparse-run representation authoritative while adding optional AVX2/bitset acceleration with a scalar fallback. P6/P6.1/P6.2 adds an optional Vulkan Compute backend dedicated to translated-overlap counts used by model-lineage search. It is built only when a Vulkan SDK and SPIR-V compiler are available, is independent from the OpenGL renderer, and leaves layer ordering, graph mutation, reconciliation and deterministic commits on the CPU. P6.1 coalesces concurrent large-component requests through one dedicated Vulkan dispatcher and uses persistent staging plus reusable device-local storage buffers instead of serialising each worker behind a per-component GPU mutex. P6.2 increases native GPU occupancy by dispatching a 3D grid (`tile × translation × job`): dense jobs split the word domain into 4096-word tiles, while the main stable-model lineage path sends compact semantic runs in 64-run tiles. The stable model reference is rasterised once per layer in the native raster domain and may remain resident in a reusable device-local Vulkan buffer for all compatible component jobs of that layer, avoiding a dense source bitmap and repeated reference upload per component. Runtime exposes two standard compute modes only: `auto` (hybrid CPU/Vulkan) and `cpu`. `auto` falls back to the canonical CPU path if Vulkan cannot be initialized or a GPU job fails; there is no full-GPU runtime mode. Runtime diagnostics expose the selected device, GPU/fallback and compact-run job counts, resident-reference uploads/reuses, submitted workgroups, batch size, transfer volume and host/queue/execution timings from the beginning of the analysis. P6.3 also publishes live preparation/semantic timings (`support_prepare_load_us`, `support_prepare_describe_us`, `support_forward_semantic_us`, `support_reverse_semantic_us`) plus prepared-layer, window-capacity and maximum-inflight counters, so an analysis cancelled early still identifies whether preparation or ordered semantic work is dominant. P6.4 introduced layer-level zero-shift Vulkan overlap batching and indexed several serial semantic scans. The long-file benchmark then showed that the layer batching regressed `auto`: millions of per-component host requests increased queue/synchronisation cost without enough useful GPU work. The stabilisation following P6.5 removes that P6.4 layer batch from the runtime analyzer entirely: both `auto` and `cpu` use the canonical sparse CPU path for zero-shift exact/envelope counts, while `auto` may use Vulkan only for translated-lineage kernels. The low-level bulk Vulkan primitive remains covered by its backend self-test but is not a selectable runtime mode. P6.5 also sizes child-count, brace-drift and model-contact scratch data from the immediately previous candidate layer instead of cumulative `states.size()`, removing repeated zero-initialisation proportional to the whole historical graph. Dedicated P6.4 sub-phase timings remain available, and P6.5 adds `support_semantic_evidence_us`, `support_semantic_evidence_lots`, `support_semantic_evidence_layer_pairs` and `support_semantic_evidence_edges` to measure the parallel graph-construction stage. The mesher preserves the same source-concurrency contract. The default preview samples one source layer out of two while preserving the exact first/last selected layers and original Z extent. Structured generation diagnostics are written with source `render3d` to the dedicated `render3d.jsonl` sink, including requested/effective worker counts and support-analysis Vulkan activation/dispatch telemetry. Production remains excluded pending the performance and robustness gates.

See [the viewer appendix](appendices/photon-viewer-formats.md). Experimental viewer code must not become an implicit dependency of cloud, MQTT or production UI fixes.
