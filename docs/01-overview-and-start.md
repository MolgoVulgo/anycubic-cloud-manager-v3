# Overview and getting started

## In brief

Anycubic Cloud Manager V3 provides one desktop interface for Anycubic cloud files, printers and live resin print state. It is intended for technical users but the normal workflow does not require knowledge of HTTP, MQTT or QML.

## Product status

| Area | Status | Meaning |
| --- | --- | --- |
| Desktop shell | ACTIVE | Main Qt/QML application. |
| Cloud files and printers | ACTIVE / PARTIAL | Core operations work; compatibility remains cloud and model dependent. |
| MQTT realtime | ACTIVE | Live state is received through the observed Anycubic broker contract. |
| Remote print | ACTIVE / PARTIAL | HTTP starts the request; MQTT later reports the actual result. |
| Photon/PWMB viewer | EXPERIMENTAL | The PWSZ development path is functional end to end with a fixed stride-two preview; optional support colouring uses a two-pass native-layer analysis. Other formats and production readiness remain partial. |

## Normal workflow

```text
HAR capture
-> session import
-> local session.json
-> authenticated cloud calls
-> initial HTTP state
-> MQTT live updates
-> QML display
```

A HAR capture is used because the application does not implement the complete official login flow. The capture and generated session file contain reusable credentials.

## Requirements

- Linux desktop environment;
- CMake 3.24 or newer;
- C++20 compiler;
- Qt6 Quick, Quick Controls, Network and SQL;
- Qt6 MQTT for realtime support;
- OpenSSL and the mTLS material required by the Anycubic broker;
- a valid Anycubic session for live cloud operations.

Missing live credentials or broker material means **environment incomplete**, not test success.

## First build

```bash
./start.sh 1
```

Or manually:

```bash
cd accloud
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

`start.sh` can launch the interactive application and is not an automated validation command.

## Import a session

```bash
cd accloud
./build/default/accloud_cli --import-har /path/to/capture.har
```

The importer extracts and normalises the fields required by the runtime. A partial session fails explicitly; it is not silently accepted.

## Runtime data

Default root:

```text
~/.local/share/accloud
```

Typical generated data:

```text
session.json       reusable session — secret
settings.ini       UI/runtime preferences
accloud_cache.db   local cache
thumbnails/        derived image cache
logs/              structured redacted logs
tmp/               temporary data and OpenSSL compatibility profile
```

## Common limits

- Anycubic can change endpoints, signatures, payloads or topics.
- Printer models and firmware can expose different fields.
- An accepted HTTP print order is not final print confirmation.
- Live tests require controlled credentials, connectivity and mTLS material.
- The viewer remains experimental; its normal preview is intentionally fixed to one source layer out of two, with no full-detail UI mode.
- Support/model colouring is inferred from two semantic passes and never replaces the original PWSZ exposure mask as geometry truth.

## Where to continue

Read [Architecture and active runtime](02-architecture-runtime.md) before modifying code, then the cloud or MQTT document for protocol work.
