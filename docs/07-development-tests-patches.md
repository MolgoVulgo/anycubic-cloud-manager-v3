# Development, tests and patches

## In brief

Correct the active owner of a behaviour, validate only the affected contract and deliver a self-contained patch. Do not repair unrelated debt or change observed Anycubic protocols by convention.

## Standard commands

From `accloud/`:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Targeted examples:

```bash
ctest --preset default -R '^accloud_har_import$' --output-on-failure
ctest --preset default -R '^accloud_security_redaction$' --output-on-failure
ctest --preset default -R '^accloud_mqtt_flow$' --output-on-failure
ctest --preset default -R '^accloud_ui_qml' --output-on-failure
```

The dev-only raw traffic logger has its own regression test:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug -R '^accloud_dev_raw_traffic_log$' --output-on-failure
```

The test verifies creation of `log_brut.txt`, HTTP/MQTT capture and mandatory credential/signed-URL redaction.

Documentation and archive guard:

```bash
python ../tools/check_documentation_contract.py --repo-root ..
ctest --preset default -R '^accloud_documentation_contract$' --output-on-failure
```

The guard validates bilingual file pairs, local links, frozen MQTT/SSL tokens, the single thumbnail `ignoreSslErrors()` scope, active QML resources, unique TS catalogs and synthetic-only public reference data.

Do not invent commands that CMake does not declare. Live broker tests require a controlled environment and are never implied by a local unit test.

## Before changing code

1. Read `AGENTS.md` and the relevant main document.
2. Verify the compiled source and runtime entry point.
3. Identify the owning module.
4. Separate active, experimental, legacy, test and reference code.
5. Preserve cloud, MQTT, security and GUI-thread contracts.
6. Keep the change minimal.

## Producing a patch

The complete production rules are in `regles-generales-production-correctifs.md`.

```text
analyse
-> modify strict scope
-> targeted validation
-> build patch ZIP
-> mechanically inspect ZIP
-> report executed and missing validations
```

A patch contains `PATCH_MANIFEST.md`, `DELETE_FILES.txt`, `MOVE_FILES.txt` and only the changed project files at repository-relative paths.

## Applying a patch

`codex-patch-mode.md` applies only when a complete patch ZIP already exists.

```text
inspect manifest
-> verify paths and contents
-> apply moves/deletions/replacements exactly
-> execute requested validations
-> stop on first mandatory failure
```

The applier does not redesign the patch or modify code to make a failing test pass.

## Failure policy

A mandatory build or test failure stops delivery. Report command, useful redacted output, likely cause, qualification and the separate correction that would be required. Do not revert unrelated changes automatically.

## Git

Never commit or push without explicit instruction. Build outputs, runtime files, logs, sessions, HAR and credentials are always excluded.
