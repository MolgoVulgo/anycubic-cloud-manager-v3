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

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

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

### MQTT build flags

- `ACCLOUD_ENABLE_MQTT=OFF` (default): MQTT stack excluded from build.
- `ACCLOUD_ENABLE_MQTT=ON`: enables MQTT integration if `Qt6::Mqtt` is available.
- `ACCLOUD_MQTT_V1_ENABLED_DEFAULT=OFF` (default): runtime feature flag default remains disabled.
- `ACCLOUD_MQTT_V1_ENABLED_DEFAULT=ON`: runtime feature flag default enabled for test environments.

Example:

```bash
cmake --preset default -DACCLOUD_ENABLE_MQTT=ON -DACCLOUD_MQTT_V1_ENABLED_DEFAULT=OFF
cmake --build --preset default
```

MQTT TLS environment variables:
- `ACCLOUD_MQTT_TLS_CA_PATH`
- `ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH`
- `ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH`
- `ACCLOUD_MQTT_TLS_ALLOW_INSECURE` (`0` by default)
- `ACCLOUD_MQTT_TLS_DEV_FALLBACK` (`0` by default, set `1` to use `Docs/MQTT/resources` explicitly in dev)

MQTT auth mode:
- default mode is `slicer`
- UI setting key: `mqtt.authMode` (`slicer` or `android`)
- optional env override: `ACCLOUD_MQTT_AUTH_MODE`

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
