#!/usr/bin/env bash
set -uo pipefail

# Benchmark the two supported ACM support-analysis runtime modes:
#   cpu    : canonical CPU path (--compute cpu)
#   hybrid : CPU + opportunistic Vulkan translated-lineage acceleration
#            (--compute auto)
#
# Defaults are resolved from this script's repository checkout:
#   test directory : <repo>/pwsz
#   build directory: <repo>/accloud/build/experimental-viewer-qt
#   workers        : 16
#   repetitions    : 1
#
# Optional environment variables:
#   WORKERS=16
#   RUNS=1
#   BUILD_DIR=/path/to/accloud/build/experimental-viewer-qt
#   TEST_DIR=/path/to/pwsz
#   RESULT_ROOT=/path/to/results
#   REQUIRE_VULKAN=1     # hybrid must really activate Vulkan; set 0 to allow fallback
#   VULKAN_MIN_AREA=N    # optional auto-mode threshold override; unset = runtime default
#
# Optional arguments:
#   ./tools/benchmark_support_analysis.sh obj_1_quant_1.pwsz cube2.pwsz
# If no file is specified, every *.pwsz in TEST_DIR is benchmarked.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd -- "${SCRIPT_DIR}/.." && pwd -P)}"
ACCLOUD_DIR="${ACCLOUD_DIR:-${PROJECT_ROOT}/accloud}"
TEST_DIR="${TEST_DIR:-${PROJECT_ROOT}/pwsz}"
BUILD_DIR="${BUILD_DIR:-${ACCLOUD_DIR}/build/experimental-viewer-qt}"
PROBE="${PROBE:-${BUILD_DIR}/accloud_support_analysis_probe}"
WORKERS="${WORKERS:-16}"
RUNS="${RUNS:-1}"
REQUIRE_VULKAN="${REQUIRE_VULKAN:-1}"
VULKAN_MIN_AREA="${VULKAN_MIN_AREA:-}"
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
command -v python3 >/dev/null 2>&1 || fail "python3 is required"
command -v jq >/dev/null 2>&1 || fail "jq is required"
command -v sha256sum >/dev/null 2>&1 || fail "sha256sum is required"
[[ -x "$PROBE" ]] || fail "probe not found or not executable: $PROBE"
[[ -d "$TEST_DIR" ]] || fail "test directory not found: $TEST_DIR"
[[ "$WORKERS" =~ ^[0-9]+$ ]] || fail "WORKERS must be an integer"
[[ "$RUNS" =~ ^[0-9]+$ ]] || fail "RUNS must be an integer"
[[ "$REQUIRE_VULKAN" =~ ^[01]$ ]] || fail "REQUIRE_VULKAN must be 0 or 1"
[[ -z "$VULKAN_MIN_AREA" || "$VULKAN_MIN_AREA" =~ ^[0-9]+$ ]] || \
    fail "VULKAN_MIN_AREA must be an integer when set"
(( WORKERS >= 1 )) || fail "WORKERS must be >= 1"
(( RUNS >= 1 )) || fail "RUNS must be >= 1"

mkdir -p "$RESULT_DIR"

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

{
    echo "date=$(date --iso-8601=seconds)"
    echo "project_root=$PROJECT_ROOT"
    echo "build_dir=$BUILD_DIR"
    echo "probe=$PROBE"
    echo "workers=$WORKERS"
    echo "runs=$RUNS"
    echo "require_vulkan=$REQUIRE_VULKAN"
    echo "vulkan_min_area=${VULKAN_MIN_AREA:-runtime-default}"
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

printf 'file\tmode\trun\texit\twall_s\tuser_s\tsys_s\tcpu_pct\tmax_rss_kb\tvulkan_compiled\tvulkan_active\tvulkan_dispatches\tvulkan_failures\tvulkan_gpu_jobs\tvulkan_workgroups\tvulkan_queue_wait_us\tvulkan_batch_execution_us\tsemantic_batch_calls\tsemantic_batch_jobs\tprep_window\tprep_max_inflight\tprepared_layers\tprepare_load_us\tprepare_describe_us\tevidence_us\tevidence_lots\tevidence_pairs\tevidence_edges\tforward_semantic_us\treverse_semantic_us\tforward_classification_us\tforward_commit_us\tforward_lineage_us\tforward_lineage_commit_us\treverse_prepare_us\treverse_commit_us\tsemantic_sha256\tjson\n' > "$SUMMARY_TSV"
printf 'file\tmode\trun\tsemantic_sha256\n' > "$SEMANTIC_TSV"

# Prints one tab-separated telemetry row. Semantic hashing is kept byte-for-byte
# compatible with the earlier standalone benchmark so existing reference hashes
# remain comparable across script integration.
json_metrics() {
    local json="$1"
    python3 - "$json" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    data = json.load(handle)
summary = data.get("summary", {})

def value(name, fallback=0):
    result = summary.get(name, data.get(name, fallback))
    if result is None:
        return fallback
    if isinstance(result, bool):
        return "true" if result else "false"
    return result

metric_names = [
    "vulkan_compute_compiled",
    "vulkan_compute_active",
    "vulkan_dispatches",
    "vulkan_dispatch_failures",
    "vulkan_gpu_jobs",
    "vulkan_submitted_workgroups",
    "vulkan_queue_wait_us",
    "vulkan_batch_execution_us",
    "vulkan_semantic_layer_batch_calls",
    "vulkan_semantic_layer_batch_jobs",
    "support_preparation_window",
    "support_max_preparation_inflight",
    "support_prepared_layers",
    "support_prepare_load_us",
    "support_prepare_describe_us",
    "support_semantic_evidence_us",
    "support_semantic_evidence_lots",
    "support_semantic_evidence_layer_pairs",
    "support_semantic_evidence_edges",
    "support_forward_semantic_us",
    "support_reverse_semantic_us",
    "support_forward_classification_us",
    "support_forward_commit_us",
    "support_forward_lineage_us",
    "support_forward_lineage_commit_us",
    "support_reverse_prepare_us",
    "support_reverse_commit_us",
]
print("\t".join(str(value(name)) for name in metric_names))
PY
}

semantic_hash() {
    local json="$1"
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
    }' "$json" | sha256sum | awk '{print $1}'
}

had_run_failure=0
had_semantic_mismatch=0
had_invalid_hybrid=0
had_runtime_batch=0

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
            if [[ -n "$VULKAN_MIN_AREA" ]]; then
                compute_args+=(--vulkan-min-area "$VULKAN_MIN_AREA")
            fi
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

    if (( rc != 0 )) || [[ ! -s "$json" ]]; then
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s' \
            "$base" "$mode" "$run" "$rc" "$wall" "$user" "$sys" "$cpu" "$rss" \
            >> "$SUMMARY_TSV"
        for _ in $(seq 1 29); do printf '\tNA' >> "$SUMMARY_TSV"; done
        printf '\n' >> "$SUMMARY_TSV"
        printf '%s\t%s\t%s\tNA\n' "$base" "$mode" "$run" >> "$SEMANTIC_TSV"
        printf '  FAIL rc=%s (see %s)\n' "$rc" "$stderr_log" >&2
        had_run_failure=1
        return
    fi

    local metrics
    metrics="$(json_metrics "$json")" || {
        printf '  FAIL: cannot parse %s\n' "$json" >&2
        had_run_failure=1
        return
    }

    local compiled active dispatches failures gpu_jobs workgroups queue_wait batch_execution semantic_batch_calls semantic_batch_jobs
    local prep_window prep_inflight prepared_layers load_us describe_us evidence_us evidence_lots evidence_pairs evidence_edges
    local forward_us reverse_us forward_classification forward_commit forward_lineage forward_lineage_commit reverse_prepare reverse_commit shash
    IFS=$'\t' read -r \
        compiled active dispatches failures gpu_jobs workgroups queue_wait batch_execution \
        semantic_batch_calls semantic_batch_jobs prep_window prep_inflight prepared_layers \
        load_us describe_us evidence_us evidence_lots evidence_pairs evidence_edges \
        forward_us reverse_us forward_classification forward_commit forward_lineage \
        forward_lineage_commit reverse_prepare reverse_commit <<< "$metrics"
    shash="$(semantic_hash "$json")"

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$base" "$mode" "$run" "$rc" "$wall" "$user" "$sys" "$cpu" "$rss" \
        "$compiled" "$active" "$dispatches" "$failures" "$gpu_jobs" "$workgroups" \
        "$queue_wait" "$batch_execution" "$semantic_batch_calls" "$semantic_batch_jobs" \
        "$prep_window" "$prep_inflight" "$prepared_layers" "$load_us" "$describe_us" \
        "$evidence_us" "$evidence_lots" "$evidence_pairs" "$evidence_edges" \
        "$forward_us" "$reverse_us" "$forward_classification" "$forward_commit" \
        "$forward_lineage" "$forward_lineage_commit" "$reverse_prepare" "$reverse_commit" \
        "$shash" "$json" >> "$SUMMARY_TSV"

    printf '%s\t%s\t%s\t%s\n' "$base" "$mode" "$run" "$shash" >> "$SEMANTIC_TSV"

    printf '  wall=%ss cpu=%s rss=%s KiB vulkan_active=%s gpu_jobs=%s workgroups=%s semantic=%s\n' \
        "$wall" "$cpu" "$rss" "$active" "$gpu_jobs" "$workgroups" "$shash"

    if [[ "$semantic_batch_calls" != "0" || "$semantic_batch_jobs" != "0" ]]; then
        printf '  ERROR: removed runtime semantic Vulkan batch became active (calls=%s jobs=%s).\n' \
            "$semantic_batch_calls" "$semantic_batch_jobs" >&2
        had_runtime_batch=1
    fi

    if [[ "$mode" == "hybrid" && "$active" != "true" && "$active" != "1" ]]; then
        if (( REQUIRE_VULKAN == 1 )); then
            printf '  ERROR: hybrid benchmark fell back to CPU; Vulkan is required for this run.\n' >&2
            had_invalid_hybrid=1
        else
            printf '  WARNING: hybrid mode fell back to CPU (REQUIRE_VULKAN=0).\n' >&2
        fi
    fi
}

printf 'Support-analysis benchmark (standard modes only)\n'
printf '  probe          : %s\n' "$PROBE"
printf '  tests          : %s\n' "$TEST_DIR"
printf '  workers        : %s\n' "$WORKERS"
printf '  runs/mode      : %s\n' "$RUNS"
printf '  require Vulkan : %s\n' "$REQUIRE_VULKAN"
printf '  results        : %s\n' "$RESULT_DIR"
printf '  files          : %s\n' "${#FILES[@]}"

for file in "${FILES[@]}"; do
    for run in $(seq 1 "$RUNS"); do
        run_one "$file" cpu "$run"
        run_one "$file" hybrid "$run"
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

# CPU and hybrid are two implementations of the same semantic contract. Any
# difference is a benchmark failure, not a warning.
while IFS=$'\t' read -r file run hashes; do
    [[ -z "$file" ]] && continue
    unique_count="$(printf '%s\n' "$hashes" | tr ',' '\n' | grep -v '^NA$' | sort -u | wc -l)"
    if (( unique_count > 1 )); then
        printf 'ERROR: CPU/hybrid semantic mismatch for %s run %s: %s\n' \
            "$file" "$run" "$hashes" >&2
        had_semantic_mismatch=1
    fi
done < <(
    awk -F '\t' 'NR>1 { key=$1 FS $3; if (h[key]=="") h[key]=$4; else h[key]=h[key]","$4 } END { for (k in h) { split(k,a,FS); print a[1] FS a[2] FS h[k] } }' \
        "$SEMANTIC_TSV" | sort
)

if (( had_run_failure == 0 && had_semantic_mismatch == 0 && had_invalid_hybrid == 0 && had_runtime_batch == 0 )); then
    printf '\nBenchmark status: PASS (CPU/hybrid semantics identical, runtime semantic batch disabled).\n'
    printf 'Results saved in: %s\n' "$RESULT_DIR"
    exit 0
fi

printf '\nBenchmark status: FAIL\n' >&2
printf '  run failure        : %s\n' "$had_run_failure" >&2
printf '  semantic mismatch  : %s\n' "$had_semantic_mismatch" >&2
printf '  invalid hybrid     : %s\n' "$had_invalid_hybrid" >&2
printf '  runtime batch used : %s\n' "$had_runtime_batch" >&2
printf 'Results saved in: %s\n' "$RESULT_DIR" >&2
exit 2
