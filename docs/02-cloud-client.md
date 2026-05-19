# Anycubic Cloud client

Status: `PARTIAL`. The main cloud workflows are usable, but Anycubic cloud behavior is reverse-engineered and can change without notice.

## Scope

The cloud subsystem covers:

- session import and persistence;
- web/workbench request signing;
- file listing and metadata;
- quota;
- printer dashboard and details;
- signed downloads;
- upload workflow;
- print order workflow;
- local cache and fallback behavior;
- response parsing and error classification.

## Source of truth for endpoints

Endpoint documentation must distinguish three layers:

1. endpoints captured from browser or legacy experiments;
2. endpoints currently represented in the C++ runtime;
3. endpoints validated by tests or live runtime behavior.

Only the current C++ registry/request code can define runtime behavior. Historical endpoint lists are useful discovery material, not final specification.

For each documented endpoint, the documentation must preserve:

- logical name;
- HTTP method;
- path;
- required authentication and signature model;
- request payload shape;
- response envelope shape;
- runtime owner module;
- verification status.

## Session and HAR import

The application supports importing a session from a HAR file. The importer must search reusable tokens from response bodies first, then headers and query parameters as fallback. A persisted session can contain partial tokens; the runtime normalizes what can be used.

The persisted session file must be treated as secret material. Recommended permissions are strict owner-only access. No code path should print raw tokens in application logs.

## Workbench signature model

Workbench calls use `XX-*` headers similar to the observed web UI behavior. The parameters used by the signer must remain configurable:

```bash
ACCLOUD_PUBLIC_APP_ID
ACCLOUD_PUBLIC_APP_SECRET
ACCLOUD_PUBLIC_VERSION
ACCLOUD_PUBLIC_DEVICE_TYPE
ACCLOUD_PUBLIC_IS_CN
ACCLOUD_REGION
ACCLOUD_DEVICE_ID
ACCLOUD_USER_AGENT
ACCLOUD_CLIENT_VERSION
```

The app must not hide those values as unexplained magic. Defaults can exist for compatibility, but the configuration surface must remain explicit.

## Response interpretation

Cloud success is a combination of transport success and application-level success:

| Condition | Interpretation |
| --- | --- |
| HTTP `2xx` and no payload code, or payload `code == 1` | Success. |
| HTTP `2xx` with payload `code != 1` | Business/API error. |
| HTTP `401` / `403` | Session expired or unauthorized; refresh/relogin path. |
| HTTP `429` / `5xx` | Retryable only with bounded retry and backoff. |
| Invalid JSON | API error, not silent success. |

This rule prevents false positives where the HTTP layer succeeds but the cloud rejects the operation.

## Download workflow

Cloud downloads are two-step operations:

1. obtain a signed download URL from the cloud API;
2. perform a direct `GET` on that signed URL.

The direct `GET` must not include cloud auth headers such as `Authorization` or `XX-*`. The signed URL itself is sensitive and must be redacted in logs. Log only the base URL, query key names and optionally a hash of the query.

## Upload workflow

The upload path follows the observed cloud sequence:

1. `lockStorageSpace` returns a presigned upload URL and a lock id;
2. direct binary `PUT` to the presigned URL, without cloud auth headers;
3. `newUploadFile` registers the uploaded file and returns the cloud file id;
4. `unlockStorageSpace` is called even after partial failure, best effort.

The application must distinguish a failed binary upload from a failed registration. Both produce different recovery paths.

## Cache and sync model

The local cache is not only a performance optimization. It is also an operational fallback for the UI when the cloud is slow, temporarily unavailable or rate-limited.

The intended model is scope-based:

| Scope | Example content | Required metadata |
| --- | --- | --- |
| `files` | cloud files page | last success, last attempt, freshness, source, error. |
| `quota` | storage usage | last success, source, error. |
| `printers` | printer list/dashboard | last success, source, printer ids/keys. |
| `jobs` | recent printer jobs | printer id, last success, page/limit. |

The old analysis identifies the central issue correctly: sync exists and works, but the contract must stay explicit. A resync operation must not only diagnose; it should reconstruct each scope when needed and expose a deterministic result.

## UI-facing rule

Every long cloud operation must be asynchronous from the UI point of view. QML should call a bridge method, receive signals, and update a model. It must not wait synchronously for network, SQLite or heavy JSON conversion on the GUI thread.

## Decision

The cloud client is product-critical. It remains the primary axis of the repository. Endpoint references and captured traces are kept as support material, but the implemented C++ runtime path is the reference for behavior.
