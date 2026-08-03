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

## Split validation gates

The project uses two explicit validation gates so a restricted environment never claims to validate the Qt desktop stack.

Restricted/offline core gate:

```bash
# Place accloud-build-deps.zip at the repository root, or set
# ACCLOUD_DEPENDENCY_ARCHIVE to its absolute path.
cmake --preset protected-core
cmake --build --preset protected-core --clean-first
ctest --preset protected-core --output-on-failure
```

This gate disables Qt, QML and external-service tests. It validates the portable core, cloud/MQTT logic that has no Qt dependency, security regressions and static Python guards. The bundled dependency archive is extracted only below the CMake build directory and no network access is attempted.

Complete local Qt gate:

```bash
cmake --preset local-full
cmake --build --preset local-full --clean-first
ctest --preset local-full --output-on-failure
```

The `local-full` preset is the complete local Qt gate. Configuration fails if the required Qt desktop, MQTT or QuickTest components are missing, and it keeps QML, SQL, GUI and integration tests enabled. The live MQTT broker test remains opt-in through `ACCLOUD_MQTT_LIVE_TEST=1`.

Tests expose CTest labels such as `core`, `static`, `qt`, `qml`, `sql`, `integration` and `live`, allowing targeted local runs with `ctest --preset local-full -L <label>`.

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

Cloud bridge boundary guard:

```bash
python ../tools/check_cloud_bridge_architecture.py --repo-root ..
ctest --preset default -R '^accloud_cloud_bridge_architecture$' --output-on-failure
```

This guard keeps `CloudBridge` as a bounded facade and prevents thumbnail TLS/image handling, signed-download transport, or upload/PWSZ orchestration from moving back into it.

Local cache boundary guard:

```bash
python ../tools/check_local_cache_architecture.py --repo-root ..
ctest --preset default -R '^accloud_local_cache_architecture$' --output-on-failure
```

This guard keeps `LocalCacheStore` as a small compatibility facade, requires separate schema/files/printers/jobs/state implementation units, and verifies that runtime and SQL regression tests compile the same `ACCLOUD_LOCAL_CACHE_SOURCES` set.


MQTT bridge boundary guard:

```bash
python ../tools/check_mqtt_bridge_architecture.py --repo-root ..
ctest --preset default -R '^accloud_mqtt_bridge_architecture$' --output-on-failure
```

This guard keeps `MqttBridge` as a bounded Qt facade, requires the session/messages/telemetry implementation units, verifies the shared `ACCLOUD_MQTT_BRIDGE_SOURCES` set and preserves the frozen broker/SLICER ownership in the session unit.

Experimental viewer isolation:

```bash
cmake --preset experimental-viewer-core
cmake --build --preset experimental-viewer-core --clean-first
ctest --preset experimental-viewer-core \
  -R '^(accloud_experimental_viewer_architecture|accloud_experimental_viewer_scaffold|accloud_pw0_decode|accloud_pwsz_reader|accloud_layer_stack_mesher|accloud_render_pipeline|accloud_viewer_controls|accloud_render3d_worker_benchmark_selftest)$' \
  --output-on-failure
```

The `default` preset enables the viewer, and `dev-debug` plus `local-full` inherit that setting. `prod` and `protected-core` explicitly keep `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`. The `experimental-viewer-core` preset validates the PWSZ reader, decoder, mesher, bounded upload queue, range render plan and camera controls without Qt. The architecture guard verifies that Qt/OpenGL sources remain behind the build option, that the per-file PWSZ action is visible, and that production remains disabled.
Mandatory local Qt/OpenGL validation for any desktop viewer change:

```bash
cmake --preset experimental-viewer-qt
cmake --build --preset experimental-viewer-qt --clean-first
ctest --preset experimental-viewer-qt --output-on-failure
```

This preset inherits `local-full`, requires native Qt dependencies, keeps `Qt6::OpenGL`, links the viewer to `accloud_cli` and also runs QML tests. `accloud-build-deps.zip` must not be imposed on the local workstation when `nlohmann_json` is already installed.

Manual worker benchmark on one unchanged PWSZ file:

```bash
./build/experimental-viewer-core/accloud_render3d_worker_benchmark \
  --input /path/to/Beetle-2.pwsz \
  --workers 1,4,8,16 \
  --repeats 1 \
  --layer-stride 2 \
  --chunk-layers 32 \
  --output-prefix /tmp/beetle-workers
```

The benchmark opens the same file once, runs each requested configuration sequentially, and verifies that every run produces exactly the same chunk, vertex, triangle, and mesh-byte totals. It measures PWSZ decoding and CPU meshing; GPU upload and rendering are intentionally excluded. `/tmp/beetle-workers.csv` and `/tmp/beetle-workers.jsonl` report total duration, first-chunk latency, requested/effective workers, decoded layers, and geometry volume. Using `--repeats 2` or `3` improves statistical stability, alternates the forward/reverse worker order between repeats, and multiplies the runtime accordingly. The PWSZ remains outside the repository and this real benchmark is not a blocking CTest test. `accloud_render3d_worker_benchmark_selftest` only validates the tool with a short synthetic source.


Do not invent commands that CMake does not declare. Live broker tests require a controlled environment and are never implied by a local unit test. The default CTest run reports `accloud_mqtt_live_broker` as **Skipped** unless live execution is explicitly enabled.

Run the live check only with a valid local session and explicit mTLS paths:

```bash
ACCLOUD_MQTT_LIVE_TEST=1 \
ACCLOUD_MQTT_TLS_CA_PATH=/controlled/path/ca.crt \
ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH=/controlled/path/client.crt \
ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH=/controlled/path/client.key \
ctest --preset default -R '^accloud_mqtt_live_broker$' --output-on-failure
```

Once explicitly enabled, missing or invalid session/TLS material and broker failures remain hard failures; they are not converted to skips.

## Before changing code

1. Read `AGENTS.md` and the relevant main document.
2. Verify the compiled source and runtime entry point.
3. Identify the owning module.
4. Separate active, experimental, legacy, test and reference code.
5. Preserve cloud, MQTT, security and GUI-thread contracts.
6. Keep the change minimal.

## Producing a patch

The complete production and delivery rules are supplied by the GPT Web session. They are intentionally not stored in this repository. Do not create, copy or search for `regles-generales-production-correctifs.md` locally. A web-only working copy under `patch/` is ignored by local Git/archive flows and is not part of the local documentation guard.

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
