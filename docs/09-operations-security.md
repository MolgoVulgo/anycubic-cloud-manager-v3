# Operations, logging, cache and security

Status: `IMPLEMENTED` for the base mechanisms, `PARTIAL` for complete policy coverage.

## Runtime paths

Default root:

```text
~/.local/share/accloud
```

Default generated paths:

```text
accloud.ini
session.json
settings.ini
accloud_cache.db
tmp/
thumbnails/
logs/
```

Environment overrides:

```bash
ACCLOUD_PATHS_INI
ACCLOUD_SESSION_PATH
ACCLOUD_DB_PATH
ACCLOUD_LOG_DIR
ACCLOUD_THUMBNAIL_DIR
ACCLOUD_MQTT_OPENSSL_CONF_PATH
```

## Logging

Logs are JSONL-oriented: one event per line, stable fields, operation ids where applicable, and separate components/sources.

Recommended sinks:

- application log;
- HTTP transport log;
- render/viewer log;
- debug UI log tail, only when debug tooling is enabled.

Every user-triggered operation should have an `op_id`. Every HTTP request should have a request id. This makes UI errors traceable without exposing secrets.

## Redaction

Mandatory redaction targets:

- token;
- id token;
- access token;
- refresh token;
- authorization;
- cookie;
- signature;
- nonce;
- timestamp when used as signed material;
- password;
- email;
- full signed URL queries;
- MQTT private key paths when they reveal sensitive deployment structure.

## Cache policy

The project uses cache in multiple roles:

- UI startup acceleration;
- offline/fallback display;
- sync memory;
- thumbnails;
- future derived viewer data.

Recommended split:

| Family | Policy |
| --- | --- |
| Runtime RAM layers/masks | Sliding window plus small LRU. |
| Downloads | Disk LRU with byte cap. |
| Thumbnails/previews | Separate disk cap. |
| Derived viewer data | Fingerprinted by source file, algorithm version and parameter hash. |
| Cloud list cache | SQLite store with freshness and source metadata. |

## Manual purge

The UI should support explicit purge operations:

- global purge;
- purge by cloud file/project;
- purge by type: downloads, previews, derived, logs if allowed.

Cache purge must not break the app. Everything deleted from cache must be reconstructible or reloadable.

## Debug bundle

A useful debug bundle should include:

- sanitized settings summary;
- runtime paths;
- recent logs with redaction;
- endpoint/request summary without secrets;
- MQTT observation summary without credentials;
- cache stats;
- viewer diagnostics when relevant.

It must not include HAR files, session tokens, private keys or full signed URLs.

## Decision

Security is a project invariant. Diagnostics are necessary because the system is reverse-engineered, but diagnostics must be useful without leaking credentials.
