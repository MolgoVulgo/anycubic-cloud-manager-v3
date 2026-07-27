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

## Cache and fallback

The cache accelerates startup and provides a labelled fallback. It does not redefine cloud authority or MQTT arbitration.

## Diagnostics

For failures, record the endpoint identifier, HTTP status, API code, redacted message and correlation data. Never record bearer tokens, cookies, session content or signed query values.

The [Cloud endpoint runtime matrix](appendices/cloud-endpoints-runtime.md) is derived from `EndpointRegistry.cpp` and the active API owners. Any registry change requires the appendix to be updated in the same patch.

### Updating cloud PWSZ files with invalid thumbnails

A downloaded thumbnail is cacheable only when its payload is at least 100 bytes and Qt can decode it. Payloads below 100 bytes are classified as empty Anycubic placeholders, are not written to the cache, and mark ready `.pwsz` entries as update candidates. A forced refresh bypasses existing thumbnail cache files and performs the same validation again.

After a completed file refresh, QML may offer one batch modification for the affected entries. The C++ use case sorts candidates by cloud creation time ascending, then by numeric cloud file identifier ascending when timestamps are equal, and processes them sequentially from the oldest file to the newest. It then downloads the original PWSZ, duplicates `preview_images/preview_1.png` as `preview_2.png`, uploads a normal replacement using the original display name, waits for cloud processing, validates the new thumbnail, and only then deletes the old file identifier. No unobserved rename endpoint is introduced. If validation fails, the replacement is deleted when possible and the original remains authoritative. If deletion of the old identifier fails, both versions are kept and the result is reported as a partial modification.
