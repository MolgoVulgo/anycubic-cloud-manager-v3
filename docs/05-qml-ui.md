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

PWSZ preview completion is controlled by two persisted settings: completion itself is enabled by default, and confirmation before permanent local replacement is enabled by default. The confirmation dialog explains that `preview_1.png` is copied to `preview_2.png`, the prepared version is uploaded, and the local file is replaced only after cloud success. “Do not ask again” disables only the confirmation; both settings remain available from the Settings menu.

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
