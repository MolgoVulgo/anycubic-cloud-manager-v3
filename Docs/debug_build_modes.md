# Build Modes: Debug vs Prod

This page centralizes how debug/prod builds are separated in `accloud`.

## Global switch

- Build flag: `ACCLOUD_DEBUG`
- `ON`: debug features enabled
- `OFF`: production-safe build

Presets are already wired in `accloud/CMakePresets.json`:

- `dev-debug` -> `ACCLOUD_DEBUG=ON`
- `prod` -> `ACCLOUD_DEBUG=OFF`
- `default` -> `ACCLOUD_DEBUG=OFF`

## What changes between Debug and Prod

### When `ACCLOUD_DEBUG=ON` (debug)

- Debug UI/runtime tools are compiled:
  - `LogBridge`
  - `UiClickTracer`
  - debug QML resources (`LogPage.qml`, `DebugPage.qml`)
- QML receives `accloudBuildDebugEnabled=true`
- Debug payload fields are exposed by cloud bridges (`endpoint`, `rawJson`, `detailsRawJson`)
- `logging::debug(...)` writes `DEBUG` lines

### When `ACCLOUD_DEBUG=OFF` (prod)

- Debug UI/runtime tools are excluded from compilation
- Debug QML resources are not embedded
- QML receives `accloudBuildDebugEnabled=false`
- Debug payload fields are not produced/exposed
- `logging::debug(...)` is a no-op

## Compilation commands

From repo root:

```bash
cd accloud
```

Debug build:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug --output-on-failure
```

Prod build:

```bash
cmake --preset prod
cmake --build --preset prod
ctest --preset prod --output-on-failure
```

## Run commands

Debug UI:

```bash
./accloud/build/dev-debug/accloud_cli --debug-ui
```

Prod run:

```bash
./accloud/build/prod/accloud_cli
```

## `start.sh` (debug launcher)

At repo root, `start.sh` is dedicated to debug mode (`dev-debug`).

- `./start.sh 1`: clean + configure + build `dev-debug`, then run with `--debug-ui`
- `./start.sh 2`: run existing `dev-debug` binary with `--debug-ui`
- `./start.sh --help`: usage

