# UI performance and latency

Status: `ANALYSIS` and `PLAN`.

## Symptom

The observed symptoms were:

- 1 to 10 seconds before the UI became interactive after startup;
- 1 to 2 seconds when changing tabs;
- file and recent-job lists that could block or stutter;
- periodic wakeups from MQTT/log flows.

The analysis conclusion is clear: this is not only a QML delegate problem. The main issue is architectural: too much work can still be triggered synchronously on the GUI thread.

## Root causes

High-impact causes:

- network calls from UI-facing paths;
- SQLite reads from QML-triggered synchronous methods;
- conversion of large `QVariantList` payloads;
- pages instantiated while invisible;
- bindings and timers on hidden pages;
- logs and MQTT streams waking the UI periodically;
- N+1 cache reads for printer jobs;
- large text buffers kept in QML.

## Startup path

The session startup check must be asynchronous. It should emit a result signal instead of returning a blocking value to QML. Startup must render a usable shell first, then fill session/cloud state progressively.

## Tab switching

Tab switching must be cheap. Expensive content should load on demand and remain bounded by visibility. Hidden tabs should not run heavy refresh or log tail logic.

## Files workflow

The files page should display cached data quickly and refresh cloud state asynchronously. Blocking actions such as download URL resolution, delete, fallback upload or fetch must have async variants before being used from production UI.

## Printers workflow

Printer page loading is heavier because it can include printers, recent jobs and per-printer details. Cache loading must avoid N+1 job queries and must not block the GUI thread. The UI should receive incremental or already-grouped model data.

## MQTT and logs

MQTT and logs are streams. They must be throttled, bounded and visibility-aware:

- do not repaint large text buffers on every event;
- do not process debug-only streams in production UI;
- collapse frequent updates when the user cannot see them;
- keep raw detail in diagnostics and expose normalized state in the main UI.

## Correction phases

1. Establish a baseline and regression guard.
2. Move startup session checks to async.
3. Make tabs lazy and visibility-aware.
4. Move cache reads to async and group printer job queries.
5. Convert remaining cloud actions to async.
6. Bound MQTT/log streams by visibility.
7. Replace large QML `ListModel` payload rebuilds with C++ models or incremental deltas.
8. Document every new async convention.

## Validation commands

```bash
ctest --preset default --output-on-failure
ctest --preset default -R 'accloud_cache|accloud_cloud|accloud_security|accloud_mqtt_flow' --output-on-failure
```

## Done definition

- Main window becomes interactive in less than one second on a normal development machine.
- Tab switch has no visible freeze.
- Files and printers show cached data quickly, then refresh asynchronously.
- Hidden tabs do not run heavy periodic work.
- No production QML path calls known blocking network, SQLite, log scan or large-buffer processing directly.

## Decision

Performance work must prioritize removing blocking UI-thread work before micro-optimizing delegates or colors. QML optimization is useful only after the data/control flow is made asynchronous.
