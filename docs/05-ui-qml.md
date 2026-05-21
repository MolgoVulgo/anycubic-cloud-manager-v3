# UI / QML

Status: `PARTIAL`. The main UI exists and is usable, while some dialogs remain draft/debug or product-incomplete.

## Product position

The UI is a cloud-first control room. Its main responsibilities are:

- show cloud files;
- show printers and printer details;
- import/update a session;
- expose remote print actions with guardrails;
- show runtime MQTT state;
- expose logs and diagnostics in debug builds;
- prepare entry points for Photon/PWMB viewer work without pretending that the viewer is complete.

## Active views

| View | Status | Role |
| --- | --- | --- |
| Main window / Control Room | Implemented | Global navigation, header, session/actions, tabs. |
| Files tab | Implemented | Cloud files, refresh, details, download/delete, entry toward print flow. |
| Printers tab | Implemented | Printer dashboard, details, recent jobs, print guardrails. |
| Logs tab | Debug implemented | Runtime JSONL tail and filters when `ACCLOUD_DEBUG=ON`. |
| Session Settings dialog | Implemented | HAR import, session target, security reminders, validation and save. |
| File Details dialog | Implemented | Human-readable cloud/slicing/file metadata. |
| Upload / print direct dialogs | Partial / draft | Visible only as non-central or debug paths until fully wired. |
| PWMB 3D Viewer dialog | Partial / draft | UI structure exists; full decode/render workflow is not product-complete. |

## Visual system

The UI uses a warm paper-like palette, rounded panels, soft borders and a teal primary accent. State colors must stay consistent:

- primary/action: teal;
- danger: red;
- warning: amber;
- ok/success: green;
- diagnostics/logs: monospace blocks.

Theme changes must not break contrast or state semantics. A color variable can change, but the role must remain stable.

## Tab decision

The tab system must avoid hidden state confusion:

- each major area has a stable tab;
- active page state must be explicit;
- hidden pages must not keep heavy refresh loops running without reason;
- expensive panes must be loaded lazily;
- log/debug panes must be gated by build/runtime debug mode.

## Files page

The files page follows a cache-first then cloud-refresh pattern:

1. show cached quota/files quickly;
2. trigger cloud refresh asynchronously;
3. update the model when fresh data arrives;
4. show deterministic inline status for cache/cloud/download/delete/sync.

The page must not perform blocking cloud calls or heavy model rebuilding directly from QML.

## Printers page

The printers page carries the remote print workflow. It must expose:

- printer identity and status;
- compatibility information;
- recent jobs;
- active task and MQTT overlay;
- reason/error messages;
- print action guardrails.

Remote print guardrails:

- identify the selected cloud file;
- check target printer compatibility;
- submit the print order;
- switch to MQTT observation;
- never open an empty viewer or start an unrelated action when a file was selected.

Printer-side resin feed controls use the same `sendOrder` endpoint with
`order_id=1224`:
- fill start: `data={"feed_type":1,"type":1}`
- drain start: `data={"feed_type":2,"type":1}`
- manual stop (same command family): `type=0` with the active `feed_type`.

## Logs page

Logs are available only in debug builds. In production builds, the UI should explain that the debug log viewer is disabled instead of exposing partial tooling.

Filters should include level, source, component, event, `op_id` and text search. Raw log volume must be bounded.

## Overflow and scrolling

Every zone with possible overflow must have functional scrolling. This applies to text areas, logs, file metadata, printer details, JSON dumps and long lists.

## UI writing rules

- User-facing text must go through i18n.
- Technical payloads can remain unlocalized in debug-only zones.
- QML must not contain business decisions that belong in use cases or bridges.
- Visible errors must include enough context and an operation id when available.
- Silent fallback is forbidden for user-triggered operations.

## Decision

The UI must remain a thin interactive layer over explicit C++ operations. The product path is Files -> Printers -> Print/MQTT. Draft dialogs stay non-central until their backend workflow is complete.
