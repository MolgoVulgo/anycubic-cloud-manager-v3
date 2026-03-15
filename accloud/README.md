# accloud

C++20 Qt/Kirigami skeleton for Anycubic Cloud Manager V3.

This directory materializes the architecture from `Docs/structure_application_photons.md`:
- layered modules (`ui`, `domain`, `infra`, `render3d`, `tests`)
- Photon multi-format drivers (`PWMB`, `PWS`, `PHZ`, `PHOTONS`, `PWSZ`)
- async jobs, cache, logging, and cloud workflows

Logging reference:
- `../Docs/logging_reference.md`
- `../Docs/README.md` (documentation map)
- `../Docs/etat_reel_vs_cible.md` (implemented vs target matrix)

## Build

Prerequisites:
- CMake >= 3.24
- Qt6 (Quick, QuickControls2, Network, Sql)
- Qt6 MQTT module (`Qt6::Mqtt`, package `Qt6MqttConfig.cmake`)
- OpenGL runtime/dev libraries

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Core regression guard run (MQTT + LOG + HAR + CLOUD CORE + SECURITY):

```bash
./tools/ci/run_core_tests.sh
```

MQTT topics baseline guard:
- The fixture-based regression test currently asserts 6 topics
  (2 user topics + 2 printer topics x 2 printers).
- This is the baseline as of 2026-03-15.
- If Anycubic evolves topic contracts, update the test in the same commit as MQTT adaptation.

MQTT topic modes:
- Default (stable): user report topics + printer wildcard topics only.
- Extended (optional): set `ACCLOUD_MQTT_EXTENDED_TOPICS=1` to enable extra topic families for diagnostics/interop.

Cloud core regression guards:
- Response envelope parser contract (`code/msg/data` and invalid payload handling).
- Local cache store roundtrip and sync-state behavior (`ACCLOUD_DB_PATH` isolated temp DB).
- Order response tracker lifecycle (duplicate msgid, timeout, ambiguous fallback).

Security regression guards:
- Sensitive key/value and message token redaction checks.

When Qt6 is unavailable, the build falls back to a headless skeleton mode.

### Debug tooling split

- `ACCLOUD_DEBUG=OFF` (default): production-safe build, debug UI/data/traces are excluded.
- `ACCLOUD_DEBUG=ON`: enables debug tooling (`LogBridge`, `UiClickTracer`, `LogPage.qml`, `DebugPage.qml`, debug payloads).

Useful presets:

```bash
# Development build with debug tooling enabled
cmake --preset dev-debug
cmake --build --preset dev-debug

# Production build (Release, debug tooling excluded)
cmake --preset prod
cmake --build --preset prod
```

Detailed guide:
- `../Docs/debug_build_modes.md`

### MQTT build behavior

- MQTT is enabled by default in Qt builds.
- `Qt6::Mqtt` is required when `ACCLOUD_ENABLE_QT=ON`.
- No dedicated MQTT enable/disable build flag is used.

MQTT TLS environment variables:
- `ACCLOUD_MQTT_TLS_CA_PATH`
- `ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH`
- `ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH`
- `ACCLOUD_MQTT_TLS_ALLOW_INSECURE` (default `1` for broker compatibility; set `0` to enforce peer verification)
- `ACCLOUD_MQTT_TLS_DEV_FALLBACK` (`0` by default, set `1` to force local fallback)
- `ACCLOUD_MQTT_OPENSSL_CONF_PATH` (optional explicit path for auto-generated OpenSSL compat profile when insecure TLS mode is enabled)

Notes:
- mTLS is mandatory (`CA + client cert + client key`).
- TLS compatibility mode (`allow_insecure`) follows reference behavior and can be disabled explicitly.
- On OpenSSL 3 hosts, compatibility mode now auto-sets `OPENSSL_CONF` with `SECLEVEL=0` (unless already set) to match reference client behavior.

Default local TLS resource fallback:
- preferred:
  - `accloud/resources/mqtt/tls/anycubic_mqtt_tls_ca.crt`
  - `accloud/resources/mqtt/tls/anycubic_mqtt_tls_client.crt`
  - `accloud/resources/mqtt/tls/anycubic_mqtt_tls_client.key`
- legacy compatibility (still accepted):
  - `accloud/resources/mqtt/tls/anycubic_mqqt_tls_ca.crt`
  - `accloud/resources/mqtt/tls/anycubic_mqqt_tls_client.crt`
  - `accloud/resources/mqtt/tls/anycubic_mqqt_tls_client.key`

MQTT auth mode:
- mode is fixed to `slicer` in runtime.

MQTT runtime state exposed to UI:
- `Disconnected`, `Connecting`, `Connected`, `Subscribed`, `Degraded`, `Reconnecting`

MQTT discovery behavior (M7-first):
- Unknown/invalid MQTT envelopes are captured in an internal observation store.
- Observations keep signature, topic, printer key, redacted payload sample, disposition, frequency, and last seen timestamp.
- This discovery path does not block realtime store updates for already-supported messages.

## Workbench signature config

Workbench API calls use XX-* signature headers with the same defaults as
`manager_anycubic_cloud`:

```bash
export ACCLOUD_PUBLIC_APP_ID="<app_id>"
export ACCLOUD_PUBLIC_APP_SECRET="<app_secret>"
```

If not overridden, built-in defaults are used:
- `ACCLOUD_PUBLIC_APP_ID=f9b3528877c94d5c9c5af32245db46ef`
- `ACCLOUD_PUBLIC_APP_SECRET=0cf75926606049a3937f56b0373b99fb`

Optional overrides:
- `ACCLOUD_PUBLIC_VERSION` (default: `1.0.0`)
- `ACCLOUD_PUBLIC_DEVICE_TYPE` (default: `web`)
- `ACCLOUD_PUBLIC_IS_CN` (default: `2`)
- `ACCLOUD_REGION` (default: `global`)
- `ACCLOUD_DEVICE_ID` (default: `manager-anycubic-cloud-dev`)
- `ACCLOUD_USER_AGENT` (default: `manager-anycubic-cloud/0.1.0`)
- `ACCLOUD_CLIENT_VERSION` (default: `0.1.0`)
