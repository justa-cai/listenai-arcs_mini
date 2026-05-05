#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: bench_adb_push.sh [options]

Options:
  --serial <adb-serial>      Target adb serial (optional when exactly one device is online)
  --size-mb <n>              Payload size in MiB for each round (default: 8)
  --rounds <n>               Number of rounds per target (default: 5)
  --targets <list>           Comma-separated targets: flash,sd (default: flash,sd)
  --flash-addr-hex <hex>     Raw NAND start address for flash benchmark (default: 500000)
  --out-root <path>          Artifact root (default: .cache/adb_push_bench)
  -h, --help                 Show this help text
USAGE
}

log() {
    printf '[adb-push-bench] %s\n' "$*"
}

fail() {
    printf '[adb-push-bench][error] %s\n' "$*" >&2
    exit 1
}

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ADB_BIN=${ADB_BIN:-adb}
SERIAL=${SERIAL:-}
SIZE_MB=${SIZE_MB:-8}
ROUNDS=${ROUNDS:-5}
TARGETS=${TARGETS:-flash,sd}
FLASH_ADDR_HEX=${FLASH_ADDR_HEX:-500000}
OUT_ROOT=${OUT_ROOT:-"$SCRIPT_DIR/.cache/adb_push_bench"}
STAMP=$(date +%Y%m%d-%H%M%S)
RUN_DIR=
PAYLOAD=
SIZE_BYTES=
FLASH_SIZE_HEX=
FLASH_REMOTE=

while [[ $# -gt 0 ]]; do
    case "$1" in
        --serial)
            SERIAL="$2"
            shift 2
            ;;
        --size-mb)
            SIZE_MB="$2"
            shift 2
            ;;
        --rounds)
            ROUNDS="$2"
            shift 2
            ;;
        --targets)
            TARGETS="$2"
            shift 2
            ;;
        --flash-addr-hex)
            FLASH_ADDR_HEX="$2"
            shift 2
            ;;
        --out-root)
            OUT_ROOT="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            fail "unknown option: $1"
            ;;
    esac
done

mkdir -p "$OUT_ROOT"
RUN_DIR="$OUT_ROOT/$STAMP"
mkdir -p "$RUN_DIR"
ln -sfn "$RUN_DIR" "$OUT_ROOT/latest"
PAYLOAD="$RUN_DIR/test.bin"

ADB_ARGS=()

require_tool() {
    command -v "$1" >/dev/null 2>&1 || fail "missing required host tool: $1"
}

resolve_serial() {
    local devices
    mapfile -t devices < <("$ADB_BIN" devices | awk 'NR > 1 && $2 == "device" { print $1 }')

    if [[ -n "$SERIAL" ]]; then
        return 0
    fi

    if [[ ${#devices[@]} -eq 1 ]]; then
        SERIAL=${devices[0]}
        return 0
    fi

    fail "please pass --serial when adb sees ${#devices[@]} online devices"
}

adb_cmd() {
    "$ADB_BIN" "${ADB_ARGS[@]}" "$@"
}

record_command() {
    local name="$1"
    shift
    local outfile="$RUN_DIR/${name}.txt"

    {
        printf '$'
        for arg in "$@"; do
            printf ' %q' "$arg"
        done
        printf '\n'
    } >"$outfile"

    local rc=0
    if "$@" >>"$outfile" 2>&1; then
        rc=0
    else
        rc=$?
    fi

    printf '\n[exit_code] %d\n' "$rc" >>"$outfile"
    cat "$outfile"
    return "$rc"
}

make_payload() {
    log "creating ${SIZE_MB}MiB payload at $PAYLOAD"
    dd if=/dev/zero of="$PAYLOAD" bs=1M count="$SIZE_MB" status=none
}

parse_push_stats() {
    local logfile="$1"
    python3 - "$logfile" <<'PY'
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").replace("\r", "\n")
match = None
for line in text.splitlines():
    found = re.search(r"\(([0-9]+) bytes in ([0-9.]+)s\)", line)
    if found:
        match = found
if match is None:
    sys.exit(1)
bytes_count = int(match.group(1))
seconds = float(match.group(2))
mb_per_sec = bytes_count / seconds / (1024 * 1024)
print(f"{bytes_count} {seconds:.6f} {mb_per_sec:.3f}")
PY
}

append_result() {
    local target_name="$1"
    local round="$2"
    local bytes_count="$3"
    local seconds="$4"
    local mbps="$5"

    printf '%s %s %s %s %s\n' "$target_name" "$round" "$bytes_count" "$seconds" "$mbps" >>"$RUN_DIR/results.tsv"
}

summarize_target() {
    local target_name="$1"
    awk -v target="$target_name" '
        $1 == target {
            count += 1;
            value = $5 + 0.0;
            sum += value;
            if (count == 1 || value < min) min = value;
            if (count == 1 || value > max) max = value;
        }
        END {
            if (count == 0) {
                exit 1;
            }
            printf "%s\trounds=%d\tavg=%.3f MB/s\tmin=%.3f MB/s\tmax=%.3f MB/s\n", target, count, sum / count, min, max;
        }
    ' "$RUN_DIR/results.tsv"
}

run_target() {
    local target_name="$1"
    local remote_file="$2"
    local round logfile parsed bytes_count seconds mbps

    for ((round = 1; round <= ROUNDS; round++)); do
        logfile="$RUN_DIR/push_${target_name}_round${round}.txt"
        log "round ${round}/${ROUNDS}: adb push -> ${remote_file}"
        record_command "push_${target_name}_round${round}" adb_cmd push "$PAYLOAD" "$remote_file" >/dev/null || \
            fail "adb push failed for ${target_name} round ${round}"
        parsed=$(parse_push_stats "$logfile") || fail "failed to parse adb push output for ${target_name} round ${round}"
        read -r bytes_count seconds mbps <<<"$parsed"
        append_result "$target_name" "$round" "$bytes_count" "$seconds" "$mbps"
    done
}

write_summary() {
    {
        printf 'serial=%s\n' "$SERIAL"
        printf 'size_mb=%s\n' "$SIZE_MB"
        printf 'size_bytes=%s\n' "$SIZE_BYTES"
        printf 'rounds=%s\n' "$ROUNDS"
        printf 'targets=%s\n' "$TARGETS"
        printf 'flash_addr_hex=%s\n' "$FLASH_ADDR_HEX"
        printf '\n'
        summarize_target flash 2>/dev/null || true
        summarize_target sd 2>/dev/null || true
    } | tee "$RUN_DIR/summary.txt"
}

require_tool "$ADB_BIN"
require_tool dd
resolve_serial
ADB_ARGS=(-s "$SERIAL")
SIZE_BYTES=$((SIZE_MB * 1024 * 1024))
FLASH_SIZE_HEX=$(printf '%x' "$SIZE_BYTES")
FLASH_REMOTE="/RAW/NAND/${FLASH_ADDR_HEX}/${FLASH_SIZE_HEX}"

adb_cmd wait-for-device
make_payload
: >"$RUN_DIR/results.tsv"

IFS=',' read -r -a target_list <<<"$TARGETS"
for target in "${target_list[@]}"; do
    case "$target" in
        flash)
            run_target flash "$FLASH_REMOTE"
            ;;
        sd)
            run_target sd "/SD:/adb_bench/test.bin"
            ;;
        *)
            fail "unsupported target: $target"
            ;;
    esac
done

write_summary
log "artifacts saved under $RUN_DIR"
