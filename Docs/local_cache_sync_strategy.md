# Local Cache Sync Strategy

## Goal

Speed up startup by loading local data first, then refreshing from cloud in background.

## Scope

- Files list + quota
- Printers list
- Printer enriched payloads (`details` + `projects`)

## Storage

SQLite local database (`accloud_cache.db` by default, overridable with `ACCLOUD_DB_PATH`).

Tables:
- `files`
- `printers`
- `quota`
- `sync_state`
- `meta`

## Runtime behavior

### Files page

1. Load cached files/quota from SQLite.
2. Trigger cloud refresh asynchronously.
3. Update UI when `filesUpdatedFromCloud` / `quotaUpdatedFromCloud` signals are emitted.

### Printers page

1. Load cached printers from SQLite.
2. Trigger first cloud refresh asynchronously (forced).
3. Persist full printer payload in DB (base fields + `details` + `projects`).
4. Start auto-refresh timer every 30 seconds after the first successful cloud refresh.
5. Update UI from signal `printersUpdatedFromCloud`.

## Sync policy

- Retry with short backoff for cloud calls.
- Use `sync_state` for refresh bookkeeping.
- Keep stale data visible if refresh fails; surface warning through UI status bar.

## Validation

Covered by QML tests in `accloud/tests/ui/qml/tst_control_room.qml`:
- cache-first load + forced cloud refresh at startup
- model update on cloud signals
- printer auto-refresh trigger after first cloud refresh
- printer cached enriched data usage (`details` / `projects`)
