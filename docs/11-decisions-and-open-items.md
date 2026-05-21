# Decisions and open items

Status: `IMPLEMENTED` for recorded decisions, `PARTIAL` for open work.

## Recorded decisions

| ID | Decision | Reason |
| --- | --- | --- |
| D-001 | Cloud manager remains the primary product path. | It is the most implemented and useful workflow. |
| D-002 | MQTT is a realtime state source, not only a log stream. | Print workflow requires live transitions after HTTP commands. |
| D-003 | HTTP and MQTT must be arbitrated explicitly. | They represent different truth scopes. |
| D-004 | HAR import is supported but HAR files are secret material. | Captures contain reusable tokens and signed data. |
| D-005 | Signed URLs are never logged fully. | Query strings can expose temporary access. |
| D-006 | UI calls to network/cache-heavy paths must be async. | Blocking QML causes startup and tab-switch latency. |
| D-007 | Debug tooling is build-gated through `ACCLOUD_DEBUG`. | Production builds must not expose debug payloads/tools. |
| D-008 | Photon viewer geometry truth is threshold-0 non-black pixels. | Anti-aliased pixels are material and must not be lost. |
| D-009 | Viewer primary geometry must not depend on contours. | Contour vectorization is optional analysis/export, not ground truth. |
| D-010 | English is the default documentation language; French remains maintained. | GitHub default is English while project work also needs French. |
| D-011 | Resin interpretation is phase-sensitive. | Pre-print autoload and runtime refill do not mean the same thing. |
| D-012 | Endpoint documentation must point back to runtime C++ behavior. | Historical endpoint snapshots can drift. |

## Open items

| Area | Open item | Priority |
| --- | --- | --- |
| Cloud | Close the explicit sync contract per scope. | High |
| Cloud | Ensure every production UI action has an async path. | High |
| MQTT | Expand printer-model coverage with observation records. | Medium |
| MQTT | Keep topic discovery controlled and redacted. | Medium |
| UI | Remove or hide draft dialogs from production paths until wired. | High |
| UI | Finish lazy loading and stream visibility rules. | High |
| i18n | Complete classification and migration of visible strings. | Medium |
| Viewer | Close PWMB decode -> mask -> render pipeline. | Medium |
| Viewer | Add golden tests for real files and orientation regressions. | Medium |
| Operations | Formalize debug bundle export with redaction. | Medium |

## Rule for future decisions

Every decision should record:

- context;
- chosen behavior;
- rejected alternatives;
- source of evidence;
- impact on code/tests/docs.
