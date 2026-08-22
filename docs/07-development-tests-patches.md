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


Registered CTest inventory declared by `accloud/CMakeLists.txt`:

- `accloud_cloud_api_architecture`
- `accloud_cloud_api_support`
- `accloud_cloud_bridge_architecture`
- `accloud_cloud_core_regressions`
- `accloud_cloud_files_delete`
- `accloud_dev_raw_traffic_log`
- `accloud_direct_print_lifecycle`
- `accloud_documentation_contract`
- `accloud_embed_spirv`
- `accloud_experimental_viewer_architecture`
- `accloud_experimental_viewer_scaffold`
- `accloud_har_import`
- `accloud_jsonl_logger_timestamp`
- `accloud_layer_stack_mesher`
- `accloud_local_cache_architecture`
- `accloud_log_flow`
- `accloud_mqtt_bridge_architecture`
- `accloud_mqtt_flow`
- `accloud_mqtt_live_broker`
- `accloud_pw0_decode`
- `accloud_pwsz_cloud_preview_update_order`
- `accloud_pwsz_preview_archive`
- `accloud_pwsz_reader`
- `accloud_render3d_shader_compile`
- `accloud_render3d_worker_benchmark_selftest`
- `accloud_render_pipeline`
- `accloud_security_redaction`
- `accloud_smoke`
- `accloud_support_analysis_bridge`
- `accloud_support_analysis_diagnostics`
- `accloud_support_analyzer`
- `accloud_support_compute_vulkan`
- `accloud_thumbnail_cache_policy`
- `accloud_thumbnail_candidates`
- `accloud_thumbnail_validation`
- `accloud_ui_migration_check`
- `accloud_ui_models`
- `accloud_ui_qml`
- `accloud_ui_qml_upload`
- `accloud_viewer_controls`

The list above is the complete set of registered test names in the current source tree. Availability in a concrete preset can still depend on build options and native dependencies; the preset-specific gates remain authoritative for what is executed.

The dev-only raw traffic logger has its own regression test:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug -R '^accloud_dev_raw_traffic_log$' --output-on-failure
```

The test verifies creation of `log_brut.txt`, HTTP/MQTT capture and mandatory credential/signed-URL redaction.

The debug-only support-analysis bridge has its own regression test because `SupportAnalysisBridge` is compiled only when `ACCLOUD_DEBUG=ON` and the experimental viewer is enabled:

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug --clean-first
ctest --preset dev-debug -R '^accloud_support_analysis_bridge$' --output-on-failure
```

The test verifies that a mixed support/model diagnostic decision is exposed as `mixed`, that distinct semantic regions of the same raster component expose distinct `region_id`/`lineage_id` values in the selected JSON, that clicking a cyan support region highlights only that region rather than the whole raster component, and that clicking the model region resolves its own semantic identity.


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
  -R '^(accloud_experimental_viewer_architecture|accloud_experimental_viewer_scaffold|accloud_pw0_decode|accloud_pwsz_reader|accloud_layer_stack_mesher|accloud_support_analyzer|accloud_support_analysis_diagnostics|accloud_render_pipeline|accloud_viewer_controls|accloud_render3d_worker_benchmark_selftest)$' \
  --output-on-failure
```

The `default` preset enables the viewer, and `dev-debug` plus `local-full` inherit that setting. `prod` and `protected-core` explicitly keep `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`. The `experimental-viewer-core` preset validates the PWSZ reader, decoder, mesher, bounded upload queue, range render plan, camera controls and transactional supersession of rapid cut-surface requests without Qt. The architecture guard verifies that Qt/OpenGL sources remain behind the build option, that the per-file PWSZ action is visible, and that production remains disabled.
Mandatory local Qt/OpenGL validation for any desktop viewer change:

```bash
cmake --preset experimental-viewer-qt
cmake --build --preset experimental-viewer-qt --clean-first
ctest --preset experimental-viewer-qt --output-on-failure
```


For support-analysis phases already validated by the core gate but requiring local viewer checks, the following helper runs the mandatory OpenGL/Vulkan validations and then the complete `experimental-viewer-qt` Qt gate:

```bash
./tools/run_support_user_validation.sh
```

The script configures and clean-builds `experimental-viewer-qt`, requires both `accloud_render3d_shader_compile` and `accloud_support_compute_vulkan`, runs those two checks without accepting `Skipped`, and then runs the preset's complete CTest gate. It stops immediately on the first failure and prints the useful output from the failing command. A missing or `Skipped` graphics test is reported as incomplete validation rather than success.

This preset inherits `local-full`, requires native Qt dependencies, keeps `Qt6::OpenGL`, links the viewer to `accloud_cli` and also runs QML tests. `accloud-build-deps.zip` must not be imposed on the local workstation when `nlohmann_json` is already installed.
The viewer Qt/OpenGL inventory also includes `accloud_render3d_shader_compile`; this test requires creation of an OpenGL 3.3 Core context before shader compilation. Failure to create that context is an environment failure and does not count as a successful shader validation.

Support-analysis worker scheduling can be compared on the exact same PWSZ without changing the semantic result:

```bash
/usr/bin/time ./build/experimental-viewer-core/accloud_support_analysis_probe \
  /path/to/input.pwsz --output /tmp/support-w1.json --workers 1
/usr/bin/time ./build/experimental-viewer-core/accloud_support_analysis_probe \
  /path/to/input.pwsz --output /tmp/support-w4.json --workers 4
```

After ignoring the `analysis_workers` field, both JSON outputs must be semantically identical. The analyzer decodes/describes each native layer exactly once, retains the compact descriptions for the reverse semantic pass, reuses each component's parent-match metrics within the current layer, and uses one persistent priority worker pool for low-priority layer preparation plus foreground forward classification, model-lineage and reverse-reconciliation batches. Foreground items are claimed dynamically by the configured workers while the coordinator commits results in deterministic component order. P6.3 sizes the native-mask preparation window adaptively instead of capping it at four: capacity is bounded by `workerCount`, remaining layers and `preparationMemoryBudgetBytes` (256 MiB by default). Foreground semantic batches still preempt background preparation in the shared scheduler. Allocation-sensitive structures are also reused: connected-component extraction keeps row buffers across scanlines, maps disjoint-set roots through a dense index instead of an ordered map, reserves each component's run storage from the exact run count, and `NodeState` references the immutable retained component instead of copying its native run vector. `SparseRunMask` merges rows in place and intersects only rows touched by the sparser operand; forward/reverse preparation vectors retain their capacities across layers. The `accloud_support_analyzer` test asserts one source `loadMask()` per native layer for both concurrent and serialized sources, verifies that a 16-worker run widens preparation beyond four layers when the native-mask budget permits it, verifies that a forced small memory budget narrows the window again, compares the 1-worker, 4-worker and wide-worker semantic graph/result, verifies deterministic local component identifiers on a dense disconnected-component materialization case, and compares the P5 hybrid bitset/SIMD path against the canonical run-only path on heavily fragmented rows. The diagnostic probe accepts `--no-bitsets` to disable only the hybrid row-bitset cache for A/B measurements; AVX2 scanline dispatch still retains its mandatory scalar fallback and does not alter the semantic contract. P6 accepts `--compute auto|cpu` and `--vulkan-min-area N`. `auto` is the standard hybrid runtime contract: when Vulkan headers/library plus `glslc` or `glslangValidator` are available at build time and a usable Vulkan compute queue exists at runtime, profitable translated-overlap batches from model-lineage search are dispatched to the GPU; otherwise the canonical CPU path is used automatically. `cpu` forces the reference path for A/B comparison. The former full-GPU `vulkan` preference is removed from the runtime/probe interface. The JSON summary reports `vulkan_compute_compiled`, `vulkan_compute_active`, the selected device, eligible/submitted/GPU/fallback jobs, dispatch counts, maximum coalesced batch size, transfer bytes and host/queue/execution timings. P6.2 additionally reports `vulkan_run_source_jobs`, `vulkan_resident_reference_uploads`, `vulkan_resident_reference_reuses` and `vulkan_submitted_workgroups`. P6.3 adds `support_preparation_window`, `support_prepared_layers`, `support_max_preparation_inflight`, `support_prepare_load_us`, `support_prepare_describe_us`, `support_forward_semantic_us` and `support_reverse_semantic_us`; these counters are also emitted in live render3d progress logs before completion. P6.4 added layer-level zero-shift semantic overlap batching and the corresponding `vulkan_semantic_layer_batch_calls`, `vulkan_semantic_layer_batch_jobs`, `support_forward_classification_us`, `support_forward_commit_us`, `support_forward_lineage_us`, `support_forward_lineage_commit_us`, `support_reverse_prepare_us` and `support_reverse_commit_us` telemetry. The long-file P6.4 benchmark showed that this path regressed normal `auto`: the backend still materialised one host request per component, producing millions of synchronization objects/queue waits for too little useful GPU work. The P6.5 stabilisation removes this layer-wide zero-shift batch from the runtime analyzer in both standard modes. `auto` keeps exact/envelope zero-shift counts on sparse CPU masks and uses Vulkan only for translated-lineage work; `cpu` stays fully CPU. The Vulkan test still submits 96 zero-shift run queries in one low-level backend call to protect that primitive independently of the runtime cost model. P6.5 instead constructs a semantic-independent adjacent-layer evidence graph once: all native descriptions are prepared, adjacent layer pairs are partitioned into contiguous worker lots, and overlap/material-distance/centre-distance facts are stored as sparse CSR-style edges. Forward classification consumes the same evidence for support-parent and previous-model decisions instead of recomputing pair geometry. Final forward/reverse state mutation remains ordered and deterministic, so this is the first graph/lot reconciliation stage rather than speculative independent lot classification. P6.5 telemetry adds `support_semantic_evidence_us`, `support_semantic_evidence_lots`, `support_semantic_evidence_layer_pairs` and `support_semantic_evidence_edges`. The analyzer test compares one-worker and multi-worker graph results, requires more than one evidence lot when workers permit it, and verifies identical semantic output. P6.5 also sizes child-count, branch-drift and model-contact scratch arrays to the immediately previous candidate layer instead of cumulative `states.size()`. The static `accloud_embed_spirv` test executes `EmbedSpirv.cmake` with paths containing spaces and rejects `-DINPUT="..."` / `-DOUTPUT="..."` forms that embed quote characters in the CMake value and prevent SPIR-V header generation. When the Vulkan backend is compiled, the conditional `accloud_support_compute_vulkan` CTest compares both the dense tiled shader path and the compact-run/resident-reference path with an independent CPU reference over all translations. It also launches concurrent jobs through the shared backend to verify identical results plus actual multi-job coalescing, and verifies that a repeated compatible run job reuses the device-local reference instead of uploading it again; it is skipped with return code 77 only when no runtime Vulkan compute device can be initialised. This measurement is separate from the mesh-worker benchmark below.

`pwsz/obj_1_quant_1.pwsz` is the current long-running support-analysis performance reference. The repository helper `tools/benchmark_support_analysis.sh` is the standard CPU-versus-hybrid benchmark and deliberately exposes only the two supported runtime modes. It runs `--compute cpu` followed by `--compute auto`, stores timing/telemetry/JSON outputs under `benchmarks/support-analysis/<timestamp>/`, computes a semantic hash that excludes worker/backend/timing fields, fails if CPU and hybrid differ, and also fails if the removed runtime semantic Vulkan layer batch becomes active. By default it requires the hybrid run to report an active Vulkan backend; set `REQUIRE_VULKAN=0` only when deliberately measuring fallback behavior. From the repository root:

```bash
./tools/benchmark_support_analysis.sh obj_1_quant_1.pwsz
```

`WORKERS=16` and `RUNS=1` are the defaults. Multiple files can be named explicitly, or omitting arguments benchmarks every `*.pwsz` directly under `pwsz/`. `BUILD_DIR=...` selects another probe build, and `VULKAN_MIN_AREA=N` is an optional hybrid threshold override; leaving it unset measures the normal runtime policy. The hybrid run must report `vulkan_compute_active=true` before its wall-clock result is interpreted as CPU/GPU performance. `vulkan_semantic_layer_batch_calls` and `vulkan_semantic_layer_batch_jobs` must remain zero in both standard modes. A fallback to CPU is functionally valid for `auto`, but with the benchmark's default `REQUIRE_VULKAN=1` it is intentionally reported as an invalid hybrid measurement. Compare a Release build as well because `experimental-viewer-qt` inherits the Debug build type and CPU-side PW0 decode, component extraction and graph orchestration can otherwise starve the GPU:

```bash
cd accloud
cmake --preset experimental-viewer-qt \
  -B build/experimental-viewer-release \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/experimental-viewer-release --clean-first
cd ..
BUILD_DIR="$PWD/accloud/build/experimental-viewer-release" \
  ./tools/benchmark_support_analysis.sh obj_1_quant_1.pwsz
```

Manual worker benchmark on one unchanged PWSZ file:

```bash
./build/experimental-viewer-core/accloud_render3d_worker_benchmark \
  --input /path/to/Beetle-2.pwsz \
  --workers 4,8,16 \
  --repeats 1 \
  --layer-stride 2 \
  --chunk-layers 8,16,32 \
  --output-prefix /tmp/beetle-workers
```

The benchmark opens the same file once and executes the full Cartesian matrix of requested chunk sizes and worker counts. For a given chunk size, all worker runs must produce the same compact signature: chunks, surface quads, triangles, compact bytes, and legacy-equivalent bytes. Chunk counts are not compared across different chunk sizes. It measures PWSZ decoding and CPU meshing; GPU upload and rendering are intentionally excluded. `/tmp/beetle-workers.csv` and `/tmp/beetle-workers.jsonl` report `surface_quads`, `compact_bytes`, `legacy_equivalent_bytes`, `compression_ratio`, chunk size, total duration, and first-chunk latency. The expected main-path ratio is exactly `15.0`: eight compact bytes replace 120 historical vertex/index bytes per rectangle. Using `--repeats 2` or `3` improves statistical stability, reverses the complete matrix order on even repeats, and multiplies the runtime accordingly. The PWSZ remains outside the repository and this real benchmark is not a blocking CTest test. `accloud_render3d_worker_benchmark_selftest` validates the matrix, geometry stability, and compact ratio with a short synthetic source.

After a compact GPU-path change, the local Qt validation must include a runtime check with `Beetle-2.pwsz` using the viewer's active fixed stride (`layer_step = 2`). Generation must reach 100%, the application must remain alive and interactive, and `render3d.jsonl` must contain `gpu.compact_chunk_uploaded` events with `compression_ratio = 15`, without `gpu.budget_exceeded`, `gpu.compact_upload_failed`, or `SIGABRT`. `resident_bytes` must remain less than or equal to `budget_bytes`. When support analysis is enabled, the additional transition layers selected by the semantic analysis remain part of the normal stride-2 workflow. This real runtime check complements the synthetic tests and must not be replaced by the CPU-only benchmark.


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

The complete production and delivery rules are supplied separately by the GPT Web session in `regles-generales-production.md`. This normative file is intentionally not stored, copied or recreated in the repository and must never be included in `acm.zip` or a patch archive.

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
