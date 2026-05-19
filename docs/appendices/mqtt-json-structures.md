# Appendix — MQTT JSON structures

Status: `SNAPSHOT`.

This appendix records the structures that matter for parser and UI design. It is not a complete specification of Anycubic MQTT.

## Common envelope

Typical useful fields:

```json
{
  "action": "...",
  "state": "...",
  "type": "...",
  "taskid": "...",
  "data": {},
  "code": 0,
  "msg": "..."
}
```

Rules:

- do not assume every field exists;
- preserve unknown fields in diagnostics;
- map known fields to domain state;
- keep topic context together with payload context.

## `status/report`

Role: global printer state and availability.

Useful mapping:

- free/busy/degraded;
- current task id if present;
- printer status code;
- high-level availability;
- machine-side reason messages.

`status/report` alone does not explain every print blockage. It must be correlated with `print/report` and automatic operation events.

## `print/report`

Role: print workflow transitions.

Common states:

```text
downloading
checking
preheating
printing
pausing
paused
resuming
resumed
finished
stopping
stopped
failed
blocked
```

Useful fields:

- task id;
- file name/id when present;
- progress;
- current layer;
- total layers;
- remaining time;
- reason/error code;
- check maps.

## `autoOperation/report`

Role: automatic hardware operations and checks, including resin/autoload operations. Must be interpreted by phase.

## `releaseFilm/report`

Role: film/release/lift telemetry. Thresholds can be machine/material dependent and must not be hardcoded as universal truth without evidence.

## `wifi/report`

Role: network-related state. It can explain degraded printer communication but should not be confused with print failure by itself.

## Maintenance rule

When a new observed payload is added, record:

- topic;
- redacted payload sample;
- printer model if known;
- firmware/app context if known;
- mapping decision;
- fields intentionally ignored;
- test fixture if possible.
