# QML interface and internationalisation

## In brief

QML owns presentation, navigation and user interaction. Bridges and Qt models expose operations and structured state. Network protocols, persistence and heavy processing stay in C++.

## Active views

The current shell includes cloud login/session settings, cloud files, printers, MQTT status and settings. Debug builds may add log and diagnostic pages. When the experimental viewer is enabled, each compatible PWSZ row exposes a **3D** action that downloads the file to a temporary local path and opens a dedicated modal; it does not add a fifth primary tab.

The main file loaded by the desktop runtime is:

```text
qrc:/qml/MainWindow.qml
```

## Responsibility rule

```text
QML         presentation, visual state, navigation, input
Qt bridges  UI-facing operations and signals
use cases   business coordination
infra       HTTP, MQTT, storage, cache, formats and logs
models      structured list/table state for QML
```

QML must not issue HTTP requests, parse large payloads, open SQLite transactions, generate MQTT credentials or implement network retry policies. Image components consume only local `file://`, `qrc:/` or inline `data:image` sources prepared by the bridge; remote thumbnail URLs are never retried directly by QML.

## Long operations

Uploads, downloads, cloud synchronisation, cache work and format decoding must not block the GUI thread. Busy state, progress, cancellation and errors are exposed through bridge properties and signals.

Remote-print preparation is owned by `PrintWorkflowBridge` and `PrepareRemotePrintUseCase`. The workflow bridge creates the correlation identifier, requests compatibility by cloud file identifier, falls back to extension when required, rejects stale completions, filters compatible printers and selects the preferred compatible printer. `CloudBridge` only executes the asynchronous cloud compatibility calls and returns their results to the workflow bridge; QML receives one semantic preparation result and does not correlate network callbacks. Local compatibility scoring has a single C++ implementation in `PrinterFileCompatibility`, shared by the preparation use case and the `CloudBridge` compatibility adapter; QML no longer tokenizes or scores machine metadata.

MQTT routing, order correlation and the normalized realtime store continue while diagnostic pages are hidden. Diagnostics-only notifications, telemetry text formatting and raw-tail model resets are enabled only while the MQTT page is active; messages received while hidden remain in the bounded C++ history and are published as one synchronized model when the page reopens. The printers page defers MQTT-driven cache projection while hidden, performs one asynchronous catch-up when reopened, and stops its periodic cloud refresh outside the active tab. The log page polls its JSONL snapshot only while the page is active and visible.

Printer selection data is held by C++ list models rather than rebuilt QML `ListModel` payloads. Compatible-printer rows use `PrintersModel`; cloud-print and printer-local file rows use `PrinterFilesModel`. Stable identities are patched with `dataChanged`, tail additions/removals use row deltas, and only identity reordering falls back to a model reset. Raw file metadata remains available through `get()` for remote-print preparation.

The remote-print confirmation dialog receives the complete row already held by `CloudFilesModel` when **Print** is invoked from the cloud-file list. The file name, estimated print time and resin usage are therefore available as soon as the dialog opens, without waiting for another cloud synchronization. While preparation runs in `PrintWorkflowBridge`, the selector can display the preferred printer from the main printer model; once the semantic preparation result arrives, QML replaces the compatible-printer model from that result and resynchronizes the selected identifier. The dialog does not offer file replacement because the originating action fixes the file.

When this entry point is used before the Printers page has been initialized, it bootstraps only the printer list required for compatibility and selection. It does not trigger the startup printer-insights or recent-jobs refresh. Full details and job history remain owned by the Printers page when that page is explicitly activated, preventing remote-print preparation from causing unrelated project-thumbnail traffic.

The recent-jobs list derives its badge from the project `printStatus`. A task is promoted to **In progress** only when the matching live project explicitly has status `1`, or when the matching MQTT task identifier is accompanied by an active workflow stage. A terminal project is never treated as live merely because it is the first or most recent history row.

The Printers page follows the same visual hierarchy as the Files page: a compact refresh action, a live fleet summary and name-only printer tabs. The selected-printer header no longer repeats the printer name and exposes only **Local Files**, followed by the printer status. **Local Files** is disabled when the selected printer cannot accept the operation, including when it is offline, and its disabled tone follows the offline status. The tooltip then exposes the blocking reason. The former **Details** action and its dedicated JSON dialog are removed. The status chip keeps its localized label while deriving its tone from the semantic printer state, so an online ready printer uses the theme success color. The device information remains grouped at the top of its card, without distributing unused height between sections.

Recent jobs remain text-only and never resolve thumbnails or depend on the original cloud file still existing. The table uses stable aligned columns for file, print date, duration and status. Only the start date is displayed, without a time or end timestamp. The technical task identifier is instantiated only when `accloudBuildDebugEnabled` is true; production builds omit that row entirely. The former **UI debug** checkbox, QML section tags and endpoint JSON panel have been removed from the Printers page.

The toolbar distinguishes **Add to cloud** from **Direct print**. Standard upload ends after cloud registration and never presents a post-print cleanup option. Direct print keeps the selected local file as an operation input, checks printer compatibility by extension, uploads it, then automatically sends the print order after the cloud file becomes ready. Its cleanup checkbox belongs to that single operation: after a confirmed successful print, ACM deletes the exact printer-local file first and deletes the cloud file only after MQTT confirms `deleteLocal`.

The Settings menu includes **Delete printer-local copy when a direct print fails**, disabled by default. Its value is snapshotted when a direct operation is launched and has no effect on standard uploads, cloud-list printing, or direct prints without cleanup selected. For a direct task that actually reached the active state and later ends with status 3 or 4, enabling the preference permits only the exact printer-local copy to be removed; the cloud file is always retained on failure, stop or cancellation.

The Settings menu also exposes **3D generation workers** and **3D colors** when the viewer is available. The persisted `render3d.workerCount` value defaults to `4`, is clamped to `1..16`, and is applied to every newly loaded PWSZ. Increasing it raises CPU and memory pressure; changing it does not alter a mesh already being generated.

The persisted `render3d.palettePreset` value selects one of five paired part/background palettes: `technical_cyan`, `industrial_amber`, `mineral_ivory`, `night_coral` or `light_graphite`. Invalid persisted values are normalized to `technical_cyan`. Applying a preset updates both colors together, propagates to an already open viewer, and schedules only a repaint through the existing `meshColor` and `backgroundColor` properties; it does not reload the PWSZ or rebuild the mesh. Free-form color entry is not exposed.

In the 3D modal, the layer range is exposed as a vertical dual-handle slider on the right side of the viewport, visibly bounded by the maximum layer and layer `1`. Hovering a handle shows its layer number in a tooltip. Wheel input over the control changes only the upper bound; the lower bound remains mouse-drag only. No redundant numeric bound fields are displayed.

Direct-print persistence, project reconciliation and cleanup state transitions are owned by `PrintWorkflowBridge`, backed by `PendingDirectPrintStore` and the typed `DirectPrintLifecycleUseCase`. QML only registers an accepted direct print and forwards refreshed project snapshots to the workflow; it no longer keeps a direct-print cleanup map, matches projects, dispatches printer-local deletion, correlates cloud deletion, or interprets cleanup transitions. `CloudBridge` no longer exposes direct-print persistence methods. Asynchronous printer-order correlation uses a structured `QVariantMap` context (`kind` plus typed fields) instead of concatenated identifiers parsed with string prefixes or `split(":")`. MQTT `deleteLocal` confirmations are wired directly from `MqttBridge` to `PrintWorkflowBridge`; the bridge owns the pending `msgId` correlation and both local/cloud delete in-flight state. Direct local/cloud transport intents are wired from the workflow to `CloudBridge` in the desktop bootstrap, while QML receives only semantic cleanup notices and tracking-release signals. Standard cloud-list printing uses the same workflow boundary for post-print cleanup: QML only reports the completed printer task, `PrintWorkflowBridge` requests deletion of the printer-local file first, requests cloud deletion only after the local order succeeds, and emits semantic completion/failure notices. The QML pending remote-print map remains only as a temporary visual telemetry placeholder for an accepted print until live project data arrives.

PWSZ preview completion is controlled by two persisted settings: completion itself is enabled by default, and confirmation before permanent local replacement is enabled by default. The confirmation dialog explains that `preview_1.png` is copied to `preview_2.png`, the prepared version is uploaded, and the local file is replaced only after cloud success. “Do not ask again” disables only the confirmation; both settings remain available from the Settings menu.

## Cloud file multi-selection

Each cloud-file row exposes an independent checkbox. The selected file identifiers and display names are kept as page state, separately from the single row used by the details view. When at least one file is selected, a destructive `Delete (N)` action appears between Refresh and Add to cloud.

The action always requires explicit confirmation. `CloudFilesPage.qml` sends the selected rows once to `CloudFilesWorkflowBridge`; it does not own the queue, current file identifier, failure accumulator or per-file callback correlation. The bridge uses the typed `DeleteCloudFilesUseCase` to preserve selection order, request exactly one asynchronous deletion at a time, ignore stale completions and aggregate failures. `CloudBridge` remains responsible for the actual cloud/cache deletion. QML receives only semantic started/progress/success/finished signals, removes successful items from the visual selection, refreshes the list once after the sequence and translates the final complete/partial/failed summary.

## Cloud file download destination

The cloud-file download action uses the application-owned `DownloadFileDialog.qml` instead of the desktop native save picker. The dialog therefore follows the active ACM palette and control styling on every supported desktop environment. It opens in the standard Downloads folder when available. The left card provides a lazily loaded expandable folder tree; the **Content** view groups subfolders of the current directory first, followed only by files matching the cloud file extension. Home and Downloads shortcuts remain available, without an Up button or a redundant destination summary line.

The complete cloud file name, including its original extension, is prefilled before the signed URL is requested. If the user removes the extension, the original suffix is restored when the destination is built. Only a base file name is accepted from the editable field; path separators are discarded before the final local path is sent to `CloudBridge::startDownload()`. Double-clicking a folder opens it; double-clicking a matching file reuses its name and triggers replacement confirmation when required.

## Resources and production separation

`resources.qrc` contains normal UI resources. `resources_debug.qrc` contains debug-only pages. Production cannot depend on debug objects or raw payload views.

Dialogs normalize a missing Qt overlay to `null`, and shell status forwarding is guarded during object teardown. This prevents QObject/QJSValue and null-root warnings in standalone QML tests without changing the visible runtime behavior.

## Internationalisation

The active catalogs are:

```text
i18n/accloud_en.ts
i18n/accloud_fr.ts
```

These are the only active TS catalogs. Copies under `accloud/i18n/` are invalid because CMake does not load them.

User-visible text uses the existing Qt translation mechanism. Source strings and both catalogs are reviewed together. Debug-only wording stays excluded from production when debug tooling is disabled.

## Performance principles

- avoid eager thumbnail downloads during startup;
- do not rebuild large models for a small change;
- limit log and MQTT tail work on the GUI thread;
- load expensive pages on demand;
- prefer measured corrections over speculative refactors.

Detailed performance notes remain in the UI performance appendix.

## Cloud PWSZ modification proposal

When a completed thumbnail refresh reports invalid PWSZ placeholders, the bridge emits one suggestion containing only file identifiers, display names and sizes. The modal states the number and total volume of affected files and requires explicit confirmation. Progress and the final counts for modified, already-compliant, failed and partial items are delivered by bridge signals; QML does not implement the transfer or deletion sequence. The progress modal exposes a cancellation action that only sets the bridge cancellation token. The C++ workflow remains responsible for aborting active transfers, preserving the original file and returning a cancelled or partial result.

Progress phases cross the bridge as stable keys (`pwsz.update.*`) and are translated only in QML. The result dialog distinguishes success, cancellation and completion with issues. For every failed, partial or cancelled item it displays the file name, status, backend detail, original cloud identifier and replacement identifier when one exists. An incomplete cloud inventory is surfaced as a warning and suppresses the batch proposal.

## Cloud file details

The details dialog prioritizes information useful for printing: the locally resolved preview, format, size, upload date, translated status, machine, material, duration, layer profile, exposure, resin usage and compatible printers. The preview and eight primary values are grouped in a compact fixed-height summary so the tabbed detail area always remains visible. QML consumes only the thumbnail source already resolved by the bridge cache and does not retry remote image URLs.

The persistent `ui.cloudFiles.showAdvancedDetails` option, exposed from **Settings > Show technical file details**, adds a technical tab for file identifiers, status code, technical timestamps, region and slice MD5. It defaults to disabled in production and enabled with `--debug-ui`, unless the user has already persisted a value.

A separate **Cloud Metadata** tab is visible only in development builds with `ACCLOUD_DEBUG` enabled. It exposes the raw cloud identity, timestamps, region, bucket and object path required for diagnosis. Signed download URLs and local thumbnail-cache paths are never rendered. Every tab uses two equal-width cards that share the full available panel height. Detail rows keep their natural height with fixed compact spacing and remain grouped at the top of each card; only the unused area below the final row expands. Panels scroll only when their content actually overflows. No rename action is exposed because no observed Anycubic rename endpoint is part of the runtime contract. Delete remains isolated on the left of the footer, while the consistently sized Close, Download and Print actions are ordered on the right so Print remains the final primary action.
