#!/bin/bash
# auto.sh - SoundSense/master 一键构建 + ADB 烧录脚本
#
# 用法:
#   ./auto.sh            # 只烧 app 固件（默认，不构建）
#   ./auto.sh build      # 构建 + 只烧 app 固件
#   ./auto.sh log        # 重启设备并抓 10 秒启动日志到 ./tmp/run-log.txt
#
# 环境变量:
#   BOARD      板型（默认 arcs_mini）
#   BUILD_DIR  构建目录（默认 build-ss）
#   DEVICE     显式指定 adb 设备序列号
set -e
set -o pipefail

# 通知正在运行的 picocom（USR1），避免串口被日志占用
pkill -USR1 picocom 2>/dev/null || true

cd "$(dirname "$0")"

BOARD="${BOARD:-arcs_mini}"
BUILD_DIR="${BUILD_DIR:-build-ss}"
MODE="${1:-flash}"

BIN="$BUILD_DIR/arcs-mini.bin"

list_devices() {
    adb devices 2>/dev/null | awk 'NR>1 && $2=="device" {print $1}'
}

first_boot_device() {
    list_devices | grep -m1 '^BOOT-'
}

first_normal_device() {
    list_devices | grep -vm1 '^BOOT-'
}

pick_device() {
    if [ -n "$DEVICE" ]; then
        echo "$DEVICE"
        return
    fi
    local d
    d=$(first_boot_device) || true
    [ -n "$d" ] && { echo "$d"; return; }
    d=$(first_normal_device) || true
    echo "$d"
}

ensure_recovery() {
    if [ -n "$(first_boot_device)" ]; then
        return 0
    fi
    local dev
    dev=$(pick_device)
    if [ -z "$dev" ]; then
        echo "!! 没有在线的 adb 设备"
        return 1
    fi
    echo ">> device: $dev -> 进入 recovery ..."
    timeout 15 adb -s "$dev" reboot recovery >/dev/null 2>&1 || true
    local i
    for i in $(seq 1 12); do
        sleep 5
        if [ -n "$(first_boot_device)" ]; then
            echo ">> device: recovery 就绪 ($(first_boot_device))"
            return 0
        fi
    done
    echo "!! 设备未在 60s 内进入 recovery"
    return 1
}

check_bin() {
    [ -f "$BIN" ] || { echo "!! 固件不存在: $BIN，先运行 ./auto.sh build"; exit 1; }
}

do_build() {
    echo ">> build: -> $BUILD_DIR (BOARD=$BOARD)"
    source ./arcs-sdk/env.sh >/dev/null 2>&1
    local start_ts
    start_ts=$(date +%s)
    ./build.sh -S ./apps/arcs-mini -B "$BUILD_DIR" -DBOARD="$BOARD"
    if [ ! -f "$BIN" ]; then
        echo "!! 构建失败: $BIN 不存在"
        exit 1
    fi
    local bin_ts
    bin_ts=$(stat -c %Y "$BIN")
    if [ "$bin_ts" -lt "$start_ts" ]; then
        echo "!! 构建失败: $BIN 未更新"
        exit 1
    fi
    echo ">> build ok: $BIN ($(du -h "$BIN" | cut -f1))"
}

do_flash() {
    mkdir -p ./tmp
    ensure_recovery || return 1
    echo ">> flash: BUILD_DIR=$BUILD_DIR (日志: ./tmp/flash-log.txt)"
    if BUILD_DIR="$BUILD_DIR" bash adb_download.sh -S res/arcs-mini app \
        2>&1 | tee ./tmp/flash-log.txt; then
        echo ">> flash ok"
        return 0
    fi
    if grep -q "failed to read copy response: EOF" ./tmp/flash-log.txt \
       && grep -qE "[0-9]+ file pushed" ./tmp/flash-log.txt; then
        echo "!! EOF 误报，固件数据已完整推送"
        return 0
    fi
    echo "!! 烧录失败"
    return 1
}

do_log() {
    mkdir -p ./tmp
    local dev
    dev=$(pick_device)
    if [ -z "$dev" ]; then
        echo "!! 没有在线的 adb 设备"
        return 1
    fi
    echo ">> device: $dev"
    adb -s "$dev" shell reboot || true
    if ! timeout 90 adb -s "$dev" wait-for-device; then
        echo "!! 等待设备 ADB 上线超时"
        return 1
    fi
    timeout 10 adb -s "$dev" shell > ./tmp/run-log.txt 2>&1
    echo ">> log: ./tmp/run-log.txt ($(wc -l < ./tmp/run-log.txt) 行)"
}

case "$MODE" in
    flash)
        check_bin
        do_flash
        ;;
    build)
        do_build
        do_flash
        ;;
    log)
        do_log
        ;;
    *)
        echo "用法: ./auto.sh [flash|build|log]  (DEVICE=<serial> 可显式指定设备)"
        exit 1
        ;;
esac
