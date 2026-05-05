#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SKILL_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SKILL_DIR/../../.." && pwd)

readonly BUILD_SCRIPT="$REPO_ROOT/build.sh"
readonly BURN_TOOL="$REPO_ROOT/tools/burn/cskburn"
readonly PACKAGE_TOOL="$REPO_ROOT/system/uboot/tools/package_ota.py"
readonly SERIAL_HELPER="$REPO_ROOT/.project/skills/sdk-assistant-agent/references/scripts/serial_read.py"
readonly BOOT_SAMPLE="samples/subsys/uboot/recovery_basic"
readonly APP_SAMPLE="samples/subsys/uboot_ota/app_only_trigger"
readonly APP_RES_CONFIG="$REPO_ROOT/samples/subsys/uboot_ota/app_only_trigger/res/config.json"
readonly DEFAULT_SERIAL="/dev/ttyACM0"
readonly DEFAULT_BOARD="arcs_evb"
readonly DEFAULT_TF_PATH="download/update.txz"
readonly DEFAULT_OUT_ROOT="$REPO_ROOT/.cache/uboot_ota_board_verify"
readonly APP_SLOT_BURN_ADDR="0x40000"
readonly FLASH_OTA_BURN_ADDR="0x600000"
readonly FLASH_OTA_OFFSET_HEX="0x00600000"
readonly FLASH_BASE_ADDR_HEX="0x30000000"
readonly BURN_BAUD="3000000"
readonly SERIAL_BAUD="921600"

SOURCE=""
SERIAL="$DEFAULT_SERIAL"
BOARD="$DEFAULT_BOARD"
TF_PATH="$DEFAULT_TF_PATH"
OUT_ROOT="$DEFAULT_OUT_ROOT"
RUN_DIR=""
RECOVERY_BUILD_DIR=""
APP_BUILD_DIR=""
RECOVERY_BOOT_BIN=""
APP_BIN=""
OTA_TXZ=""
OTA_SIZE_DEC=0
OTA_SIZE_HEX=""
STATUS="FAIL"
REASON="not_started"
SUMMARY_LINES=()
CAPTURE_PID=""

log() {
    printf '[uboot-ota-board-verify] %s\n' "$*"
}

add_summary() {
    SUMMARY_LINES+=("$1")
}

usage() {
    cat <<EOF2
Usage: bash .project/skills/uboot-ota-board-verify/scripts/run_verify.sh --source <flash|tf> [options]

Options:
  --source <flash|tf>     OTA source to verify (required)
  --serial <path>         Serial port used by cskburn and log capture (default: $DEFAULT_SERIAL)
  --board <board>         Build board (default: $DEFAULT_BOARD)
  --tf-path <path>        OTA request path for TF mode (default: $DEFAULT_TF_PATH)
  --out-root <dir>        Evidence output root (default: $DEFAULT_OUT_ROOT)
  -h, --help              Show this help text
EOF2
}

write_summary() {
    local summary_path

    if [[ -z "$RUN_DIR" ]]; then
        return 0
    fi

    mkdir -p "$RUN_DIR"
    summary_path="$RUN_DIR/summary.txt"

    {
        printf 'status=%s\n' "$STATUS"
        printf 'reason=%s\n' "$REASON"
        printf 'source=%s\n' "$SOURCE"
        printf 'board=%s\n' "$BOARD"
        printf 'serial=%s\n' "$SERIAL"
        printf 'tf_path=%s\n' "$TF_PATH"
        printf 'run_dir=%s\n' "$RUN_DIR"
        printf 'recovery_build_dir=%s\n' "$RECOVERY_BUILD_DIR"
        printf 'app_build_dir=%s\n' "$APP_BUILD_DIR"
        printf 'recovery_boot_bin=%s\n' "$RECOVERY_BOOT_BIN"
        printf 'app_bin=%s\n' "$APP_BIN"
        printf 'ota_txz=%s\n' "$OTA_TXZ"
        printf 'ota_size_hex=%s\n' "$OTA_SIZE_HEX"
        printf '\n[checks]\n'
        for line in "${SUMMARY_LINES[@]}"; do
            printf '%s\n' "$line"
        done
    } > "$summary_path"
}

trap write_summary EXIT

fail() {
    REASON="$*"
    log "ERROR: $*"
    exit 1
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        fail "missing required command: $1"
    fi
}

require_file() {
    if [[ ! -f "$1" ]]; then
        fail "missing required file: $1"
    fi
}

record_command() {
    local name="$1"
    local outfile
    local rc

    shift
    outfile="$RUN_DIR/${name}.txt"
    log "Running [$name]: $*"

    if (set -o pipefail; "$@" 2>&1 | tee "$outfile"); then
        add_summary "cmd.$name=PASS"
        return 0
    fi

    rc=$?
    add_summary "cmd.$name=FAIL(rc=$rc)"
    return "$rc"
}

normalize_tf_device_path() {
    case "$1" in
        /SD:/*)
            printf '%s\n' "$1"
            ;;
        /*)
            printf '/SD:%s\n' "$1"
            ;;
        *)
            printf '/SD:/%s\n' "$1"
            ;;
    esac
}

start_serial_capture() {
    local outfile="$1"
    local timeout_s="$2"
    local attempts="${3:-200}"

    (
        local attempt

        for attempt in $(seq 1 "$attempts"); do
            if python3 "$SERIAL_HELPER" "$SERIAL" -b "$SERIAL_BAUD" -t "$timeout_s" > "$outfile" 2>&1; then
                exit 0
            fi
            sleep 0.2
        done

        exit 1
    ) &

    CAPTURE_PID=$!
}

run_with_serial_capture() {
    local action_name="$1"
    local serial_name="$2"
    local serial_timeout_s="$3"
    local serial_out
    local capture_pid

    shift 3
    serial_out="$RUN_DIR/${serial_name}.txt"
    start_serial_capture "$serial_out" "$serial_timeout_s"
    capture_pid="$CAPTURE_PID"

    if ! record_command "$action_name" "$@"; then
        wait "$capture_pid" || true
        return 1
    fi

    if ! wait "$capture_pid"; then
        add_summary "serial.$serial_name=FAIL"
        return 1
    fi

    add_summary "serial.$serial_name=PASS"
    return 0
}

reset_board_via_dtr() {
    python3 - "$SERIAL" <<'PY'
import fcntl
import os
import struct
import sys
import time

TIOCMBIS = 0x5416
TIOCMBIC = 0x5417
TIOCM_DTR = 0x002

fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
try:
    fcntl.ioctl(fd, TIOCMBIS, struct.pack("I", TIOCM_DTR))
    time.sleep(0.5)
    fcntl.ioctl(fd, TIOCMBIC, struct.pack("I", TIOCM_DTR))
finally:
    os.close(fd)
PY
}

capture_after_reset() {
    local serial_name="$1"
    local serial_timeout_s="$2"
    local serial_out
    local capture_pid

    serial_out="$RUN_DIR/${serial_name}.txt"
    start_serial_capture "$serial_out" "$serial_timeout_s" 80
    capture_pid="$CAPTURE_PID"

    if ! reset_board_via_dtr > "$RUN_DIR/reset_board.txt" 2>&1; then
        wait "$capture_pid" || true
        return 1
    fi

    if ! wait "$capture_pid"; then
        add_summary "serial.$serial_name=FAIL"
        return 1
    fi

    add_summary "serial.$serial_name=PASS"
    return 0
}

sha256_from_file() {
    tr -d '\r' < "$1" | sed -n 's/^\([0-9a-f]\{64\}\).*/\1/p' | head -n 1
}

md5_from_file() {
    tr -d '\r' < "$1" | sed -n 's/^\([0-9a-f]\{32\}\).*/\1/p' | head -n 1
}

require_config_line() {
    local file="$1"
    local expected="$2"
    local key="$3"

    if grep -Fxq "$expected" "$file"; then
        add_summary "config.$key=PASS"
        return 0
    fi

    add_summary "config.$key=FAIL"
    fail "missing config line in $file: $expected"
}

require_marker() {
    local file="$1"
    local marker="$2"
    local key="$3"

    if grep -Fq "$marker" "$file"; then
        add_summary "marker.$key=PASS"
        return 0
    fi

    add_summary "marker.$key=FAIL"
    fail "missing marker [$marker] in $file"
}

wait_for_adb_device() {
    local timeout_s="$1"
    local deadline
    local poll_log="$RUN_DIR/adb_devices_poll.txt"
    local current="$RUN_DIR/adb_devices_current.txt"
    local final="$RUN_DIR/adb_devices_ready.txt"

    : > "$poll_log"
    deadline=$(( $(date +%s) + timeout_s ))

    while true; do
        adb devices > "$current" || true
        cat "$current" >> "$poll_log"
        printf '\n' >> "$poll_log"

        if awk '/\tdevice$/{found=1} END{exit !found}' "$current"; then
            mv "$current" "$final"
            add_summary 'adb.wait=PASS'
            return 0
        fi

        if (( $(date +%s) >= deadline )); then
            mv "$current" "$final"
            add_summary 'adb.wait=FAIL'
            return 1
        fi

        sleep 1
    done
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --source)
                SOURCE="$2"
                shift 2
                ;;
            --serial)
                SERIAL="$2"
                shift 2
                ;;
            --board)
                BOARD="$2"
                shift 2
                ;;
            --tf-path)
                TF_PATH="$2"
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
                usage >&2
                fail "unknown argument: $1"
                ;;
        esac
    done

    case "$SOURCE" in
        flash|tf)
            ;;
        *)
            usage >&2
            fail '--source must be flash or tf'
            ;;
    esac
}

setup_run_dir() {
    local stamp

    stamp=$(date +%Y%m%d-%H%M%S)
    RUN_DIR="$OUT_ROOT/${stamp}-${SOURCE}"
    mkdir -p "$RUN_DIR/build"
    ln -sfn "$RUN_DIR" "$OUT_ROOT/latest"
}

preflight() {
    require_command python3
    require_file "$BUILD_SCRIPT"
    require_file "$BURN_TOOL"
    require_file "$PACKAGE_TOOL"
    require_file "$SERIAL_HELPER"
    require_file "$APP_RES_CONFIG"

    if [[ "$SOURCE" == "tf" ]]; then
        require_command adb
    fi

    add_summary "preflight=PASS"
}

build_recovery() {
    RECOVERY_BUILD_DIR="$RUN_DIR/build/recovery_basic"
    RECOVERY_BOOT_BIN="$RECOVERY_BUILD_DIR/boot.bin"

    record_command build_recovery \
        "$BUILD_SCRIPT" -C -S "$BOOT_SAMPLE" -B "$RECOVERY_BUILD_DIR" "-DBOARD=$BOARD" || \
        fail 'recovery_basic build failed'

    require_file "$RECOVERY_BOOT_BIN"
    require_config_line "$RECOVERY_BUILD_DIR/boot/.config" 'CONFIG_BOOT_ADB_SYNC=y' 'recovery_adb_sync'
    require_config_line "$RECOVERY_BUILD_DIR/boot/.config" 'CONFIG_BOOT_OTA_PACKAGE=y' 'recovery_ota_package'
    require_config_line "$RECOVERY_BUILD_DIR/boot/.config" 'CONFIG_BOOT_OTA_SOURCE_TF=y' 'recovery_tf_source'
}

build_app() {
    local -a cmd

    APP_BUILD_DIR="$RUN_DIR/build/app_only_trigger_${SOURCE}"
    APP_BIN="$APP_BUILD_DIR/uboot_ota_app_only_trigger.bin"
    cmd=("$BUILD_SCRIPT" -C -S "$APP_SAMPLE" -B "$APP_BUILD_DIR" "-DBOARD=$BOARD")

    if [[ "$SOURCE" == "tf" ]]; then
        cmd+=(
            '-DCONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH=n'
            '-DCONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF=y'
            "-DCONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF_PATH=\"$TF_PATH\""
        )
    fi

    record_command build_app "${cmd[@]}" || fail 'app_only_trigger build failed'
    require_file "$APP_BIN"

    if [[ "$SOURCE" == "flash" ]]; then
        require_config_line "$APP_BUILD_DIR/.config" 'CONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH=y' 'app_flash_source'
    else
        require_config_line "$APP_BUILD_DIR/.config" '# CONFIG_SAMPLE_BOOT_OTA_TRIGGER_FLASH is not set' 'app_flash_disabled'
        require_config_line "$APP_BUILD_DIR/.config" 'CONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF=y' 'app_tf_source'
        require_config_line "$APP_BUILD_DIR/.config" "CONFIG_SAMPLE_BOOT_OTA_TRIGGER_TF_PATH=\"$TF_PATH\"" 'app_tf_path'
    fi
}

package_ota() {
    OTA_TXZ="$APP_BUILD_DIR/ota.txz"

    record_command package_ota \
        python3 "$PACKAGE_TOOL" --config "$APP_RES_CONFIG" --txz "$OTA_TXZ" || \
        fail 'package_ota.py failed'

    require_file "$OTA_TXZ"
    OTA_SIZE_DEC=$(stat -c %s "$OTA_TXZ")
    OTA_SIZE_HEX=$(printf '0x%x' "$OTA_SIZE_DEC")
    add_summary "ota.size=$OTA_SIZE_HEX"
}

verify_flash_package_size() {
    local configured

    configured=$(awk -F= '/^CONFIG_SAMPLE_BOOT_OTA_FLASH_PACKAGE_SIZE=/{print $2}' "$APP_BUILD_DIR/.config")
    if [[ -z "$configured" ]]; then
        fail 'missing CONFIG_SAMPLE_BOOT_OTA_FLASH_PACKAGE_SIZE in app config'
    fi

    if [[ "${configured,,}" != "$OTA_SIZE_HEX" ]]; then
        add_summary "flash.package_size=FAIL(config=$configured actual=$OTA_SIZE_HEX)"
        fail "flash package size mismatch: config=$configured actual=$OTA_SIZE_HEX"
    fi

    add_summary "flash.package_size=PASS($OTA_SIZE_HEX)"
}

burn_recovery_boot() {
    record_command erase_all \
        "$BURN_TOOL" -C arcs -s "$SERIAL" -b "$BURN_BAUD" --erase-all || \
        fail 'erase-all failed'

    run_with_serial_capture burn_recovery_boot serial_recovery_boot 10 \
        "$BURN_TOOL" -C arcs -s "$SERIAL" -b "$BURN_BAUD" --verify-all 0x0 "$RECOVERY_BOOT_BIN" || \
        fail 'burn recovery boot failed'

    require_marker "$RUN_DIR/serial_recovery_boot.txt" 'boot adb: stage=ready' 'recovery_boot_ready'
}

prepare_flash_media() {
    verify_flash_package_size

    record_command burn_flash_ota \
        "$BURN_TOOL" -C arcs -s "$SERIAL" -b "$BURN_BAUD" --verify-all "$FLASH_OTA_BURN_ADDR" "$OTA_TXZ" || \
        fail 'burn flash ota package failed'
}

prepare_tf_media() {
    local device_path
    local host_sha
    local remote_sha

    device_path=$(normalize_tf_device_path "$TF_PATH")
    add_summary "tf.device_path=$device_path"

    adb kill-server >/dev/null 2>&1 || true
    adb start-server > "$RUN_DIR/adb_start_server.txt" 2>&1 || true

    wait_for_adb_device 20 || fail 'adb device did not enumerate after recovery boot'

    record_command host_ota_sha256 md5sum "$OTA_TXZ" || fail 'failed to hash host ota package'
    record_command adb_push_ota adb push "$OTA_TXZ" "$device_path" || fail 'adb push ota package failed'

    # boot shell has no file-level hash command; pull the file back and compare on host
    local pulled="$RUN_DIR/ota_pulled.txz"
    record_command adb_pull_ota adb pull "$device_path" "$pulled" || fail 'adb pull ota package failed'
    record_command adb_sha256_ota md5sum "$pulled" || fail 'failed to hash pulled ota package'

    host_sha=$(md5_from_file "$RUN_DIR/host_ota_sha256.txt")
    remote_sha=$(md5_from_file "$RUN_DIR/adb_sha256_ota.txt")

    if [[ -z "$host_sha" || -z "$remote_sha" ]]; then
        add_summary 'tf.md5=FAIL(parse)'
        fail 'failed to parse tf md5 output'
    fi

    if [[ "$host_sha" != "$remote_sha" ]]; then
        add_summary "tf.md5=FAIL(host=$host_sha remote=$remote_sha)"
        fail 'host/device tf package md5 mismatch'
    fi

    add_summary "tf.md5=PASS($host_sha)"
}

verify_serial_flow() {
    local serial_log="$RUN_DIR/serial_ota_flow.txt"
    local flash_src_addr
    local tf_device_path

    require_marker "$serial_log" 'APP-only OTA trigger sample running' 'app_running'
    require_marker "$serial_log" 'APP-only OTA request saved, rebooting' 'app_request_saved'
    require_marker "$serial_log" 'ota: txz update start' 'boot_txz_start'
    require_marker "$serial_log" "ota: txz update success" 'boot_txz_success'
    require_marker "$serial_log" 'ota: lifecycle updated, reboot' 'boot_lifecycle_updated'
    require_marker "$serial_log" 'APP-only OTA upgraded firmware running' 'upgraded_fw_running'

    if [[ "$SOURCE" == "flash" ]]; then
        flash_src_addr=$(printf '0x%08x' $((FLASH_BASE_ADDR_HEX + FLASH_OTA_OFFSET_HEX)))
        require_marker "$serial_log" 'APP-only OTA source: flash' 'app_flash_marker'
        require_marker "$serial_log" "ota: src=$flash_src_addr size=$OTA_SIZE_HEX" 'boot_flash_src_marker'
    else
        tf_device_path=$(normalize_tf_device_path "$TF_PATH")
        require_marker "$serial_log" "APP-only OTA source: tf path=$TF_PATH" 'app_tf_marker'
        require_marker "$serial_log" "ota: src=$tf_device_path size=$OTA_SIZE_HEX" 'boot_tf_src_marker'
    fi
}

verify_post_upgrade_reset() {
    capture_after_reset serial_post_upgrade_reset 8 || fail 'failed to capture post-upgrade reset log'
    require_marker "$RUN_DIR/serial_post_upgrade_reset.txt" 'APP-only OTA upgraded firmware running' 'post_reset_upgraded_fw'
}

main() {
    parse_args "$@"
    setup_run_dir
    preflight
    build_recovery
    build_app
    package_ota
    burn_recovery_boot

    if [[ "$SOURCE" == "flash" ]]; then
        prepare_flash_media
    else
        prepare_tf_media
    fi

    run_with_serial_capture burn_trigger_app serial_ota_flow 40 \
        "$BURN_TOOL" -C arcs -s "$SERIAL" -b "$BURN_BAUD" --verify-all "$APP_SLOT_BURN_ADDR" "$APP_BIN" || \
        fail 'burn trigger app failed'

    verify_serial_flow
    verify_post_upgrade_reset

    STATUS='PASS'
    REASON='verification completed'
    log "PASS: $SOURCE OTA board verification completed. Summary: $RUN_DIR/summary.txt"
}

main "$@"
