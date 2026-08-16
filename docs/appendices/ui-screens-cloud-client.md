# Appendix — Cloud client screens

Status: `IMPLEMENTED` for the active cloud/printer workflows, `EXPERIMENTAL` for the PWSZ viewer and development diagnostics.

## Main window

`qrc:/qml/MainWindow.qml` is the desktop shell. Primary navigation exposes **Files**, **Printers** and **MQTT**. **Logs** is available only when the build exposes the debug log tooling; otherwise the tab is disabled. **Support analysis** is loaded only when both `ACCLOUD_DEBUG` and `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` are enabled. Settings are reached from the application menu rather than a primary tab.

## Files

The Files page is the cloud-file work surface. It provides refresh, deterministic cloud/cache state, selection, multi-file deletion, download, standard upload, direct print, normal cloud-file print and details. `CloudFilesWorkflowBridge` owns multi-delete sequencing; `PrintWorkflowBridge` owns remote/direct-print orchestration; QML only forwards user intent and renders semantic progress/results.

Compatible PWSZ rows also expose a **3D** action in development builds where the experimental viewer is enabled. The file is downloaded to a temporary local path before the modal is opened. PWSZ preview completion/replacement remains a separate guarded cloud workflow and never rewrites the local source before successful cloud replacement.

## File details

The details modal prioritizes print-relevant metadata, a locally resolved preview, format, size, upload date, machine/material/layer information, exposure, consumption and compatible printers. Optional technical details are controlled by `ui.cloudFiles.showAdvancedDetails`. Raw cloud metadata is development-only. Signed URLs and local thumbnail-cache paths are not displayed.

## Upload and direct print

**Add to cloud** registers the uploaded file in the Anycubic cloud and stops there. **Direct print** keeps the local file as operation input, checks compatibility, uploads it and then sends the printer order when the cloud file is ready. Cleanup policy belongs to the direct-print operation and is coordinated by `PrintWorkflowBridge`; QML does not implement persistence, task reconciliation or local/cloud delete ordering.

## 3D viewer

The PWSZ viewer is an experimental development workflow, not a draft dialog. The viewport overlay is ordered as machine name, file name plus layer count, then navigation help. Header actions are **Reset view** and **Full screen**; footer actions are **Print** and **Close**. Print closes the viewer and forwards the same cloud-file print intent as the Files list.

The preview uses a fixed one-layer-out-of-two mesh path; no full-detail UI mode exists. The optional **Supports** checkbox runs two semantic passes over every native layer, reconciles support/model evidence, injects validated `forcedSampleLayers` at support/model transitions and then builds the stride-two mesh. Geometry always comes from the original PWSZ exposure mask.

## Printers

The Printers page shows the fleet summary, name-only printer tabs, selected-printer status/device information, local files and recent jobs. Recent jobs are text-only. MQTT-derived live state is projected through the normalized realtime store; the page does not parse raw MQTT payloads. Local-file actions are disabled when the selected printer cannot accept them.

## MQTT

The MQTT page is diagnostic/operational UI over the C++ MQTT bridge and realtime store. Core routing, order correlation and store updates continue while the page is hidden; diagnostics-only formatting and raw-tail publication are activated when the page is visible.

## Logs

The log page is debug-only. It reads bounded structured JSONL snapshots while active and must not become a production dependency.

## Support analysis workspace

When debug tooling and the viewer are both enabled, the **Support analysis** workspace runs the external support-analysis probe through `SupportAnalysisBridge`. It combines the 3D viewer, layer diagnostics and lazy per-layer JSON without moving analysis, filesystem work or heavy parsing into QML.

## Session and settings

Session settings import a HAR into the normalized session without displaying raw token values. The Settings menu also owns theme/language, the fixed MQTT Slicer mode, PWSZ preview-completion preferences, direct-print failure cleanup, advanced cloud-file details, and viewer worker/palette settings when the viewer is available.
