#!/bin/bash
set -x
# auto.sh - Anima/master 一键构建 + ADB 烧录脚本
#
# 用法:
#   ./auto.sh            # 只烧 app 固件（默认，不构建；日常烧录用这条）
#   ./auto.sh build      # 构建 + 只烧 app 固件
#   ./auto.sh full       # 只烧 ap/tone/wake_word/emoji/respak/app（不含 boot，不构建）
#   ./auto.sh all        # 构建 + 烧全部资源 + app（不含 boot）
#   ./auto.sh log        # 重启设备并抓 10 秒启动日志到 ./tmp/run-log.txt
#
# 环境变量:
#   BOARD      板型（默认 arcs_mini，doll_v2 用 arcs_mini_doll_v2）
#   BUILD_DIR  构建目录（默认 build-anima，与 nes 分支的 build-nes 惯例一致；
#              注意仓库根目录的 build/ 是 ble_peripheral 的旧缓存，勿混用）
#   APP        应用源码目录（默认 ./apps/arcs-mini）
#   DEVICE     显式指定 adb 设备序列号（普通模式或 BOOT-* 均可），默认自动选取
#
# 已知问题（本机设备实测）:
#   1. build.sh 在 CMake 失败时也可能返回 0 —— 本脚本用产物时间戳兜底校验
#   2. adb push 数据完整推送后，设备写完 NAND 不回最终应答，adb 报
#      "failed to read copy response: EOF"，adb_download.sh 按失败计 —— 属误报，
#      本脚本识别后放行，建议用 ./auto.sh log 验证新固件运行
#   3. "adb reboot recovery" 常报 error: closed 但实际生效 —— 脚本按结果轮询判断
set -e
set -o pipefail

# 通知正在运行的 picocom（USR1），避免串口被日志占用
pkill -USR1 picocom 2>/dev/null || true

cd "$(dirname "$0")"

BOARD="${BOARD:-arcs_mini}"
BUILD_DIR="${BUILD_DIR:-build-anima}"
APP="${APP:-./apps/arcs-mini}"
MODE="${1:-flash}"

# ---------------------------------------------------------------------------
# 0. 设备选择：所有 adb 操作显式 -s 指定设备
#    优先 BOOT-*（已在 recovery），其次普通模式设备；DEVICE 可强制指定
# ---------------------------------------------------------------------------
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

# 确保设备进入 recovery：发送 reboot recovery 后轮询等待 BOOT-* 上线
ensure_recovery() {
    if [ -n "$(first_boot_device)" ]; then
        return 0
    fi
    local dev
    dev=$(pick_device)
    if [ -z "$dev" ]; then
        echo "!! 没有在线的 adb 设备，请检查连接或手动指定: DEVICE=<serial> ./auto.sh"
        return 1
    fi
    echo ">> device: $dev -> 进入 recovery ..."
    timeout 15 adb -s "$dev" reboot recovery >/dev/null 2>&1 || true # 常假报错，按结果判断
    local i
    for i in $(seq 1 12); do
        sleep 5
        if [ -n "$(first_boot_device)" ]; then
            echo ">> device: recovery 就绪 ($(first_boot_device))"
            return 0
        fi
    done
    echo "!! 设备未在 60s 内进入 recovery，可重试或长按电源键后再运行"
    return 1
}

check_bin() {
    [ -f "$BIN" ] || { echo "!! 固件不存在: $BIN，先运行 ./auto.sh build"; exit 1; }
}

BIN="$BUILD_DIR/arcs-mini.bin"
FLASH_MODE="app"   # app=仅 app, default=资源+app

# ---------------------------------------------------------------------------
# 1. 构建并校验产物
# ---------------------------------------------------------------------------
do_build() {
    echo ">> build: $APP -> $BUILD_DIR (BOARD=$BOARD)"
    source ./arcs-sdk/env.sh >/dev/null 2>&1
    local start_ts
    start_ts=$(date +%s)
    ./build.sh -S "$APP" -B "$BUILD_DIR" -DBOARD="$BOARD"

    if [ ! -f "$BIN" ]; then
        echo "!! 构建失败: $BIN 不存在"
        exit 1
    fi
    local bin_ts
    bin_ts=$(stat -c %Y "$BIN")
    if [ "$bin_ts" -lt "$start_ts" ]; then
        echo "!! 构建失败: $BIN 未更新（时间戳早于本次构建）"
        exit 1
    fi
    echo ">> build ok: $BIN ($(du -h "$BIN" | cut -f1))"
}

# ---------------------------------------------------------------------------
# 2. ADB 烧录（复用 adb_download.sh，多设备并发；先确保设备已进 recovery）
# ---------------------------------------------------------------------------
do_flash() {
    mkdir -p ./tmp
    ensure_recovery || return 1
    echo ">> flash: BUILD_DIR=$BUILD_DIR MODE=$FLASH_MODE (日志: ./tmp/flash-log.txt)"
    if BUILD_DIR="$BUILD_DIR" bash adb_download.sh -S res/arcs-mini "$FLASH_MODE" \
        2>&1 | tee ./tmp/flash-log.txt; then
        echo ">> flash ok"
        return 0
    fi
    # 已知误报: 固件数据已完整推送（"N file pushed"），仅最终应答缺失（EOF）
    if grep -q "failed to read copy response: EOF" ./tmp/flash-log.txt \
       && grep -qE "[0-9]+ file pushed" ./tmp/flash-log.txt; then
        echo "!! adb_download.sh 报告失败，但固件数据已完整推送（EOF 误报，本机设备已知现象）"
        echo "   大概率已烧录成功，重启设备后执行 ./auto.sh log 验证新固件是否运行"
        return 0
    fi
    echo "!! 烧录失败，详见 ./tmp/flash-log.txt"
    return 1
}

# ---------------------------------------------------------------------------
# 3. 抓启动日志
# ---------------------------------------------------------------------------
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
        echo "!! 等待设备 ADB 上线超时（新固件可能启动失败，或 adb 未起来）"
        echo "   可改用串口: timeout 10 picocom -b 921600 --lower-dtr --lower-rts /dev/ttyACM0"
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
    full)
        check_bin
        FLASH_MODE="default"
        do_flash
        ;;
    all)
        FLASH_MODE="default"
        do_build
        do_flash
        ;;
    log)
        do_log
        ;;
    *)
        echo "用法: ./auto.sh [flash|build|full|all|log]  (DEVICE=<serial> 可显式指定设备)"
        exit 1
        ;;
esac
