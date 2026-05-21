# MQTT runtime

Status: `IMPLEMENTED` for the current runtime path, `PARTIAL` for exhaustive printer-model coverage.

## Role

MQTT is the realtime channel of the application. HTTP/cloud commands initiate or resynchronize actions; MQTT observes printer state, job transitions, progress, checks, errors and asynchronous command results.

The model is intentionally split:

- HTTP: account/cloud state, file metadata, signed URLs, command submission, full resync.
- MQTT: live printer state, job progress, transient events, delayed command outcomes.
- Cache: offline/fallback state and UI startup acceleration.

Mixing those three sources without rules leads to inconsistent UI behavior.

## Connection and TLS

The runtime uses mTLS material for broker access:

```bash
ACCLOUD_MQTT_TLS_CA_PATH
ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH
ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH
ACCLOUD_MQTT_TLS_ALLOW_INSECURE
ACCLOUD_MQTT_TLS_DEV_FALLBACK
ACCLOUD_MQTT_OPENSSL_CONF_PATH
```

Important constraints:

- mTLS material is mandatory for normal broker access;
- runtime auth mode is fixed to `slicer`;
- compatibility mode may be required on OpenSSL 3 hosts;
- strict peer verification should be used when the broker environment supports it;
- TLS keys and certs must never be logged.

## Runtime states

The UI receives normalized MQTT connection states:

```text
Disconnected
Connecting
Connected
Subscribed
Degraded
Reconnecting
```

`Connected` alone is not sufficient. A connected broker session without required subscriptions is not operational; this is why `Subscribed` is a distinct state.

## Topics

The important observed topic families include:

| Topic family | Meaning |
| --- | --- |
| `status/report` | global machine state, availability, busy/free status. |
| `print/report` | print job transitions, progress, preparation, failure and completion. |
| `releaseFilm/report` | film/lift/release-related telemetry and thresholds. |
| `autoOperation/report` | automatic checks and hardware/autoload operations. |
| `wifi/report` | network state and Wi-Fi-related signals. |

The runtime must preserve observed topic variants. Discovery subscriptions are useful during research, but stable mode should stay minimal enough to avoid unnecessary noise.

## Parsing pipeline

The expected ingestion pipeline is:

```text
raw MQTT message
  -> topic classifier
  -> JSON envelope parser
  -> action/state/type extraction
  -> domain event mapping
  -> realtime printer store update
  -> UI overlay update
  -> unknown observation store when unsupported
```

A message that cannot be mapped must not break the realtime store. It should be captured as an observation with topic, redacted sample, signature, disposition, frequency and last-seen timestamp.

## Important payload keys

The old capture analysis correctly identifies the following fields as structurally important:

- `action`;
- `state`;
- `type`;
- `taskid`;
- progress and layer fields;
- check maps;
- printer identifiers;
- error codes and reason messages.

The same action can have different meaning depending on phase and topic. The parser must not interpret a field in isolation.

## Realtime store

The realtime store should track:

- printer connection state;
- normalized busy/free/degraded state;
- active task id;
- active file/job identity when known;
- print stage;
- progress;
- current/total layer;
- resin/autoload state;
- hardware check state;
- last significant MQTT event;
- diagnostic unknown observations.

## HTTP and MQTT arbitration

Rules:

1. HTTP/cloud is authoritative for full resynchronization.
2. MQTT is authoritative for live transitions after a command has started.
3. Cache is authoritative only for explicit fallback state and must be labelled as such.
4. A stale MQTT state must not override a newer HTTP resync.
5. A live MQTT failure must be shown immediately even if the cache still contains a previous success state.

## Decision

MQTT is not a log viewer. It is a realtime state source. The UI must expose a clean printer state and keep raw message detail in debug/diagnostic zones only.
