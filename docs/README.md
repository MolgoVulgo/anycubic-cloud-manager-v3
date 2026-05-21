# Documentation

This directory is the English entry point for the project documentation. It is written as product documentation: each page records what the subsystem does, how it is expected to behave, what decisions were taken, and what remains open.

## Reading order

1. `01-architecture.md` — project role, source-of-truth hierarchy and module boundaries.
2. `02-cloud-client.md` — Anycubic Cloud behavior, sessions, endpoints, downloads, uploads and sync.
3. `03-mqtt-runtime.md` — MQTT connection, topics, parser/routing model and realtime state.
4. `04-print-workflow-resin.md` — remote print workflow and resin/autoload interpretation.
5. `05-ui-qml.md` — UI shell, pages, dialogs, visual rules and screen decisions.
6. `06-ui-performance.md` — latency analysis, root causes and correction plan.
7. `07-photon-viewer-formats.md` — Photon/PWMB viewer target, format parsing and geometry rules.
8. `08-i18n.md` — translation architecture, rules and migration plan.
9. `09-operations-security.md` — logs, redaction, cache, runtime paths and diagnostics.
10. `10-development-codex.md` — development rules for Codex-assisted work.
11. `11-decisions-and-open-items.md` — project decisions and open points.

Appendices contain screen-level details, MQTT JSON structures, file-extension notes and capture-derived material.

## Status markers

| Marker | Meaning |
| --- | --- |
| `IMPLEMENTED` | Visible in the current code or validated by the runtime path. |
| `PARTIAL` | Started and useful, but not closed as a complete product path. |
| `SPEC` | Target contract, design rule or future behavior. |
| `ANALYSIS` | Diagnostic work used to support decisions. |
| `PLAN` | Ordered implementation plan. |
| `SNAPSHOT` | Capture-derived or historical material; useful but not normative by itself. |

## Source-of-truth hierarchy

1. Current repository code.
2. This documentation.
3. Appendices and capture-derived analyses.
4. Historical traces, HAR captures, raw MQTT logs and binary fixtures.

When documentation contradicts the code, the code wins. When a trace contradicts a high-level assumption, the assumption must be revisited and documented.

## Public archive policy

The documentation archive intentionally excludes HAR captures and binary PWMB samples. HAR files can contain tokens and signed URLs; PWMB files are fixtures rather than documentation. MQTT-derived JSON/CSV analysis can be included when it does not expose secrets.
