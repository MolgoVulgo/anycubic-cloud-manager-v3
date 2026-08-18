#!/usr/bin/env bash
set -uo pipefail

# Benchmark ACM support analysis in three modes:
#   cpu        : CPU only (--compute cpu)
#   hybrid     : CPU + Vulkan selected automatically (--compute auto)
#   gpu        : Vulkan forced and eligibility threshold removed
#                (--compute vulkan --vulkan-min-area 0)
#
# Default test directory:
#   /home/kaj/Develop/Linux/anycubic-cloud-manager-v3/pwsz/
#
# Optional environment variables:
#   WORKERS=16
#   RUNS=1
#   BUILD_DIR=/path/to/accloud/build/experimental-viewer-qt
#   TEST_DIR=/path/to/pwsz
#   RESULT_ROOT=/path/to/results
#   GPU_MIN_AREA=0
#
# Optional arguments:
#   ./benchmark_support_analysis.sh obj_1_quant_1.pwsz cube2.pwsz
# If no file is specified, every *.pwsz in TEST_DIR is benchmarked.

PROJECT_ROOT="${PROJECT_ROOT:-/home/kaj/Develop/Linux/anycubic-cloud-manager-v3}"
ACCLOUD_DIR="${ACCLOUD_DIR:-${PROJECT_ROOT}/accloud}"
TEST_DIR="${TEST_DIR:-${PROJECT_ROOT}/pwsz}"
BUILD_DIR="${BUILD_DIR:-${ACCLOUD_DIR}/build/experimental-viewer-qt}"
PROBE="${PROBE:-${BUILD_DIR}/accloud_support_analysis_probe}"
WORKERS="${WORKERS:-16}"
RUNS="${RUNS:-1}"
GPU_MIN_AREA="${GPU_MIN_AREA:-0}"
RESULT_ROOT="${RESULT_ROOT:-${PROJECT_ROOT}/benchmarks/support-analysis}"
STAMP="$(date +%Y%m%d-%H%M%S)"
RESULT_DIR="${RESULT_ROOT}/${STAMP}"
SUMMARY_TSV="${RESULT_DIR}/summary.tsv"
SEMANTIC_TSV="${RESULT_DIR}/semantic_hashes.tsv"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

command -v /usr/bin/time >/dev/null 2>&1 || fail "/usr/bin/time is required"
command -v sha256sum >/dev/null 2>&1 || fail "sha256sum is required"
[[ -x "$PROBE" ]] || fail "probe not found or not executable: $PROBE"
[[ -d "$TEST_DIR" ]] || fail "test directory not found: $TEST_DIR"
[[ "$WORKERS" =~ ^[0-9]+$ ]] || fail "WORKERS must be an integer"
[[ "$RUNS" =~ ^[0-9]+$ ]] || fail "RUNS must be an integer"
(( WORKERS >= 1 )) || fail "WORKERS must be >= 1"
(( RUNS >= 1 )) || fail "RUNS must be >= 1"

mkdir -p "$RESULT_DIR"

# Resolve test files.
declare -a FILES=()
if (( $# > 0 )); then
    for arg in "$@"; do
        if [[ "$arg" = /* ]]; then
            file="$arg"
        else
            file="${TEST_DIR}/${arg}"
        fi
        [[ -f "$file" ]] || fail "test file not found: $file"
        FILES+=("$file")
    done
else
    while IFS= read -r -d '' file; do
        FILES+=("$file")
    done < <(find "$TEST_DIR" -maxdepth 1 -type f -name '*.pwsz' -print0 | sort -z)
fi

(( ${#FILES[@]} > 0 )) || fail "no .pwsz files found in $TEST_DIR"

# Keep environment information with the benchmark.
{
    echo "date=$(date --iso-8601=seconds)"
    echo "project_root=$PROJECT_ROOT"
    echo "build_dir=$BUILD_DIR"
    echo "probe=$PROBE"
    echo "workers=$WORKERS"
    echo "runs=$RUNS"
    echo "gpu_min_area=$GPU_MIN_AREA"
    if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
        grep -E '^CMAKE_BUILD_TYPE:STRING=' "${BUILD_DIR}/CMakeCache.txt" || true
    fi
    if command -v lscpu >/dev/null 2>&1; then
        echo
        lscpu | grep -E '^(Model name|CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)):' || true
    fi
    if command -v vulkaninfo >/dev/null 2>&1; then
        echo
        echo '[vulkaninfo --summary]'
        vulkaninfo --summary 2>&1 || true
    fi
} > "${RESULT_DIR}/environment.txt"

printf 'file\tmode\trun\texit\twall_s\tuser_s\tsys_s\tcpu_pct\tmax_rss_kb\tvulkan_compiled\tvulkan_active\tvulkan_dispatches\tvulkan_failures\tvulkan_gpu_jobs\tvulkan_workgroups\tprep_window\tprep_max_inflight\tprepare_load_us\tprepare_describe_us\tforward_semantic_us\treverse_semantic_us\tjson\n' > "$SUMMARY_TSV"
printf 'file\tmode\trun\tsemantic_sha256\n' > "$SEMANTIC_TSV"

json_value() {
    local json="$1"
    local expr="$2"
    if command -v jq >/dev/null 2>&1 && [[ -s "$json" ]]; then
        jq -r "$expr // 0" "$json" 2>/dev/null || printf '0'
    else
        printf 'NA'
    fi
}

semantic_hash() {
    local json="$1"
    if ! command -v jq >/dev/null 2>&1 || [[ ! -s "$json" ]]; then
        printf 'NA'
        return
    fi

    # Only semantic fields are hashed. Backend/timing/worker telemetry is excluded.
    jq -S -c '{
        resolution,
        layer_count,
        pitch_mm,
        summary: {
            raft_last_layer: .summary.raft_last_layer,
            first_model_layer: .summary.first_model_layer,
            last_support_layer: .summary.last_support_layer,
            components: .summary.components,
            candidate_nodes: .summary.candidate_nodes,
            accepted_nodes: .summary.accepted_nodes,
            raft_runs: .summary.raft_runs,
            support_runs: .summary.support_runs,
            free_support_runs: .summary.free_support_runs,
            projected_support_runs: .summary.projected_support_runs,
            projected_contact_pixels: .summary.projected_contact_pixels,
            rejected_projection_runs: .summary.rejected_projection_runs,
            rejected_growth_pixels: .summary.rejected_growth_pixels,
            untapered_model_contacts: .summary.untapered_model_contacts,
            contacts_without_valid_projection: .summary.contacts_without_valid_projection,
            maximum_contact_growth_ratio: .summary.maximum_contact_growth_ratio,
            terminal_support_stops: .summary.terminal_support_stops,
            expanding_model_contacts: .summary.expanding_model_contacts,
            maximum_model_expansion_ratio: .summary.maximum_model_expansion_ratio,
            continuations: .summary.continuations,
            splits: .summary.splits,
            braces: .summary.braces,
            model_contacts: .summary.model_contacts,
            forced_semantic_samples: .summary.forced_semantic_samples,
            reverse_model_seeds: .summary.reverse_model_seeds,
            reverse_model_continuations: .summary.reverse_model_continuations,
            bidirectional_mixed_components: .summary.bidirectional_mixed_components
        },
        forced_sample_layers,
        node_kinds,
        layers
    }' "$json" 2>/dev/null | sha256sum | awk '{print $1}'
}

run_one() {
    local file="$1"
    local mode="$2"
    local run="$3"
    local base stem outdir json stdout_log stderr_log time_file
    local -a compute_args

    base="$(basename "$file")"
    stem="${base%.pwsz}"
    outdir="${RESULT_DIR}/${stem}/${mode}/run-${run}"
    mkdir -p "$outdir"

    json="${outdir}/result.json"
    stdout_log="${outdir}/stdout.log"
    stderr_log="${outdir}/stderr.log"
    time_file="${outdir}/time.tsv"

    case "$mode" in
        cpu)
            compute_args=(--compute cpu)
            ;;
        hybrid)
            compute_args=(--compute auto)
            ;;
        gpu)
            # This is Vulkan-forced support compute. The analyzer still has CPU-only
            # orchestration/semantic stages by design.
            compute_args=(--compute vulkan --vulkan-min-area "$GPU_MIN_AREA")
            ;;
        *)
            fail "unknown mode: $mode"
            ;;
    esac

    printf '\n[%s] %s | mode=%s | run=%s/%s | workers=%s\n' \
        "$(date +%H:%M:%S)" "$base" "$mode" "$run" "$RUNS" "$WORKERS"

    local rc=0
    /usr/bin/time \
        -f '%e\t%U\t%S\t%P\t%M' \
        -o "$time_file" \
        "$PROBE" "$file" "$json" \
        --workers "$WORKERS" \
        "${compute_args[@]}" \
        >"$stdout_log" 2>"$stderr_log" || rc=$?

    local wall='NA' user='NA' sys='NA' cpu='NA' rss='NA'
    if [[ -s "$time_file" ]]; then
        IFS=$'\t' read -r wall user sys cpu rss < "$time_file" || true
    fi

    local compiled active dispatches failures gpu_jobs workgroups prep_window prep_inflight load_us describe_us forward_us reverse_us
    compiled="$(json_value "$json" '.summary.vulkan_compute_compiled')"
    active="$(json_value "$json" '.summary.vulkan_compute_active')"
    dispatches="$(json_value "$json" '.summary.vulkan_dispatches')"
    failures="$(json_value "$json" '.summary.vulkan_dispatch_failures')"
    gpu_jobs="$(json_value "$json" '(.summary.vulkan_gpu_jobs // .vulkan_gpu_jobs)')"
    workgroups="$(json_value "$json" '(.summary.vulkan_submitted_workgroups // .vulkan_submitted_workgroups)')"
    prep_window="$(json_value "$json" '(.summary.support_preparation_window // .support_preparation_window)')"
    prep_inflight="$(json_value "$json" '(.summary.support_max_preparation_inflight // .support_max_preparation_inflight)')"
    load_us="$(json_value "$json" '(.summary.support_prepare_load_us // .support_prepare_load_us)')"
    describe_us="$(json_value "$json" '(.summary.support_prepare_describe_us // .support_prepare_describe_us)')"
    forward_us="$(json_value "$json" '(.summary.support_forward_semantic_us // .support_forward_semantic_us)')"
    reverse_us="$(json_value "$json" '(.summary.support_reverse_semantic_us // .support_reverse_semantic_us)')"

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$base" "$mode" "$run" "$rc" "$wall" "$user" "$sys" "$cpu" "$rss" \
        "$compiled" "$active" "$dispatches" "$failures" "$gpu_jobs" "$workgroups" \
        "$prep_window" "$prep_inflight" "$load_us" "$describe_us" "$forward_us" "$reverse_us" "$json" \
        >> "$SUMMARY_TSV"

    local shash
    shash="$(semantic_hash "$json")"
    printf '%s\t%s\t%s\t%s\n' "$base" "$mode" "$run" "$shash" >> "$SEMANTIC_TSV"

    if (( rc != 0 )); then
        printf '  FAIL rc=%s (see %s)\n' "$rc" "$stderr_log" >&2
        return 0
    fi

    printf '  wall=%ss cpu=%s rss=%s KiB vulkan_active=%s gpu_jobs=%s workgroups=%s semantic=%s\n' \
        "$wall" "$cpu" "$rss" "$active" "$gpu_jobs" "$workgroups" "$shash"

    if [[ "$mode" == "gpu" && "$active" != "true" && "$active" != "1" ]]; then
        printf '  WARNING: GPU mode completed but Vulkan is not reported active.\n' >&2
    elif [[ "$mode" == "hybrid" && "$active" != "true" && "$active" != "1" ]]; then
        printf '  WARNING: hybrid mode fell back to CPU (Vulkan not reported active).\n' >&2
    fi
}

printf 'Support-analysis benchmark\n'
printf '  probe      : %s\n' "$PROBE"
printf '  tests      : %s\n' "$TEST_DIR"
printf '  workers    : %s\n' "$WORKERS"
printf '  runs/mode  : %s\n' "$RUNS"
printf '  results    : %s\n' "$RESULT_DIR"
printf '  files      : %s\n' "${#FILES[@]}"

for file in "${FILES[@]}"; do
    for run in $(seq 1 "$RUNS"); do
        run_one "$file" cpu "$run"
        run_one "$file" hybrid "$run"
        run_one "$file" gpu "$run"
    done
done

printf '\n=== Summary ===\n'
if command -v column >/dev/null 2>&1; then
    column -t -s $'\t' "$SUMMARY_TSV"
else
    cat "$SUMMARY_TSV"
fi

printf '\n=== Semantic hashes ===\n'
if command -v column >/dev/null 2>&1; then
    column -t -s $'\t' "$SEMANTIC_TSV"
else
    cat "$SEMANTIC_TSV"
fi

# Detect semantic mismatches between modes for the same file/run.
if command -v awk >/dev/null 2>&1; then
    mismatch=0
    while IFS=$'\t' read -r file run hashes; do
        [[ -z "$file" ]] && continue
        count="$(printf '%s\n' "$hashes" | tr ',' '\n' | grep -v '^NA$' | sort -u | wc -l)"
        if (( count > 1 )); then
            printf 'WARNING: semantic mismatch for %s run %s: %s\n' "$file" "$run" "$hashes" >&2
            mismatch=1
        fi
    done < <(
        awk -F '\t' 'NR>1 { key=$1 FS $3; if (h[key]=="") h[key]=$4; else h[key]=h[key]","$4 } END { for (k in h) { split(k,a,FS); print a[1] FS a[2] FS h[k] } }' "$SEMANTIC_TSV" | sort
    )
    if (( mismatch == 0 )); then
        printf '\nSemantic comparison: identical across available modes.\n'
    fi
fi

printf '\nResults saved in: %s\n' "$RESULT_DIR"
printf 'Main table: %s\n' "$SUMMARY_TSV"
