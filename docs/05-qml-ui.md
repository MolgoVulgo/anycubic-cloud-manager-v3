# QML interface and internationalisation

## In brief

QML owns presentation, navigation and user interaction. Bridges and Qt models expose operations and structured state. Network protocols, persistence and heavy processing stay in C++.

## Active views

The current shell includes cloud login/session settings, cloud files, printers, MQTT status, settings and the experimental viewer. Debug builds may add log and debug pages.

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

Remote-print compatibility checks by cloud file identifier and file extension run in `CloudBridge` background tasks. Each request carries a correlation identifier; QML consumes only the matching completion signal and ignores stale responses. The synchronous compatibility methods remain available for plain test mocks and legacy adapters, but the production QObject bridge uses the asynchronous path.

MQTT routing, order correlation and the normalized realtime store continue while diagnostic pages are hidden. Diagnostics-only notifications, telemetry text formatting and raw-tail model resets are enabled only while the MQTT page is active; messages received while hidden remain in the bounded C++ history and are published as one synchronized model when the page reopens. The printers page defers MQTT-driven cache projection while hidden, performs one asynchronous catch-up when reopened, and stops its periodic cloud refresh outside the active tab. The log page polls its JSONL snapshot only while the page is active and visible.

Printer selection data is held by C++ list models rather than rebuilt QML `ListModel` payloads. Compatible-printer rows use `PrintersModel`; cloud-print and printer-local file rows use `PrinterFilesModel`. Stable identities are patched with `dataChanged`, tail additions/removals use row deltas, and only identity reordering falls back to a model reset. Raw file metadata remains available through `get()` for remote-print preparation.

The remote-print confirmation dialog receives the complete row already held by `CloudFilesModel` when **Print** is invoked from the cloud-file list. The file name, estimated print time and resin usage are therefore available as soon as the dialog opens, without waiting for another cloud synchronization. While the asynchronous compatibility check runs, the selector displays the preferred printer from the main printer model; it then switches to the filtered compatible-printer model and resynchronizes the selected identifier. The dialog does not offer file replacement because the originating action fixes the file.

PWSZ preview completion is controlled by two persisted settings: completion itself is enabled by default, and confirmation before permanent local replacement is enabled by default. The confirmation dialog explains that `preview_1.png` is copied to `preview_2.png`, the prepared version is uploaded, and the local file is replaced only after cloud success. “Do not ask again” disables only the confirmation; both settings remain available from the Settings menu.

## Cloud file multi-selection

Each cloud-file row exposes an independent checkbox. The selected file identifiers and display names are kept as page state, separately from the single row used by the details view. When at least one file is selected, a destructive `Delete (N)` action appears between Refresh and Upload.

The action always requires explicit confirmation. Deletions are then submitted sequentially through the existing asynchronous bridge operation so the GUI thread remains responsive and the current cloud/cache deletion contract is preserved. Successful items are removed from the selection; failed items remain selected. The list is refreshed once after the sequence and the status bar reports complete, partial or failed completion.

## Cloud file download destination

The cloud-file download action uses the application-owned `DownloadFileDialog.qml` instead of the desktop native save picker. The dialog therefore follows the active ACM palette and control styling on every supported desktop environment. It opens in the standard Downloads folder when available. The left card provides a lazily loaded expandable folder tree; the **Content** view groups subfolders of the current directory first, followed only by files matching the cloud file extension. Home and Downloads shortcuts remain available, without an Up button or a redundant destination summary line.

The complete cloud file name, including its original extension, is prefilled before the signed URL is requested. If the user removes the extension, the original suffix is restored when the destination is built. Only a base file name is accepted from the editable field; path separators are discarded before the final local path is sent to `CloudBridge::startDownload()`. Double-clicking a folder opens it; double-clicking a matching file reuses its name and triggers replacement confirmation when required.

## Resources and production separation

`resources.qrc` contains normal UI resources. `resources_debug.qrc` contains debug-only pages. Production cannot depend on debug objects or raw payload views.

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
- load expensive pages or viewer data on demand;
- prefer measured corrections over speculative refactors.

Detailed performance notes remain in the UI performance appendix.

## Cloud PWSZ modification proposal

When a completed thumbnail refresh reports invalid PWSZ placeholders, the bridge emits one suggestion containing only file identifiers, display names and sizes. The modal states the number and total volume of affected files and requires explicit confirmation. Progress and the final counts for modified, already-compliant, failed and partial items are delivered by bridge signals; QML does not implement the transfer or deletion sequence. The progress modal exposes a cancellation action that only sets the bridge cancellation token. The C++ workflow remains responsible for aborting active transfers, preserving the original file and returning a cancelled or partial result.

Progress phases cross the bridge as stable keys (`pwsz.update.*`) and are translated only in QML. The result dialog distinguishes success, cancellation and completion with issues. For every failed, partial or cancelled item it displays the file name, status, backend detail, original cloud identifier and replacement identifier when one exists. An incomplete cloud inventory is surfaced as a warning and suppresses the batch proposal.

## Cloud file details

The details dialog prioritizes information useful for printing: the locally resolved preview, format, size, upload date, translated status, machine, material, duration, layer profile, exposure, resin usage and compatible printers. The preview and eight primary values are grouped in a compact fixed-height summary so the tabbed detail area always remains visible. QML consumes only the thumbnail source already resolved by the bridge cache and does not retry remote image URLs.

The persistent `ui.cloudFiles.showAdvancedDetails` option, exposed from **Settings > Show technical file details**, adds a technical tab for file identifiers, status code, technical timestamps, region and slice MD5. It defaults to disabled in production and enabled with `--debug-ui`, unless the user has already persisted a value.

A separate **Cloud Metadata** tab is visible only in development builds with `ACCLOUD_DEBUG` enabled. It exposes the raw cloud identity, timestamps, region, bucket and object path required for diagnosis. Signed download URLs and local thumbnail-cache paths are never rendered. Every tab uses two equal-width cards that share the full available panel height. Detail rows keep their natural height with fixed compact spacing and remain grouped at the top of each card; only the unused area below the final row expands. Panels scroll only when their content actually overflows. Rename is located in the header; Delete remains isolated on the left of the footer, while the consistently sized Close, Download and Print actions are ordered on the right so Print remains the final primary action.
