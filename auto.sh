#!/bin/bash
# auto.sh - Yolo/master LNN demo 一键构建 + ADB 烧录脚本
#
# 用法:
#   ./auto.sh            # 只烧 AP+CP+模型（默认，不构建）
#   ./auto.sh build      # 构建 demo + 烧录 AP+CP+模型
#   ./auto.sh app        # 只烧 AP+CP（跳过 2MB 模型，快速迭代）
#   ./auto.sh build app  # 构建 + 只烧 AP+CP
#   ./auto.sh boot       # 额外烧录 boot 分区（res/arcs-mini/boot.bin → 0x0，
#                        # 仅 boot 本身变更时用；烧错 boot 需串口救砖）
#   ./auto.sh log        # 抓 15 秒串口日志到 ./tmp/run-log.txt
#
# 环境变量:
#   BOARD        板型（默认 arcs_mini）
#   BUILD_DIR    构建目录（默认 build-lnn）
#   DEVICE       显式指定 adb 设备序列号（普通模式 or BOOT- 序列号）
#   WAIT_DEVICE  等待设备插入的秒数（默认 15）
#   WAIT_RECOVERY 等待进入 recovery 的秒数（默认 60）
#   DEBUG=1      打印脚本执行轨迹（set -x）
#
# 设备模式自动侦测（按 adb devices 序列号前缀）:
#   BOOT-*  → 已在 BOOT recovery 烧录模式，直接烧
#   其他    → 普通模式（产品固件或带 ADB 的 demo），自动 reboot recovery 后烧
#   无设备  → 等待插入；demo 无 ADB 时需 burn_serial.sh 串口救援
#
# 分区映射（产品分区表，0x0 boot 永不触碰）:
#   AP@0x40000  CP@0x600000  模型@0x200000(借 wake_word 分区)

set -e
set -o pipefail
[ -n "$DEBUG" ] && set -x

# 通知正在运行的 picocom（USR1），避免串口被日志占用
pkill -USR1 picocom 2>/dev/null || true

cd "$(dirname "$0")"

BOARD="${BOARD:-arcs_mini}"
BUILD_DIR="${BUILD_DIR:-build-lnn}"
DEMO_DIR="arcs-sdk/labs/lnn/thinker_resnet18_real"
WAIT_DEVICE="${WAIT_DEVICE:-15}"
WAIT_RECOVERY="${WAIT_RECOVERY:-60}"
MODE=""

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'

# 参数解析：build / app / boot / log 任意组合
DO_BUILD=0
FLASH_MODEL=1
FLASH_BOOT=0
for arg in "$@"; do
    case "$arg" in
    build) DO_BUILD=1 ;;
    app) FLASH_MODEL=0 ;;
    boot) FLASH_BOOT=1 ;;
    log) MODE="log" ;;
    *) echo "未知参数: $arg（可用: build | app | boot | log）"; exit 1 ;;
    esac
done
[ "$MODE" = "log" ] && DO_BUILD=0

# ---- 环境与工具链（build.sh 需要正确的 Nuclei 工具链）----------------------
ROOT="$(pwd)"
export ARCS_BASE="${ARCS_BASE:-$ROOT/arcs-sdk}"
if [ -z "$NUCLEI_TOOLCHAIN_PATH" ] && [ -d "$ROOT/listenai-dev-tools/gcc" ]; then
    export NUCLEI_TOOLCHAIN_PATH="$ROOT/listenai-dev-tools/gcc"
fi
if [ -d "$ROOT/listenai-dev-tools/listenai-tools" ]; then
    export LISTENAI_TOOLS_PATH="$ROOT/listenai-dev-tools/listenai-tools"
fi
export PATH="$NUCLEI_TOOLCHAIN_PATH/bin:$LISTENAI_TOOLS_PATH/bin:$PATH"

# ---- 镜像路径 --------------------------------------------------------------
AP_BIN="$BUILD_DIR/remote/thinker_resnet18_real_ap.bin"
CP_BIN="$BUILD_DIR/thinker_resnet18_real_cp.bin"
MODEL_BIN="${MODEL_BIN:-tmp/yolo192f.pkg}"
BOOT_BIN="res/arcs-mini/boot.bin"

# ---- 分区地址 ---------------------------------------------------------------
BOOT_ADDR=0         # 0x0      boot 分区（仅 boot 变更时烧，烧错需串口救砖）
AP_ADDR=40000       # 0x40000  ap 分区
MODEL_ADDR=200000   # 0x200000 wake_word 分区（demo 不用资源分区，借放模型）
CP_ADDR=600000      # 0x600000 app 分区

# ============================================================================
# 日志模式
# ============================================================================
if [ "$MODE" = "log" ]; then
    mkdir -p ./tmp
    echo ">> 抓 15 秒串口日志 → ./tmp/run-log.txt（demo 日志在 UART0 921600）"
    timeout 15 picocom -b 921600 /dev/ttyACM0 > ./tmp/run-log.txt 2>&1 || true
    echo ">> 完成: $(wc -l < ./tmp/run-log.txt) 行"
    exit 0
fi

# ============================================================================
# 构建
# ============================================================================
if [ "$DO_BUILD" = 1 ]; then
    echo ">> build: $DEMO_DIR -> $BUILD_DIR (BOARD=$BOARD)"
    ./build.sh -S "$DEMO_DIR" -B "$BUILD_DIR" -DBOARD="$BOARD"
fi

for f in "$AP_BIN" "$CP_BIN" \
         $([ $FLASH_MODEL = 1 ] && echo "$MODEL_BIN") \
         $([ $FLASH_BOOT = 1 ] && echo "$BOOT_BIN"); do
    if [ ! -f "$f" ]; then
        echo -e "${RED}错误: 缺少 $f（先运行: ./auto.sh build）${NC}"
        exit 1
    fi
done

# ============================================================================
# 设备模式侦测与烧录
# ============================================================================
# 在线设备列表（仅 state=device 的）
adb_device_list() {
    adb devices 2>/dev/null | awk 'NR>1 && $2=="device" {print $1}'
}

# 侦测当前模式：输出 recovery:<sn> | normal:<sn> | none
# 规则：BOOT- 前缀 = BOOT recovery 烧录模式；其余 = 普通模式
detect_mode() {
    local sns boot normal
    sns=$(adb_device_list)
    [ -z "$sns" ] && { echo "none"; return; }
    boot=$(echo "$sns" | grep -m1 '^BOOT-' || true)
    if [ -n "$boot" ]; then
        echo "recovery:$boot"
        return
    fi
    if [ -n "$DEVICE" ]; then
        normal=$(echo "$sns" | grep -m1 "^$DEVICE\$" || true)
    else
        normal=$(echo "$sns" | head -1)
    fi
    [ -n "$normal" ] && echo "normal:$normal" || echo "none"
}

adb_of() {
    adb -s "$CUR_SN" "$@"
}

# 1) 等待设备出现
MODE_SN="none"
for i in $(seq 1 "$WAIT_DEVICE"); do
    MODE_SN=$(detect_mode)
    [ "$MODE_SN" != "none" ] && break
    [ "$i" = 1 ] && echo -e "${YELLOW}>> 未发现设备，等待插入（最多 ${WAIT_DEVICE}s）...${NC}"
    sleep 1
done

if [ "$MODE_SN" = "none" ]; then
    echo -e "${RED}错误: 没有可用 ADB 设备。${NC}"
    echo "  - 检查设备自身 USB 线（非串口适配器那条）"
    echo "  - 设备跑无 ADB 固件时: sudo ./burn_serial.sh 串口救援"
    exit 1
fi

MODE_NAME="${MODE_SN%%:*}"
CUR_SN="${MODE_SN#*:}"

# 2) 普通模式 → 自动 reboot recovery
if [ "$MODE_NAME" = "normal" ]; then
    echo -e ">> 设备 ${GREEN}$CUR_SN${NC} 处于${YELLOW}普通模式${NC}，自动切换到 BOOT recovery ..."
    adb -s "$CUR_SN" reboot recovery || true

    TARGET_SN=""
    for i in $(seq 1 "$WAIT_RECOVERY"); do
        sleep 1
        MODE_SN=$(detect_mode)
        if [[ "$MODE_SN" == recovery:* ]]; then
            TARGET_SN="${MODE_SN#*:}"
            break
        fi
    done
    if [ -z "$TARGET_SN" ]; then
        echo -e "${RED}错误: ${WAIT_RECOVERY}s 内未进入 BOOT recovery（串口看 boot 日志）${NC}"
        exit 1
    fi
    CUR_SN="$TARGET_SN"
    echo -e ">> 已进入 ${GREEN}BOOT recovery${NC}（$CUR_SN）"
else
    echo -e ">> 设备 ${GREEN}$CUR_SN${NC} 已在${GREEN}BOOT recovery${NC} 模式"
fi

# 3) 烧录
echo ">> 烧录目标: $CUR_SN"
[ $FLASH_BOOT = 1 ] && echo -e "   BOOT → /RAW/NAND/$BOOT_ADDR     ($(du -h "$BOOT_BIN" | cut -f1)) ${RED}⚠️ 仅 boot 变更时使用${NC}"
echo "   AP   → /RAW/NAND/$AP_ADDR   ($(du -h "$AP_BIN" | cut -f1))"
echo "   CP   → /RAW/NAND/$CP_ADDR   ($(du -h "$CP_BIN" | cut -f1))"
[ $FLASH_MODEL = 1 ] && echo "   模型 → /RAW/NAND/$MODEL_ADDR ($(du -h "$MODEL_BIN" | cut -f1))"

mkdir -p ./tmp
{
    if [ $FLASH_BOOT = 1 ]; then
        adb_of push "$BOOT_BIN" "/RAW/NAND/$BOOT_ADDR" || exit 1
    fi
    adb_of push "$AP_BIN" "/RAW/NAND/$AP_ADDR" || exit 1
    adb_of push "$CP_BIN" "/RAW/NAND/$CP_ADDR" || exit 1
    if [ $FLASH_MODEL = 1 ]; then
        adb_of push "$MODEL_BIN" "/RAW/NAND/$MODEL_ADDR" || exit 1
    fi
} 2>&1 | tee ./tmp/flash-log.txt | grep -a "pushed\|error" | tail -5

EXPECTED_PUSH=$((1 + 1 + FLASH_MODEL + FLASH_BOOT))
PUSHED_OK=$(grep -c "1 file pushed" ./tmp/flash-log.txt || true)
if [ "$PUSHED_OK" -lt "$EXPECTED_PUSH" ]; then
    echo -e "${RED}错误: 烧录不完整（$PUSHED_OK/$EXPECTED_PUSH），查看 ./tmp/flash-log.txt${NC}"
    exit 1
fi

# 4) 复位回业务固件
echo ">> 烧录完成，重启设备 ..."
adb_of shell reboot hard || adb_of reboot || true
echo -e "${GREEN}>> flash ok${NC}（日志: ./tmp/flash-log.txt）"
