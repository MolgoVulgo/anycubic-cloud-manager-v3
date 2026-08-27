# Technical decisions

> Status: ACTIVE decision register. Runtime facts must remain aligned with compiled code.


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
| D-013 | The validated Anycubic MQTT broker configuration is frozen. | Generic MQTT normalization can break the observed broker compatibility. |
| D-014 | `ignoreSslErrors()` remains local to thumbnail preview downloads. | The exception preserves a non-critical image cache path without weakening authenticated cloud operations. |
| D-015 | MQTT `VerifyNone` and OpenSSL `SECLEVEL=0` are dedicated compatibility controls, not a global SSL policy. | MQTT broker constraints and thumbnail TLS exceptions have different owners and risk scopes. |
| D-016 | URLs are reduced to a safe log representation before thumbnail logging. | Query strings, fragments and userinfo may expose signed access or credentials. |
| D-017 | Repository-local MQTT TLS fallback is explicit and resolves `<repo>/resources/mqtt/tls`. | Silent fallback hid configuration errors, and the previous `accloud/resources/...` path did not match the repository layout. |
| D-018 | Public reference data is synthetic-only and mechanically guarded. | Raw MQTT histories expose persistent operational identifiers and do not belong in the distributable archive. |
| D-019 | PWSZ `pw0Img` decoding uses mixed one/two-byte RLE and detects antialiasing from raster data. | Valid binary files contain only levels 0/15, while intermediate levels 1..14 are optional. |
| D-020 | Viewer mesh is generated from material/void transitions between stacked layers and split into layer chunks. | This preserves exterior/interior surfaces, supports and holes while enabling dynamic Z-range selection. |
| D-021 | The first desktop viewer backend uses `QQuickFramebufferObject` with Qt OpenGL and exact shader Z clipping. | It uses public Qt 6 APIs, matches the existing `render3d/gl` boundary, and is enabled by desktop presets including production while remaining build-gated and experimental. |

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
| Viewer | Validate the per-file Qt/OpenGL path locally, then add GPU eviction, LOD/simplification and dynamic cut caps. | High |
| Viewer | Add golden tests for real files and orientation regressions. | Medium |
| Operations | Formalize debug bundle export with redaction. | Medium |

## Rule for future decisions

Every decision should record:

- context;
- chosen behavior;
- rejected alternatives;
- source of evidence;
- impact on code/tests/docs.
