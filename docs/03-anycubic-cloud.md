# Anycubic cloud

## In brief

The cloud subsystem converts an imported Anycubic session into signed Workbench requests. It lists files and printers, resolves signed download URLs, uploads files and initiates printer orders.

QML never builds cloud requests. It calls a bridge, which delegates to application use cases and infrastructure APIs.

## Session and HAR import

A HAR capture is sensitive. The importer extracts reusable tokens and required identity fields, normalises them and writes the configured session file. Missing mandatory fields produce an explicit failure.

The active C++ implementation is the runtime source of truth. Captures and historical implementations are analysis material only.

## Workbench requests

Authenticated Workbench endpoints use observed `XX-*` headers, bearer authentication and request signing. These rules must not be replaced by generic REST conventions.

Response interpretation:

```text
HTTP 2xx and no blocking payload code, or code == 1  -> success
HTTP 2xx and code != 1                               -> business/API error
HTTP 401 or 403                                      -> unauthorised or expired session
HTTP 429 or 5xx                                      -> bounded retry may apply
invalid JSON                                         -> API error, never silent success
```

## Main workflows

### Initial synchronisation

```text
session context
-> files / quota / printers HTTP requests
-> normalised UI models
-> cache update
-> MQTT overlay applied afterwards
```

### Download

```text
cloud endpoint returns signed URL
-> direct GET on signed URL
-> no Workbench headers reinjected
-> local file validation
```

The complete signed URL must not be logged.


### Thumbnail previews

Thumbnail previews use a separate, non-critical cache path. A local `QNetworkAccessManager` may call `ignoreSslErrors()` only for that image fetch, then validates the image and writes it atomically. This exception is not used for authenticated cloud APIs or user-file downloads.

The file listing can expose several preview references. ACM now preserves an ordered candidate list and tries it sequentially: top-level `thumbnail`, top-level `img`/`image`, `slice_param.image_id`, `printer_image_id`, then `slice_param.image0_id`. Relative `slice_param` paths are expanded only when both bucket and region are available. Each candidate attempt, failure and selected source is logged with a redacted URL.

The bridge exposes only a validated local `file://` URL, or an empty value, to QML. A failed backend fetch never falls back to the remote HTTP URL. Files still reported as `PROCESSING` do not trigger preview downloads. Thumbnail requests have a bounded timeout and a short in-memory negative cache for repeated `403`, `404`, timeout or transient failures.

The model separates `thumbnailSourceUrl` from the local `thumbnailUrl`. Source URLs containing user information, query or fragment are kept only in memory for the current request and are not persisted. Logs use `logging::safeUrlForLogs()`, which keeps scheme, host and path while removing user information, query and fragment.

Metadata refresh and thumbnail refresh use four explicit policies. Printer-side cloud-file selection is cache-only and never starts a thumbnail GET. Printer history is stricter: project mapping preserves the raw image reference only as metadata, exposes an empty display image, and never consults either the thumbnail cache or the network. The automatic Files inventory compares stable cloud `fileId` values with the previous successful inventory and downloads previews only for newly detected files; the first inventory establishes the baseline without treating the whole account as new. A successful upload, including direct print upload, permits resolution of missing thumbnails and keeps the bounded delayed retries required while Anycubic is still processing the file. The Files **Refresh** button alone invokes `refreshFilesAndThumbnailsAsync()` and forces a complete retry, including invalidating valid local candidates and clearing negative-cache entries.

A cached local thumbnail associated with the same cloud file identifier is validated before any remote candidate unless the explicit forced refresh is used. Remote cache keys are derived from a canonical URL containing only scheme, host, port and path, so rotating signed query values reuse the same image. The complete signed URL remains memory-only and is used solely for the actual GET request. Within one application process, a successful local validation is memoized with the file path, size and modification timestamp; any file change invalidates that result.

Remote failures are keyed by the same canonical source identity, not by the rotating signed query. Transient failures retain a bounded retry delay. Content failures classified as `placeholder_too_small` or `invalid_image` are suppressed for the rest of the process and are retried only by the explicit forced Files refresh. Upload-authorized passes resolve newly available or previously untried missing previews, but do not clear permanent content failures. Repeated printer-history and printer metadata refreshes therefore remain network-passive for previews.

### Upload

```text
lockStorageSpace
-> binary PUT to preSignUrl
-> newUploadFile
-> unlockStorageSpace
-> bounded getUploadStatus polling
```

Unlock is attempted when required even after partial failure. PUT, registration and unlock errors remain distinct. Polling starts only after unlock. Status `1` means ready; status `2` means cloud processing. `gcode_id` values `null`, empty, numeric `0` or string `"0"` are sentinels and must not mark an upload ready. A processing upload remains successful as a transfer, but the UI reports it as pending and schedules bounded follow-up file refreshes.

For `.pwsz` files, optional preview completion is enabled by default. Before upload, ACM inspects the ZIP central directory. If `preview_images/preview_2.png` is absent while `preview_1.png` exists, the UI requests confirmation unless the user disabled confirmation. The infra layer creates a temporary archive in the same directory and duplicates the compressed `preview_1.png` bytes under the `preview_2.png` name; it does not decode or resize the PNG. The normal upload pipeline reads size and content from that prepared archive while retaining the original file name for the cloud request.

The local source is replaced atomically by the prepared archive only after the cloud upload has been registered successfully. Preparation, session, lock, PUT or registration failure removes the temporary archive and leaves the original unchanged. If cloud upload succeeds but local replacement fails, the prepared file is retained as a recovery copy and the UI reports a synchronization warning.

### Printer and print orders

An HTTP order starts the action. The final operational result can arrive later over MQTT. HTTP acceptance therefore means **request accepted**, not **print confirmed**.

For a cloud-backed print, `cloud_files.id` is sent as `file_id`. The resulting project exposes the same value in `project.model`, the cloud `gcode_id` in `project.gcode_id`, and the print task identifier in `project.id/taskid`. MQTT `start` telemetry and `project.device_message.filename` expose the exact file name copied to the printer. The printer-local identity is therefore `printer_id + path + filename`; local deletion uses order `104` and is complete only after a matching MQTT `deleteLocal/success` confirmation. Cloud deletion is a separate authenticated API call.

Direct print always sends `is_delete_file=0` so ACM owns the cleanup sequence. A successful operation with cleanup enabled deletes the printer-local copy before the cloud object. A stopped, cancelled or failed task retains the cloud object; the local copy is deleted only when the operation requested cleanup and the user preference captured at launch explicitly permits failure cleanup.


## Cache and fallback

The cache accelerates startup and provides a labelled fallback. It does not redefine cloud authority or MQTT arbitration.

## Diagnostics

For failures, record the endpoint identifier, HTTP status, API code, redacted message and correlation data. Never record bearer tokens, cookies, session content or signed query values.

The [Cloud endpoint runtime matrix](appendices/cloud-endpoints-runtime.md) is derived from `EndpointRegistry.cpp` and the active API owners. Any registry change requires the appendix to be updated in the same patch.

### Updating cloud PWSZ files with invalid thumbnails

A downloaded thumbnail is cacheable only when its payload is at least 100 bytes and Qt can decode it. Payloads below 100 bytes are classified as empty Anycubic placeholders, are not written to the cache, and mark ready `.pwsz` entries as update candidates. An explicit forced thumbnail refresh bypasses existing thumbnail cache files and performs the same validation again.

After a completed file refresh, QML may offer one batch modification for the affected entries. The C++ use case sorts candidates by cloud creation time ascending, then by numeric cloud file identifier ascending when timestamps are equal, and processes them sequentially from the oldest file to the newest. It then downloads the original PWSZ, duplicates `preview_images/preview_1.png` as `preview_2.png`, uploads a normal replacement using the original display name, waits for cloud processing, validates the new thumbnail, and only then deletes the old file identifier. No unobserved rename endpoint is introduced. If validation fails, the replacement is deleted when possible and the original remains authoritative. If deletion of the old identifier fails, both versions are kept and the result is reported as a partial modification.

The original PWSZ download is streamed into an atomic `QSaveFile` in 64 KiB chunks; the complete archive is never accumulated in memory. After registration, `unlockStorageSpace` is retried a bounded number of times and its result gates the destructive part of the workflow. If unlock is not confirmed, the old file is never deleted and the batch stops with a partial result. Cancellation is propagated from `CloudBridge` to the use case, aborts active PWSZ downloads and presigned PUT requests, interrupts polling delays, and stops before the next destructive phase. When cancellation happens after registration, both versions are kept rather than risking loss of the original.

Candidate discovery is based on a complete, bounded cloud inventory rather than the first display page. The listing is paged in batches of at least 100 entries, deduplicated by cloud file identity and capped at 100 pages. Only an empty page confirms completion, so a server-side page-size cap cannot truncate discovery. A failed page, a repeated full page without progress, or the page cap marks the inventory incomplete; ACM still refreshes the visible fallback page but does not offer a destructive batch update from partial evidence.

Post-upload thumbnail validation delegates to the same local thumbnail cache flow used by normal file display. This preserves the thumbnail-only `ignoreSslErrors()` compatibility exception, content validation, atomic cache write and cancellation behavior without extending SSL bypass to authenticated cloud APIs or user-file downloads.
