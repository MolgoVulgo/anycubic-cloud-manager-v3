# Development rules for Codex-assisted work

Status: `IMPLEMENTED` as project working rules.

## Project frame

This repository is a C++20 / Qt6 / QML application. Codex-assisted work must preserve that frame. Python can be used for scripts, analysis and tooling, but must not become the architectural center of the app.

## Non-negotiable rules

- Do not invent commands, presets, files or tests.
- Prefer reading repository files before changing behavior.
- Preserve existing CMake presets and build structure.
- Do not commit generated build outputs, caches, binaries or local runtime files.
- Do not commit HAR captures, tokens or TLS private keys.
- Keep QML thin and move workflow logic into C++ bridges/use cases.
- Keep cloud, MQTT, cache and viewer concerns separated.
- Mark partial/spec work honestly.

## Standard commands

```bash
cd accloud
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
../tools/ci/run_core_tests.sh
```

When a command cannot run because dependencies are missing, the result must be reported as an environment limitation, not as success.

## MQTT/auth/secrets rules

- Redact secrets in every diagnostic output.
- Do not print raw tokens or signed URLs.
- Do not hardcode private credentials.
- Keep public/client compatibility constants configurable.
- Treat HAR files as secret captures.

## Modification policy

A code change should include the smallest necessary documentation update when it changes:

- endpoint behavior;
- MQTT parsing or topic interpretation;
- UI state or workflow;
- cache/sync semantics;
- i18n rules;
- viewer geometry/format contract;
- security/logging behavior.

## Git policy

Work should be reviewable by logical topic. Large mixed changes hide protocol and UI regressions. Generated files and build directories must remain out of commits unless explicitly intended packaging artifacts.

## Decision

Codex is a development accelerator, not the source of truth. The repository code, tests, captures and this documentation decide the project behavior.
