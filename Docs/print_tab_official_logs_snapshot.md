# Print Tab Official Logs Snapshot

Status: `SNAPSHOT`

## Scope

Consolidated findings for official Anycubic app logs used to implement the `Print` tab behavior.
This page merges the previous analysis and the latest capture, without duplicate mappings.

## Reference Captures

- Previous analysis capture set (official app, PhotonWorkshop4 logs).
- Latest capture set:
  - `/run/media/kaj/Windows/Program Files/AnycubicPhotonWorkshop4/Log/application_Log.log` (updated `2026-03-13 14:05`)
  - `/run/media/kaj/Windows/Program Files/AnycubicPhotonWorkshop4/Log/cloud_Log.log` (updated `2026-03-13 14:04`)

## Print Tab Button Behavior (consolidated)

### `From cloud`

- Must list only cloud files compatible with the selected printer.
- Compatibility checks stay cloud-side (`file_id` / extension based filtering).

### `From local`

- Must list files already stored on the printer storage.
- Observed command flow:
  - `sendOrder(order_id=103, data={"path":"/"})`
  - MQTT response `type=file`, `action=listLocal`, with `records[]`.
- `order_id=101` is a different list source (`listUdisk`) and should not replace `listLocal`.

## Consolidated `order_id` / `printerOrder` Mapping

| order_id | Payload signature (observed) | Observed effect | Confidence |
|---|---|---|---|
| `1` | file payload (`filename`, `filepath`, `filetype`, `project_type`, `task_settings`, `is_delete_file`) | Print start accepted, response includes `task_id` (example `78541442`) | High |
| `101` | `{"path":"/"}` | MQTT `file/report` with `action=listUdisk` | High |
| `103` | `{"path":"/"}` | MQTT `file/report` with `action=listLocal` and file `records[]` | High |
| `201` | `{"axis":"z","distance":...,"move_type":...}` | Z axis move command | High |
| `301` | `{"exposure_type":...,"last_time":...}` | Exposure test command | High |
| `501` | `{"index":0,"time":10}` | Timed printer action | Medium |
| `1224` | `{"feed_type":<1\|2>,"type":<0\|1>}` | Repeated resin/feed control related sequence | Medium |
| `1252` | `["total_space","free_space","connect","resin_temp","layers","signal_strength","times"]` | Device metrics/status query | High |
| `802` | empty data in captures | Function not confirmed | Low |
| `1228` | empty data in captures | Function not confirmed | Low |
| `1231` | frequent in traces; payload not stable in extracted subset | Function not confirmed (likely file manager related) | Low |
| `1232` | empty data in captures | Function not confirmed | Low |

## Frequency Snapshot (latest capture)

- `cloud_Log.log` (`order_id=`): `1231(16)`, `201(8)`, `103(7)`, `1224(4)`, `802(3)`, `1252(3)`, `1232(3)`, `101(3)`, `301(2)`, `501(1)`, `1228(1)`, `1(1)`.
- `application_Log.log` (`printerOrder =`): `1231(25)`, `201(16)`, `103(10)`, `1224(8)`, `802(6)`, `1252(6)`, `1232(6)`, `301(4)`, `101(4)`, `501(2)`, `1228(2)`, `1(2)`.

## `sendOrder` Endpoint Observation

- In latest `cloud_Log.log`, most printer operations go through:
  - `POST /p/p/workbench/api/work/operation/sendOrder` (`52` calls in this capture).

## Impact on Current App Implementation

- Current app still contains `localFileStartPrintOrderId=999` as temporary placeholder.
- Official logs now show `order_id=1` as the likely real start-print command.
- Recommended integration rule:
  - keep runtime-configurable order id,
  - default target should move to `1` after live print validation,
  - keep `999` only as temporary fallback/debug toggle while validation is pending.

## Remaining Validation Items

1. Run one real local print from the app and verify the exact minimal payload accepted with `order_id=1`.
2. Confirm whether local listing always uses `path="/"` or if model-specific subpaths are required.
3. Map unknown frequent ids (`1231`, `1232`, `1228`, `802`) with a dedicated action->effect trace.
4. Keep translating Chinese status fragments through i18n mapping in `Print` UI messages.
5. Investigate recurring end-of-session QML `TypeError ... of null` lines (likely teardown, but still noisy).
