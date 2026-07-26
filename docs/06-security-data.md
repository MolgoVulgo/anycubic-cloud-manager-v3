# Security, logs, cache and data

## In brief

The application handles reusable web tokens, MQTT credentials, private key material and signed URLs. Security relies on strict data boundaries, redacted observability and local-only runtime storage.

## Sensitive material

Never commit, package or print:

- HAR captures;
- `session.json` content;
- access, refresh, ID or MQTT auth tokens;
- `Authorization` and `Cookie` headers;
- private TLS keys;
- complete signed URLs;
- raw credentials or password fields.

Tests use synthetic values only.

## Runtime paths

The default root is `~/.local/share/accloud`. Session, settings, cache, thumbnails, temporary files and logs remain runtime data and are excluded from patches and source archives.

## Logging and URL representation

Logs are structured and redacted. Useful fields include component, event, status, endpoint identifier, printer key, correlation data and bounded error messages.

A value is not safe merely because its field name is generic. Any URL that can contain temporary credentials must pass through `logging::safeUrlForLogs()` before it is emitted. The safe form keeps scheme, host and path, removes URL user information, and removes the complete query string and fragment.

The full URL is still used internally for the request and thumbnail cache hash. Only the log representation is reduced.

## Three distinct TLS cases

### Authenticated cloud HTTPS

Workbench APIs, session operations, uploads, printer commands and user-file downloads retain normal HTTPS verification. No global `ignoreSslErrors()` policy exists.

### MQTT broker compatibility

The broker uses TLS 1.2 and mTLS. `VerifyNone` and OpenSSL `SECLEVEL=0` may be required by the frozen Anycubic compatibility contract. This exception is confined to the MQTT session manager.

### Thumbnail preview exception

`ignoreSslErrors()` exists only in the synchronous thumbnail fetch used to populate the local preview cache:

```text
thumbnail URL
-> operation-local QNetworkAccessManager
-> image download
-> QImageReader format/content validation
-> atomic QSaveFile write
-> local thumbnail URL
```

It must not be moved to a global network manager or reused for authenticated APIs, HAR import, print commands, uploads, user-file downloads or MQTT. The request has a bounded timeout, validates the decoded image before atomic write, and returns no remote fallback URL to QML. Signed or credential-bearing thumbnail source URLs are never persisted in SQLite.

## Cache

The cache improves startup and supports explicit fallback. Thumbnail files are derived data and are written atomically after content validation. Cache purge must not remove the session unless explicitly requested. The typed SQLite cache schema is version 4; startup migrates version 3 by adding `cloud_files.status_code` and `cloud_files.thumbnail_source_url` in a verified transaction. Replacing the cloud-file snapshot is transactional: preparation, insertion and commit failures are logged by stage, and the previous snapshot is preserved after rollback.

## Reference data

Public documentation archives contain only synthetic, redacted or aggregated examples. Long raw captures and persistent printer/task identifiers belong outside the distributable source archive.

## Incident rule

When a security validation fails, stop the requested validation chain, report the exact redacted failure and classify whether it comes from the patch, the environment or pre-existing debt. Do not suppress the check.
