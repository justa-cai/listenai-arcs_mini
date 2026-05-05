#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: verify_boot_adb_shell.sh [options]

Options:
  --board <name>            Target board name (default: arcs_evb)
  --port <path>             Serial port to use (auto-detect /dev/ttyACM* or /dev/ttyUSB*)
  --out-root <path>         Root directory for persisted bench artifacts
  --serial-baud <rate>      Serial baudrate for serial_read.py (default: 921600)
  --serial-timeout <sec>    Serial capture timeout in seconds (default: 12)
  --normal-timeout <sec>    Seconds to wait for trigger-firmware markers before recovery (default: 20)
  --recovery-timeout <sec>  Seconds to wait for ADB enumeration in recovery (default: 20)
  --preflight-only          Only run prerequisite checks and persist diagnostic outputs
  --skip-build              Skip the build step
  --skip-burn               Skip the burn step
  -h, --help                Show this help text

Environment:
  ADB_BIN                   Override adb executable (default: adb)
  SERIAL_READ_PY            Override serial_read.py path
EOF
}

log() {
    printf '[boot-adb-bench] %s\n' "$*"
}

warn() {
    printf '[boot-adb-bench][warn] %s\n' "$*" >&2
}

fail() {
    printf '[boot-adb-bench][error] %s\n' "$*" >&2
    exit 1
}

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
REPO_REALPATH=$(CDPATH= cd -- "$REPO_ROOT" && pwd -P)

BOARD=arcs_evb
SERIAL_PORT=${SERIAL_PORT:-}
OUT_ROOT=${OUT_ROOT:-"$REPO_ROOT/.cache/boot_adb_shell_bench"}
SERIAL_BAUD=${SERIAL_BAUD:-921600}
SERIAL_TIMEOUT=${SERIAL_TIMEOUT:-12}
NORMAL_TIMEOUT=${NORMAL_TIMEOUT:-20}
RECOVERY_TIMEOUT=${RECOVERY_TIMEOUT:-20}
PREFLIGHT_ONLY=0
SKIP_BUILD=0
SKIP_BURN=0
ADB_BIN=${ADB_BIN:-adb}
SERIAL_READ_PY=${SERIAL_READ_PY:-/home/openclaw/.agents/skills/arcs-dev-tools/serial_read.py}
BURN_BAUD=3000000
NORMAL_RUNNING_MARKER="APP enter boot recovery sample running"
NORMAL_REQUEST_MARKER="APP requested boot recovery mode, rebooting"
BOOT_REASON_MARKER="boot reason: soft req"
RECOVERY_MARKER="boot adb: start reason=soft req"
RESOLVED_ARCS_BASE=""
BUILD_ROOT_SOURCE=""
BUILD_SHARED_ROOT=""
PLANNED_OVERLAY_ROOT=""
BUILD_BOOT_SOURCE_DIR=""
BUILD_CACHE_FILE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --board)
            BOARD="$2"
            shift 2
            ;;
        --port)
            SERIAL_PORT="$2"
            shift 2
            ;;
        --out-root)
            OUT_ROOT="$2"
            shift 2
            ;;
        --serial-baud)
            SERIAL_BAUD="$2"
            shift 2
            ;;
        --serial-timeout)
            SERIAL_TIMEOUT="$2"
            shift 2
            ;;
        --normal-timeout)
            NORMAL_TIMEOUT="$2"
            shift 2
            ;;
        --recovery-timeout)
            RECOVERY_TIMEOUT="$2"
            shift 2
            ;;
        --preflight-only)
            PREFLIGHT_ONLY=1
            shift
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --skip-burn)
            SKIP_BURN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            fail "Unknown option: $1"
            ;;
    esac
done

STAMP=$(date +%Y%m%d-%H%M%S)
RUN_DIR="$OUT_ROOT/$STAMP"
mkdir -p "$RUN_DIR"
mkdir -p "$OUT_ROOT"
ln -sfn "$RUN_DIR" "$OUT_ROOT/latest"

write_note() {
    local path="$1"
    shift
    {
        for line in "$@"; do
            printf '%s\n' "$line"
        done
    } >"$path"
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

find_serial_port() {
    if [[ -n "$SERIAL_PORT" ]]; then
        printf '%s\n' "$SERIAL_PORT"
        return 0
    fi

    local ports=()
    shopt -s nullglob
    ports=(/dev/ttyACM* /dev/ttyUSB*)
    shopt -u nullglob

    if [[ ${#ports[@]} -eq 0 ]]; then
        return 1
    fi

    printf '%s\n' "${ports[0]}"
}

read_cache_entry() {
    local cache_path="$1"
    local cache_key="$2"

    awk -F= -v prefix="^${cache_key}:[^=]*=" '$0 ~ prefix { sub(/^[^=]*=/, ""); print; exit }' "$cache_path"
}

prepare_arcs_base_overlay() {
    local shared_root="$1"
    local overlay_root="$REPO_ROOT/.cache/arcs_base_overlay"
    local entry_name

    rm -rf "$overlay_root"
    mkdir -p "$overlay_root"

    shopt -s dotglob nullglob
    for entry_path in "$shared_root"/* "$shared_root"/.*; do
        entry_name=$(basename "$entry_path")
        case "$entry_name" in
            .|..|.git|build|.cache|.gsd|.openclaw|.bg-shell)
                continue
                ;;
        esac
        ln -sfn "$entry_path" "$overlay_root/$entry_name"
    done
    shopt -u dotglob nullglob

    for entry_name in samples system; do
        if [[ -e "$REPO_ROOT/$entry_name" ]]; then
            rm -rf "$overlay_root/$entry_name"
            ln -sfn "$REPO_ROOT/$entry_name" "$overlay_root/$entry_name"
        fi
    done

    if [[ -d "$shared_root/components" ]]; then
        rm -rf "$overlay_root/components"
        mkdir -p "$overlay_root/components"
        shopt -s dotglob nullglob
        for entry_path in "$shared_root/components"/* "$shared_root/components"/.*; do
            entry_name=$(basename "$entry_path")
            case "$entry_name" in
                .|..)
                    continue
                    ;;
            esac
            ln -sfn "$entry_path" "$overlay_root/components/$entry_name"
        done
        shopt -u dotglob nullglob
    fi

    if [[ -d "$REPO_ROOT/components/tinyusb-appclass" ]]; then
        rm -rf "$overlay_root/components/tinyusb-appclass"
        cp -a "$REPO_ROOT/components/tinyusb-appclass" "$overlay_root/components/tinyusb-appclass"
    fi

    BUILD_SHARED_ROOT="$shared_root"
    PLANNED_OVERLAY_ROOT="$overlay_root"
    BUILD_BOOT_SOURCE_DIR="$overlay_root/system/uboot"
    RESOLVED_ARCS_BASE="$overlay_root"
}

resolve_shared_root_from_git() {
    local git_meta
    local gitdir_line
    local gitdir_path
    local shared_root

    for git_meta in "$REPO_ROOT/.git" "$REPO_REALPATH/.git"; do
        if [[ ! -f "$git_meta" ]]; then
            continue
        fi

        gitdir_line=$(<"$git_meta")
        if [[ "$gitdir_line" != gitdir:\ * ]]; then
            continue
        fi

        gitdir_path=${gitdir_line#gitdir: }
        if [[ "$gitdir_path" != /* ]]; then
            gitdir_path="$(dirname "$git_meta")/$gitdir_path"
        fi
        gitdir_path=$(realpath "$gitdir_path")
        shared_root=$(dirname "$(dirname "$(dirname "$gitdir_path")")")
        if [[ -f "$shared_root/cmake/listenai-cmake-config.cmake" ]]; then
            printf '%s\n' "$shared_root"
            return 0
        fi
    done

    return 1
}

resolve_generated_boot_root() {
    local cache_path
    local cache_root
    local cache_source

    for cache_path in "$REPO_ROOT/build/boot/CMakeCache.txt"; do
        if [[ ! -f "$cache_path" ]]; then
            continue
        fi

        cache_root=$(read_cache_entry "$cache_path" "ARCS_SDK_BASE")
        cache_source=$(read_cache_entry "$cache_path" "APPLICATION_SOURCE_DIR")
        if [[ -n "$cache_root" ]]; then
            BUILD_CACHE_FILE="$cache_path"
            BUILD_BOOT_SOURCE_DIR="$cache_source"
            RESOLVED_ARCS_BASE="$cache_root"
            return 0
        fi
    done

    return 1
}

find_arcs_base() {
    local current_dir="$REPO_ROOT"
    local shared_root

    if resolve_generated_boot_root; then
        BUILD_ROOT_SOURCE="generated-boot-cache"
        if shared_root=$(resolve_shared_root_from_git); then
            BUILD_SHARED_ROOT="$shared_root"
            PLANNED_OVERLAY_ROOT="$REPO_ROOT/.cache/arcs_base_overlay"
        fi
        return 0
    fi

    if shared_root=$(resolve_shared_root_from_git); then
        BUILD_ROOT_SOURCE="worktree-overlay"
        prepare_arcs_base_overlay "$shared_root"
        return 0
    fi

    while [[ "$current_dir" != "/" ]]; do
        if [[ -f "$current_dir/cmake/listenai-cmake-config.cmake" ]]; then
            BUILD_ROOT_SOURCE="direct-root"
            BUILD_BOOT_SOURCE_DIR="$current_dir/system/uboot"
            RESOLVED_ARCS_BASE="$current_dir"
            return 0
        fi

        local subdir
        for subdir in "$current_dir"/*/; do
            [[ -d "$subdir" ]] || continue
            if [[ -f "${subdir}cmake/listenai-cmake-config.cmake" ]]; then
                BUILD_ROOT_SOURCE="parent-discovery"
                RESOLVED_ARCS_BASE="$(CDPATH= cd -- "$subdir" && pwd)"
                BUILD_BOOT_SOURCE_DIR="$RESOLVED_ARCS_BASE/system/uboot"
                return 0
            fi
        done

        current_dir=$(dirname "$current_dir")
    done

    return 1
}

adb_device_count() {
    local path="$1"
    awk 'NR > 1 && $2 == "device" { count++ } END { print count + 0 }' "$path"
}

assert_contains() {
    local path="$1"
    local needle="$2"
    if ! grep -Fq "$needle" "$path"; then
        fail "Expected '$needle' in $path"
    fi
}

assert_regex() {
    local path="$1"
    local pattern="$2"
    if ! grep -Eq "$pattern" "$path"; then
        fail "Expected /$pattern/ in $path"
    fi
}

wait_for_file_contains() {
    local path="$1"
    local needle="$2"
    local timeout="$3"
    local deadline=$((SECONDS + timeout))

    while (( SECONDS < deadline )); do
        if [[ -f "$path" ]] && grep -Fq "$needle" "$path"; then
            return 0
        fi
        sleep 1
    done

    return 1
}

first_match_line() {
    local path="$1"
    local needle="$2"
    grep -Fn "$needle" "$path" | head -n1 | cut -d: -f1
}

assert_markers_in_order() {
    local path="$1"
    shift
    local previous_line=0
    local needle
    local line

    for needle in "$@"; do
        line=$(first_match_line "$path" "$needle")
        if [[ -z "$line" ]]; then
            fail "Expected '$needle' in $path"
        fi
        if (( line <= previous_line )); then
            fail "Expected markers in order in $path: $*"
        fi
        previous_line=$line
    done
}

wait_for_file_contains_while_adb_quiet() {
    local path="$1"
    local needle="$2"
    local timeout="$3"
    local poll_name="$4"
    local deadline=$((SECONDS + timeout))
    local poll_file="$RUN_DIR/${poll_name}.txt"

    while (( SECONDS < deadline )); do
        if [[ -f "$path" ]] && grep -Fq "$needle" "$path"; then
            return 0
        fi

        "$ADB_BIN" devices >"$poll_file" 2>&1 || true
        if [[ $(adb_device_count "$poll_file") -gt 0 ]]; then
            fail "ADB enumerated before '$needle'; see $poll_file and $path"
        fi
        sleep 1
    done

    return 1
}

max_timeout() {
    local max="$1"
    shift
    local value

    for value in "$@"; do
        if (( value > max )); then
            max=$value
        fi
    done

    printf '%s\n' "$max"
}

copy_artifact_file() {
    local src="$1"
    local dest_name="$2"

    if [[ ! -f "$src" ]]; then
        return 1
    fi

    cp "$src" "$RUN_DIR/$dest_name"
    return 0
}

capture_serial() {
    local label="$1"
    local port
    port=$(find_serial_port) || {
        warn "No serial port found for $label capture"
        return 0
    }

    record_command "$label" python3 "$SERIAL_READ_PY" "$port" -b "$SERIAL_BAUD" -t "$SERIAL_TIMEOUT"
}

SERIAL_CAPTURE_PID=""

cleanup() {
    if [[ -n "$SERIAL_CAPTURE_PID" ]]; then
        kill "$SERIAL_CAPTURE_PID" >/dev/null 2>&1 || true
        wait "$SERIAL_CAPTURE_PID" >/dev/null 2>&1 || true
        SERIAL_CAPTURE_PID=""
    fi
}
trap cleanup EXIT

start_serial_capture() {
    local outfile="$1"
    local timeout="$2"
    local port

    cleanup
    port=$(find_serial_port) || {
        warn "No serial port found for $(basename "$outfile") capture"
        return 1
    }

    python3 "$SERIAL_READ_PY" "$port" -b "$SERIAL_BAUD" -t "$timeout" >"$outfile" 2>&1 &
    SERIAL_CAPTURE_PID="$!"
    return 0
}

stop_serial_capture() {
    if [[ -n "$SERIAL_CAPTURE_PID" ]]; then
        kill "$SERIAL_CAPTURE_PID" >/dev/null 2>&1 || true
        wait "$SERIAL_CAPTURE_PID" >/dev/null 2>&1 || true
        SERIAL_CAPTURE_PID=""
    fi
}

capture_adb_devices() {
    local label="$1"
    record_command "$label" "$ADB_BIN" devices
}

assert_adb_device_count() {
    local path="$1"
    local expected="$2"
    local actual

    actual=$(adb_device_count "$path")
    if [[ "$actual" -ne "$expected" ]]; then
        fail "Expected $expected adb devices in $path, got $actual"
    fi
}

wait_for_adb_device() {
    local timeout="$1"
    local deadline=$((SECONDS + timeout))
    local poll_file

    while (( SECONDS < deadline )); do
        poll_file="$RUN_DIR/adb_devices_recovery_poll.txt"
        "$ADB_BIN" devices >"$poll_file" 2>&1 || true
        cat "$poll_file"
        if [[ $(adb_device_count "$poll_file") -gt 0 ]]; then
            return 0
        fi
        sleep 1
    done

    return 1
}

extract_sha256_hex() {
    local path="$1"
    awk 'match($0, /[0-9a-fA-F]{64}/) { print substr($0, RSTART, RLENGTH); exit }' "$path"
}

record_sha256sum() {
    local name="$1"
    local path="$2"
    record_command "$name" sha256sum "$path"
}

assert_sha256_matches() {
    local left_path="$1"
    local right_path="$2"
    local left_hash
    local right_hash

    left_hash=$(extract_sha256_hex "$left_path")
    right_hash=$(extract_sha256_hex "$right_path")

    if [[ -z "$left_hash" || -z "$right_hash" ]]; then
        fail "Could not extract sha256 values from $left_path and $right_path"
    fi

    if [[ "$left_hash" != "$right_hash" ]]; then
        fail "sha256 mismatch between $left_path and $right_path"
    fi
}

capture_build_truth() {
    local build_arcs_base="$1"
    local build_truth_file="$RUN_DIR/build_truth.txt"
    local failures=0
    local cache_root
    local cache_source

    : >"$build_truth_file"
    {
        printf 'expected_build_arcs_base=%s\n' "$build_arcs_base"
        printf 'expected_boot_source_dir=%s\n' "${BUILD_BOOT_SOURCE_DIR:-}"
    } >>"$build_truth_file"

    if ! copy_artifact_file "$REPO_ROOT/build/build_root_context.txt" build_root_context.txt; then
        printf 'info=build/build_root_context.txt not generated by this build; skipping that optional truth source\n' >>"$build_truth_file"
    fi

    if ! copy_artifact_file "$REPO_ROOT/build/boot/app.config" boot_app.config.txt; then
        printf 'fail=missing build/boot/app.config\n' >>"$build_truth_file"
        failures=$((failures + 1))
    fi

    if ! copy_artifact_file "$REPO_ROOT/build/boot/.config" boot_dot_config.txt; then
        printf 'fail=missing build/boot/.config\n' >>"$build_truth_file"
        failures=$((failures + 1))
    fi

    if ! copy_artifact_file "$REPO_ROOT/build/boot/CMakeCache.txt" boot_CMakeCache.txt; then
        printf 'fail=missing build/boot/CMakeCache.txt\n' >>"$build_truth_file"
        failures=$((failures + 1))
    fi

    if [[ -f "$REPO_ROOT/build/build_root_context.txt" ]]; then
        if grep -Fq "build_arcs_base=$build_arcs_base" "$REPO_ROOT/build/build_root_context.txt"; then
            printf 'ok=build_root_context matches build_arcs_base\n' >>"$build_truth_file"
        else
            printf 'fail=build_root_context.txt does not match build_arcs_base=%s\n' "$build_arcs_base" >>"$build_truth_file"
            failures=$((failures + 1))
        fi
    fi

    if [[ -f "$REPO_ROOT/build/boot/app.config" ]]; then
        local app_line
        for app_line in \
            'CONFIG_BOOT_ADB=y' \
            'CONFIG_BOOT_ADB_SHELL=y' \
            'CONFIG_BOOT_ADB_SYNC=y'; do
            if grep -Fxq "$app_line" "$REPO_ROOT/build/boot/app.config"; then
                printf 'ok=build/boot/app.config contains %s\n' "$app_line" >>"$build_truth_file"
            else
                printf 'fail=build/boot/app.config missing %s\n' "$app_line" >>"$build_truth_file"
                failures=$((failures + 1))
            fi
        done
    fi

    if [[ -f "$REPO_ROOT/build/boot/.config" ]]; then
        local boot_line
        for boot_line in \
            'CONFIG_BOOT_ADB=y' \
            'CONFIG_BOOT_ADB_SHELL=y' \
            'CONFIG_BOOT_ADB_SYNC=y' \
            'CONFIG_ADB_SHELL=y' \
            'CONFIG_ADB_SYNC=y' \
            'CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT="/SD:/adb/"'; do
            if grep -Fxq "$boot_line" "$REPO_ROOT/build/boot/.config"; then
                printf 'ok=build/boot/.config contains %s\n' "$boot_line" >>"$build_truth_file"
            else
                printf 'fail=build/boot/.config missing %s\n' "$boot_line" >>"$build_truth_file"
                failures=$((failures + 1))
            fi
        done
    fi

    if [[ -f "$REPO_ROOT/build/boot/CMakeCache.txt" ]]; then
        cache_root=$(read_cache_entry "$REPO_ROOT/build/boot/CMakeCache.txt" "ARCS_SDK_BASE")
        cache_source=$(read_cache_entry "$REPO_ROOT/build/boot/CMakeCache.txt" "APPLICATION_SOURCE_DIR")

        if [[ "$cache_root" == "$build_arcs_base" ]]; then
            printf 'ok=boot CMakeCache ARCS_SDK_BASE matches build_arcs_base\n' >>"$build_truth_file"
        else
            printf 'fail=boot CMakeCache ARCS_SDK_BASE=%s does not match %s\n' "$cache_root" "$build_arcs_base" >>"$build_truth_file"
            failures=$((failures + 1))
        fi

        if [[ -n "$BUILD_BOOT_SOURCE_DIR" && "$cache_source" == "$BUILD_BOOT_SOURCE_DIR" ]]; then
            printf 'ok=boot CMakeCache APPLICATION_SOURCE_DIR matches boot source dir\n' >>"$build_truth_file"
        else
            printf 'fail=boot CMakeCache APPLICATION_SOURCE_DIR=%s does not match %s\n' "$cache_source" "${BUILD_BOOT_SOURCE_DIR:-}" >>"$build_truth_file"
            failures=$((failures + 1))
        fi
    fi

    printf 'failure_count=%d\n' "$failures" >>"$build_truth_file"
    cat "$build_truth_file"

    if (( failures > 0 )); then
        return 1
    fi

    return 0
}

run_file_transfer_flow() {
    local host_push="$RUN_DIR/host_push.txt"
    local pulled_push="$RUN_DIR/pulled_push.txt"
    local host_sync="$RUN_DIR/host_sync.txt"
    local pulled_sync="$RUN_DIR/pulled_sync.txt"
    local pulled_sync_stale="$RUN_DIR/pulled_sync_after_stale.txt"
    local remote_push_rel="bench/file_push_payload.txt"
    local remote_push_abs="/SD:/adb/$remote_push_rel"
    local remote_sync_rel="bench/file_sync_payload.txt"
    local remote_sync_abs="/SD:/adb/$remote_sync_rel"

    # Stale-sync truth only claims relative file-path transfers rooted under /SD:/adb/.

    write_note "$host_push" \
        "boot-adb push payload" \
        "stamp=$STAMP" \
        "path=$remote_push_rel"
    record_sha256sum host_push_sha256 "$host_push"

    record_command adb_push_file "$ADB_BIN" push "$host_push" "$remote_push_rel"
    record_command adb_pull_file "$ADB_BIN" pull "$remote_push_rel" "$pulled_push"
    record_sha256sum pulled_push_sha256 "$pulled_push"
    assert_sha256_matches "$RUN_DIR/host_push_sha256.txt" "$RUN_DIR/pulled_push_sha256.txt"

    write_note "$host_sync" \
        "boot-adb sync payload v1" \
        "stamp=$STAMP" \
        "path=$remote_sync_rel"
    record_command adb_push_sync_seed "$ADB_BIN" push "$host_sync" "$remote_sync_rel"

    sleep 1
    write_note "$host_sync" \
        "boot-adb sync payload v2" \
        "stamp=$STAMP" \
        "path=$remote_sync_rel"
    record_sha256sum host_sync_sha256 "$host_sync"

    record_command adb_push_sync "$ADB_BIN" push --sync "$host_sync" "$remote_sync_rel"
    record_command adb_pull_sync "$ADB_BIN" pull "$remote_sync_rel" "$pulled_sync"
    record_sha256sum pulled_sync_sha256 "$pulled_sync"
    assert_sha256_matches "$RUN_DIR/host_sync_sha256.txt" "$RUN_DIR/pulled_sync_sha256.txt"

    record_command adb_push_sync_stale "$ADB_BIN" push --sync "$host_sync" "$remote_sync_rel"
    assert_regex "$RUN_DIR/adb_push_sync_stale.txt" 'skipped|0 file[s]? pushed'
    record_command adb_pull_sync_stale "$ADB_BIN" pull "$remote_sync_rel" "$pulled_sync_stale"
    record_sha256sum pulled_sync_stale_sha256 "$pulled_sync_stale"
    assert_sha256_matches "$RUN_DIR/host_sync_sha256.txt" "$RUN_DIR/pulled_sync_stale_sha256.txt"
}

write_pattern_file() {
    local path="$1"
    local size="$2"

    python3 - "$path" "$size" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
size = int(sys.argv[2], 0)
pattern = bytes([i & 0xFF for i in range(256)])
payload = (pattern * ((size + len(pattern) - 1) // len(pattern)))[:size]
path.write_bytes(payload)
PY
}

run_raw_sdraw_flow() {
    local raw_path="${RAW_SDRAW_PATH:-/RAW/SDRAW/0/1000}"
    local raw_host="$RUN_DIR/raw_sdraw_host.bin"
    local raw_after="$RUN_DIR/raw_sdraw_after.bin"

    write_pattern_file "$raw_host" 0x1000
    record_sha256sum raw_sdraw_host_sha256 "$raw_host"

    record_command adb_push_raw_sdraw "$ADB_BIN" push "$raw_host" "$raw_path"
    record_command adb_pull_raw_sdraw "$ADB_BIN" pull "$raw_path" "$raw_after"
    record_sha256sum raw_sdraw_after_sha256 "$raw_after"
    assert_sha256_matches "$RUN_DIR/raw_sdraw_host_sha256.txt" "$RUN_DIR/raw_sdraw_after_sha256.txt"
}

run_raw_flash_fastpath_flow() {
    local flash_path="${RAW_FLASH_FAST_PATH:-/RAW/FLASH/40000/10000}"
    local nand_path="${RAW_NAND_FAST_PATH:-/RAW/NAND/40000/10000}"
    local raw_host="$RUN_DIR/raw_flash_fastpath_host.bin"

    write_note "$RUN_DIR/raw_target.txt" \
        "raw_sdraw_path=${RAW_SDRAW_PATH:-/RAW/SDRAW/0/1000}" \
        "raw_flash_fast_path=$flash_path" \
        "raw_nand_fast_path=$nand_path"

    write_pattern_file "$raw_host" 0x10000
    record_sha256sum raw_flash_fastpath_host_sha256 "$raw_host"

    record_command adb_unlock "$ADB_BIN" shell "root;listenai;upgrade enter"
    assert_contains "$RUN_DIR/adb_unlock.txt" "upgrade enter successfully"
    record_command adb_status_after_unlock "$ADB_BIN" shell "root;listenai;upgrade status"
    assert_contains "$RUN_DIR/adb_status_after_unlock.txt" "upgrade status: enter"

    record_command adb_push_raw_flash_fastpath "$ADB_BIN" push "$raw_host" "$flash_path"
    record_command adb_push_raw_nand_fastpath "$ADB_BIN" push "$raw_host" "$nand_path"
    capture_adb_devices adb_devices_after_raw_flash
    assert_adb_device_count "$RUN_DIR/adb_devices_after_raw_flash.txt" 1
}

run_preflight() {
    local build_arcs_base="$1"
    local preflight_file="$RUN_DIR/preflight.txt"
    local failures=0

    : >"$preflight_file"
    {
        printf 'repo_root=%s\n' "$REPO_ROOT"
        printf 'repo_realpath=%s\n' "$REPO_REALPATH"
        printf 'build_arcs_base=%s\n' "$build_arcs_base"
        printf 'build_root_source=%s\n' "${BUILD_ROOT_SOURCE:-unknown}"
        if [[ -n "$BUILD_SHARED_ROOT" ]]; then
            printf 'build_shared_root=%s\n' "$BUILD_SHARED_ROOT"
        fi
        if [[ -n "$PLANNED_OVERLAY_ROOT" ]]; then
            printf 'planned_overlay_root=%s\n' "$PLANNED_OVERLAY_ROOT"
        fi
        if [[ -n "$BUILD_BOOT_SOURCE_DIR" ]]; then
            printf 'boot_source_dir=%s\n' "$BUILD_BOOT_SOURCE_DIR"
        fi
        if [[ -n "$BUILD_CACHE_FILE" ]]; then
            printf 'boot_cache=%s\n' "$BUILD_CACHE_FILE"
        fi
        printf 'board=%s\n' "$BOARD"
        printf 'serial_read_py=%s\n' "$SERIAL_READ_PY"
    } >>"$preflight_file"

    if [[ "$BUILD_ROOT_SOURCE" == "generated-boot-cache" ]]; then
        printf 'info=using generated boot cache to confirm the effective BOOT_ADB source tree\n' >>"$preflight_file"
    elif [[ "$BUILD_ROOT_SOURCE" == "worktree-overlay" ]]; then
        printf 'info=logical worktree lacks cmake/listenai-cmake-config.cmake; preflight prepared the same overlay root that build.sh uses\n' >>"$preflight_file"
    elif [[ ! -f "$REPO_ROOT/cmake/listenai-cmake-config.cmake" && "$build_arcs_base" != "$REPO_ROOT" ]]; then
        printf 'warn=logical worktree lacks cmake/listenai-cmake-config.cmake; build.sh will resolve ARCS_BASE outside this mirrored worktree\n' >>"$preflight_file"
    fi

    if [[ -n "$BUILD_CACHE_FILE" && -n "$PLANNED_OVERLAY_ROOT" && "$build_arcs_base" != "$PLANNED_OVERLAY_ROOT" ]]; then
        printf 'warn=generated boot cache disagrees with the planned overlay root; preflight prefers the compiled boot artifact root\n' >>"$preflight_file"
    fi

    if ! command -v "$ADB_BIN" >/dev/null 2>&1; then
        printf 'fail=adb executable not found: %s\n' "$ADB_BIN" >>"$preflight_file"
        failures=$((failures + 1))
    fi

    if [[ ! -x "$REPO_ROOT/tools/burn/cskburn" ]]; then
        printf 'fail=missing executable cskburn: %s/tools/burn/cskburn\n' "$REPO_ROOT" >>"$preflight_file"
        failures=$((failures + 1))
    fi

    if [[ ! -f "$SERIAL_READ_PY" ]]; then
        printf 'fail=serial_read.py not found: %s\n' "$SERIAL_READ_PY" >>"$preflight_file"
        failures=$((failures + 1))
    fi

    if ! find_serial_port >/dev/null 2>&1; then
        printf 'fail=no serial port detected under /dev/ttyACM* or /dev/ttyUSB*\n' >>"$preflight_file"
        failures=$((failures + 1))
    fi

    local required_files=(
        "$build_arcs_base/modules/tinyusb/src/tusb.h"
        "$build_arcs_base/system/uboot/src/boot_stage_gate.c"
        "$build_arcs_base/system/uboot/src/boot_adb_runtime.c"
        "$build_arcs_base/system/uboot/src/boot_adb_runtime.h"
        "$build_arcs_base/system/uboot/src/boot_adb_storage.c"
        "$build_arcs_base/system/uboot/src/boot_adb_storage.h"
        "$build_arcs_base/system/uboot/src/boot_adb_disk_policy.c"
        "$build_arcs_base/system/uboot/src/boot_adb_disk_policy.h"
        "$build_arcs_base/system/uboot/test/boot_adb_runtime/CMakeLists.txt"
        "$build_arcs_base/system/uboot/test/boot_adb_raw_policy/CMakeLists.txt"
        "$build_arcs_base/components/tinyusb-appclass/adb/adb_sync.c"
        "$build_arcs_base/components/tinyusb-appclass/adb/adb_sync_ext_disk.c"
        "$build_arcs_base/components/tinyusb-appclass/adb/adb_sync_ext_disk.h"
    )

    local required_file
    for required_file in "${required_files[@]}"; do
        if [[ ! -f "$required_file" ]]; then
            printf 'fail=missing prerequisite file: %s\n' "$required_file" >>"$preflight_file"
            failures=$((failures + 1))
        fi
    done

    if [[ -f "$build_arcs_base/components/tinyusb-appclass/adb/adb_shell.h" ]] && \
        ! grep -Fq 'adb_shell_flush' "$build_arcs_base/components/tinyusb-appclass/adb/adb_shell.h"; then
        printf 'fail=adb_shell_flush declaration missing from %s/components/tinyusb-appclass/adb/adb_shell.h\n' "$build_arcs_base" >>"$preflight_file"
        failures=$((failures + 1))
    fi

    local config_line
    for config_line in \
        'CONFIG_BOOT_ADB=y' \
        'CONFIG_BOOT_ADB_SHELL=y' \
        'CONFIG_BOOT_ADB_SYNC=y'; do
        if grep -Eq "^${config_line}$" "$SCRIPT_DIR/prj.conf"; then
            printf 'ok=sample config contains %s\n' "$config_line" >>"$preflight_file"
        else
            printf 'fail=sample config missing %s in %s/prj.conf\n' "$config_line" "$SCRIPT_DIR" >>"$preflight_file"
            failures=$((failures + 1))
        fi
    done

    if [[ -f "$REPO_ROOT/build/build_root_context.txt" ]]; then
        printf 'info=existing build_root_context.txt captured for later truth checks\n' >>"$preflight_file"
    fi

    printf 'failure_count=%d\n' "$failures" >>"$preflight_file"
    cat "$preflight_file"

    if (( failures > 0 )); then
        return 1
    fi

    return 0
}

run_sample_contract_check() {
    if ! record_command recovery_contract python3 "$SCRIPT_DIR/test_recovery_contract.py" "$@"; then
        fail "Sample-owned recovery contract drifted; see $RUN_DIR/recovery_contract.txt"
    fi
}

main() {
    local build_arcs_base
    local port
    local normal_serial_timeout
    local recovery_serial_timeout

    find_arcs_base || fail "Could not resolve ARCS_BASE from $REPO_ROOT"
    build_arcs_base="$RESOLVED_ARCS_BASE"

    write_note "$RUN_DIR/context.txt" \
        "repo_root=$REPO_ROOT" \
        "repo_realpath=$REPO_REALPATH" \
        "build_arcs_base=$build_arcs_base" \
        "build_root_source=${BUILD_ROOT_SOURCE:-unknown}" \
        "build_shared_root=${BUILD_SHARED_ROOT:-}" \
        "planned_overlay_root=${PLANNED_OVERLAY_ROOT:-}" \
        "boot_source_dir=${BUILD_BOOT_SOURCE_DIR:-}" \
        "boot_cache=${BUILD_CACHE_FILE:-}" \
        "board=$BOARD" \
        "raw_sdraw_path=${RAW_SDRAW_PATH:-/RAW/SDRAW/0/1000}" \
        "raw_flash_fast_path=${RAW_FLASH_FAST_PATH:-/RAW/FLASH/40000/10000}" \
        "raw_nand_fast_path=${RAW_NAND_FAST_PATH:-/RAW/NAND/40000/10000}"

    capture_adb_devices adb_devices_baseline || true
    write_note "$RUN_DIR/serial_ports.txt" "$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true)"

    if ! run_preflight "$build_arcs_base"; then
        fail "Preflight failed; see $RUN_DIR/preflight.txt for the exact BOOT_ADB prerequisites"
    fi

    run_sample_contract_check --source-only

    if (( PREFLIGHT_ONLY == 1 )); then
        log "Preflight passed. Artifacts: $RUN_DIR"
        return 0
    fi

    if (( SKIP_BUILD == 0 )); then
    if ! record_command build bash "$REPO_ROOT/build.sh" -C -S samples/subsys/uboot/app_enter_boot_recovery -DBOARD="$BOARD"; then
            capture_build_truth "$build_arcs_base" || true
            fail "Build failed; see $RUN_DIR/build.txt and $RUN_DIR/build_truth.txt"
        fi
    else
        warn "Skipping build because --skip-build was provided"
    fi

    if ! capture_build_truth "$build_arcs_base"; then
        fail "Generated build truth is stale or incomplete; see $RUN_DIR/build_truth.txt"
    fi

    run_sample_contract_check

    if (( SKIP_BURN == 0 )); then
        port=$(find_serial_port) || fail "No serial port available for burn step"
        record_command burn_erase "$REPO_ROOT/tools/burn/cskburn" -C arcs -s "$port" -b "$BURN_BAUD" --erase-all
        record_command burn_main "$REPO_ROOT/tools/burn/cskburn" -C arcs -s "$port" -b "$BURN_BAUD" --verify-all 0x0 "$REPO_ROOT/build/arcs.bin"
    else
        warn "Skipping burn because --skip-burn was provided"
    fi

    normal_serial_timeout=$(max_timeout "$SERIAL_TIMEOUT" "$NORMAL_TIMEOUT")
    recovery_serial_timeout=$(max_timeout "$SERIAL_TIMEOUT" "$RECOVERY_TIMEOUT")

    start_serial_capture "$RUN_DIR/serial_normal.txt" "$normal_serial_timeout" || \
        fail "Could not start normal-cycle serial capture"

    if ! wait_for_file_contains_while_adb_quiet "$RUN_DIR/serial_normal.txt" "$NORMAL_RUNNING_MARKER" "$NORMAL_TIMEOUT" "adb_devices_normal_running_poll"; then
        stop_serial_capture
        fail "Did not observe trigger-firmware running marker; see $RUN_DIR/serial_normal.txt"
    fi

    capture_adb_devices adb_devices_normal
    assert_adb_device_count "$RUN_DIR/adb_devices_normal.txt" 0

    if ! wait_for_file_contains_while_adb_quiet "$RUN_DIR/serial_normal.txt" "$NORMAL_REQUEST_MARKER" "$NORMAL_TIMEOUT" "adb_devices_normal_request_poll"; then
        stop_serial_capture
        fail "Did not observe trigger-firmware recovery-request marker; see $RUN_DIR/serial_normal.txt"
    fi

    assert_markers_in_order "$RUN_DIR/serial_normal.txt" \
        "$NORMAL_RUNNING_MARKER" \
        "$NORMAL_REQUEST_MARKER"
    stop_serial_capture

    start_serial_capture "$RUN_DIR/serial_recovery_wait.txt" "$recovery_serial_timeout" || \
        fail "Could not start recovery-cycle serial capture"

    if ! wait_for_adb_device "$RECOVERY_TIMEOUT"; then
        stop_serial_capture
        fail "ADB device did not enumerate during explicit recovery; see $RUN_DIR/adb_devices_recovery_poll.txt and $RUN_DIR/serial_recovery_wait.txt"
    fi

    capture_adb_devices adb_devices_recovery
    assert_adb_device_count "$RUN_DIR/adb_devices_recovery.txt" 1

    if ! wait_for_file_contains "$RUN_DIR/serial_recovery_wait.txt" "$BOOT_REASON_MARKER" "$RECOVERY_TIMEOUT"; then
        stop_serial_capture
        fail "Did not observe recovery boot reason marker; see $RUN_DIR/serial_recovery_wait.txt"
    fi

    if ! wait_for_file_contains "$RUN_DIR/serial_recovery_wait.txt" "$RECOVERY_MARKER" "$RECOVERY_TIMEOUT"; then
        stop_serial_capture
        fail "Did not observe boot ADB recovery marker; see $RUN_DIR/serial_recovery_wait.txt"
    fi

    assert_markers_in_order "$RUN_DIR/serial_recovery_wait.txt" \
        "$BOOT_REASON_MARKER" \
        "$RECOVERY_MARKER"
    stop_serial_capture
    capture_serial serial_recovery || true

    record_command adb_status_initial "$ADB_BIN" shell "root;listenai;upgrade status"
    assert_contains "$RUN_DIR/adb_status_initial.txt" "upgrade status: exit"

    if ! record_command adb_status_bad_credentials "$ADB_BIN" shell "root;wrongpass;upgrade status"; then
        :
    fi
    record_command adb_status_after_bad_credentials "$ADB_BIN" shell "root;listenai;upgrade status"
    assert_contains "$RUN_DIR/adb_status_after_bad_credentials.txt" "upgrade status: exit"

    run_file_transfer_flow
    run_raw_sdraw_flow
    run_raw_flash_fastpath_flow

    write_note "$RUN_DIR/result.txt" \
        "PASS: normal boot stayed ADB-dark before soft recovery was armed" \
        "PASS: trigger firmware requested soft recovery on its own" \
        "PASS: build truth captured generated boot configs and CMake cache" \
        "PASS: explicit recovery enumerated exactly one ADB device" \
        "PASS: quoted adb shell upgrade status remained available in recovery" \
        "PASS: bad credentials kept upgrade status at exit" \
        "PASS: adb push and adb pull preserved file payload checksum" \
        "PASS: relative file-path adb push --sync updated the default root and stale sync was skipped" \
        "PASS: SDRAW push/pull preserved checksum evidence" \
        "PASS: recovery unlock switched upgrade status to enter" \
        "PASS: aligned raw flash fast-path push succeeded on FLASH and NAND aliases"

    log "Bench completed. Artifacts: $RUN_DIR"
}

main "$@"
