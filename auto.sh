#!/bin/bash
# =============================================================================
# auto.sh - 一键烧录 arcs-mini app 分区 (单设备版, 自动识别正确的 adb 设备)
#
# 用法:
#   ./auto.sh                # 自动探测 listenai 开发板 (排除手机等无关设备)
#   ./auto.sh FFBBCCDDEE001124   # 显式指定 adb serial
#
# 流程: 重置串口日志 -> 断开网络 adb 干扰源 -> 目标设备进 recovery -> push app 镜像 -> 重启
# 注意: 构建目录是 build-nes (构建: ./build.sh -S ./apps/arcs-mini -B build-nes -DBOARD=arcs_mini)
# =============================================================================
set -euo pipefail
set -x

cd "$(dirname "$0")"

# ---- 0. 重置串口日志: 给 picocom 发 SIGUSR1, 使其重置 log.txt ----
PICOCOM_PAT='^picocom .*--logfile log\.txt'
if pgrep -f "$PICOCOM_PAT" >/dev/null 2>&1; then
    # picocom 经 sudo 启动(root 属主), 需要 sudo 发信号
    sudo pkill -USR1 -f "$PICOCOM_PAT" || true
    sleep 0.3
    echo "==> 已发送 SIGUSR1, picocom 重置 log.txt"
fi

BIN=build-nes/arcs-mini.bin
RAW_PATH=/RAW/NAND/600000
# NES ROM 独立 flash 分区 (partition_table.json: nes_rom @ 0xE00000)
ROM_BIN=res/arcs-mini/nes_rom.bin
ROM_RAW_PATH=/RAW/NAND/E00000
BOOT_WAIT_SEC=30

[ -f "$BIN" ] || { echo "!! 固件不存在: $BIN (先构建)"; exit 1; }

# 推送 app + nes_rom 分区 (ROM 分区缺失会导致游戏 "ROM load failed")
push_all() {
    local dev="$1"
    adb -s "$dev" push "$BIN" "$RAW_PATH" >/dev/null 2>&1 || { echo "!! app 推送失败"; return 1; }
    if [ -f "$ROM_BIN" ]; then
        adb -s "$dev" push "$ROM_BIN" "$ROM_RAW_PATH" >/dev/null 2>&1 || echo "!! nes_rom 推送失败 (游戏将无法加载)"
    else
        echo "!! 缺少 $ROM_BIN, 跳过 ROM 分区 (游戏将无法加载)"
    fi
    return 0
}

# ---- 1. 选定目标设备: 排除手机/网络 adb, 只认 listenai 开发板 ----
adb disconnect >/dev/null 2>&1 || true
sleep 1

SERIAL="${1:-}"
if [ -z "$SERIAL" ]; then
    # 排除手机与已处于 recovery 的 BOOT-* 设备
    mapfile -t BOARDS < <(adb devices -l | awk '$2=="device" && !/BOOT-/ && /model:listenai/ {print $1}')
    case ${#BOARDS[@]} in
        0)
            # 无普通模式设备: 若已有 listenai 板处于 recovery 则直接使用
            if adb devices | awk '$2=="device"' | grep -q "^BOOT-"; then
                SERIAL=$(adb devices | awk '$1 ~ /^BOOT-/ && $2=="device" {print $1; exit}')
                echo "==> 开发板已在 recovery: $SERIAL"
                BOOT="$SERIAL"
                push_all "$BOOT"
                sleep 1
                adb -s "$BOOT" shell "reboot hard" >/dev/null 2>&1 || true
                echo "==> 烧录完成, 设备重启中"
                exit 0
            fi
            echo "!! 未发现 listenai 开发板 (adb devices -l 检查)"
            exit 1 ;;
        1) SERIAL="${BOARDS[0]}" ;;
        *) echo "!! 发现多台开发板, 请指定: ./auto.sh <serial>"; printf '  %s\n' "${BOARDS[@]}"; exit 1 ;;
    esac
fi
adb -s "$SERIAL" get-state >/dev/null 2>&1 || { echo "!! 设备不在线: $SERIAL"; exit 1; }
echo "==> 目标设备: $SERIAL"

# ---- 2. 进入 recovery (boot 侧接受 /RAW/NAND push); 已在 recovery 则跳过 ----
if [[ "$SERIAL" == BOOT-* ]]; then
    BOOT="$SERIAL"
    echo "==> 设备已在 recovery: $BOOT"
else
    adb -s "$SERIAL" shell "recovery" >/dev/null 2>&1 || \
    adb -s "$SERIAL" reboot recovery >/dev/null 2>&1 || true
fi

# ---- 3. 等待 BOOT-* 枚举 ----
BOOT=""
for i in $(seq 1 "$BOOT_WAIT_SEC"); do
    sleep 1
    BOOT=$(adb devices | awk '$1 ~ /^BOOT-/ && $2 == "device" {print $1; exit}')
    [ -n "$BOOT" ] && break
done
[ -n "$BOOT" ] || { echo "!! ${BOOT_WAIT_SEC}s 内未进入 recovery (BOOT-* 未枚举)"; exit 1; }
echo "==> recovery: $BOOT"

# ---- 4. push 固件并重启 ----
push_all "$BOOT"
sleep 1
adb -s "$BOOT" shell "reboot hard" >/dev/null 2>&1 || true
echo "==> 烧录完成, 设备重启中"
