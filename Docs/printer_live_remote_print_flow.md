# Printer Live Remote Print Flow

Status: `IMPLEMENTE`

## Goal

Document the implemented behavior for:
- Files tab `Print` action routing to Printers tab.
- Live printer job telemetry refresh and debug payload visibility.

## Code References

- Files print intent emit: `accloud/ui/qml/pages/CloudFilesPage.qml`
- Files row/button wiring: `accloud/ui/qml/pages/CloudFilesTableRow.qml`
- File details print wiring: `accloud/ui/qml/pages/CloudFileDetailsDialog.qml`
- Tab routing Files -> Printers: `accloud/ui/qml/MainWindow.qml`
- Printer orchestration and refresh policy: `accloud/ui/qml/pages/PrinterPage.qml`
- Printer details + debug panel rendering: `accloud/ui/qml/pages/PrinterDetailPanel.qml`
- Printer panel property forwarding: `accloud/ui/qml/pages/PrinterMainPanel.qml`
- Cloud parsing and normalization: `accloud/infra/cloud/CloudClient.cpp`, `accloud/infra/cloud/CloudClient.h`
- Bridge mapping to QML models: `accloud/app/CloudBridge.cpp`, `accloud/app/CloudBridge.h`
- QML behavior tests: `accloud/tests/ui/qml/tst_control_room.qml`

## Implemented Behavior

### 1) Files tab `Print` action

- `Print` from files list/details now emits a print intent with `fileId` and `fileName`.
- Main window routes the intent to Printers tab and calls printer page entrypoint.
- Printer page preselects file/printer (compatible printer preferred when available).

### 2) Live job data consolidation

- Printer UI merges job fields from:
  - inline printers payload,
  - printer details payload,
  - projects payload (`getProjects`),
  - cached jobs fallback.
- Exposed live fields include:
  - `currentFile`, `progress`, `elapsedSec`, `remainingSec`,
  - `currentLayer`, `totalLayers`.

### 3) Duration unit normalization

- Payload fields interpreted as minutes are converted to seconds before UI usage.
- `remain_time` / `remaining_time` and `elapsed_time` / `time_elapsed` / `print_time` are normalized.
- UI keeps rendering elapsed/remaining through existing time formatters.

### 4) Real-time refresh policy

- Fast refresh mode is enabled only while a print is effectively active.
- Active print detection is restricted to:
  - printer state `PRINTING`, or
  - `printStatus == 1` (direct or in project list).
- No fast refresh trigger from stale metadata alone.

### 5) Printer card rendering updates

- Current job line now includes layers info after progress:
  - `<printed> printed | <remaining> remaining`.
- Recent jobs card is kept as snapshot (not continuously rewritten by live polling).
- Recent jobs line no longer displays progress percentage.

### 6) Debug panel payload visibility

- Printer debug panel now aggregates and displays:
  - combined printers/projects endpoint payload,
  - printer details endpoint payload,
  - projects endpoint payload,
  - computed live job details block (status, progress, layers, elapsed/remaining, timestamps).
