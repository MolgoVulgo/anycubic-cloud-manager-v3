# Documentation

The documentation is organised as a progressive technical guide. Each main document starts with the user-visible purpose, then explains the active runtime, constraints and diagnostic details.

## Main guide

1. [Overview and getting started](01-overview-and-start.md)
2. [Architecture and active runtime](02-architecture-runtime.md)
3. [Anycubic cloud](03-anycubic-cloud.md)
4. [MQTT and realtime state](04-mqtt-realtime.md)
5. [QML interface and internationalisation](05-qml-ui.md)
6. [Security, logs, cache and data](06-security-data.md)
7. [Development, tests and patches](07-development-tests-patches.md)

## Technical appendices

- [UI performance](appendices/ui-performance.md)
- [Photon/PWMB formats and viewer](appendices/photon-viewer-formats.md)
- [Anycubic file extensions](appendices/anycubic-file-extensions.md)
- [Cloud UI screens](appendices/ui-screens-cloud-client.md)
- [MQTT JSON structures](appendices/mqtt-json-structures.md)
- [Active cloud endpoint matrix](appendices/cloud-endpoints-runtime.md)
- [Active MQTT topics](appendices/mqtt-topics.md)
- [Runtime environment variables](appendices/environment-variables.md)
- [MQTT print capture analysis](appendices/mqtt-print-capture-analysis.md)
- [Archive policy](appendices/archive-policy.md)
- [Technical decisions](appendices/technical-decisions.md)

## Public synthetic reference data

- [Reference-data policy and fixtures](reference-data/README.md)
- [Synthetic MQTT workflow](reference-data/mqtt/mqtt_synthetic_workflow.md)

## Status vocabulary

- **ACTIVE**: used by the current runtime;
- **PARTIAL**: usable but incomplete or dependent on printer/cloud behaviour;
- **EXPERIMENTAL**: not a closed production workflow;
- **REFERENCE**: explanatory material, not an executable contract;
- **HISTORICAL**: investigation snapshot retained for traceability.

The code compiled by CMake is the runtime source of truth. Historical sources under `FR/sources-techniques/` must not override it.
