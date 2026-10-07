#!/bin/bash
set -x
# =============================================================================
# auto.sh - Lua/master 一键构建 + 烧录脚本
#
# 用法:
#   ./auto.sh              # 只烧 app 固件 (默认, 日常迭代用; 不构建)
#   ./auto.sh build        # 构建 + 只烧 app 固件
#   ./auto.sh -n           # 只构建, 不烧录 (等同 ./auto.sh build -n)
#   ./auto.sh full         # 烧 ap/tone/wake_word/emoji/respak/app (不含 boot, 不构建)
#   ./auto.sh all          # 烧全部分区含 boot (boot 改过才用; 烧错 boot 需串口救砖)
#   ./auto.sh build full   # 构建 + 烧全资源分区
#   ./auto.sh log          # 重启设备并抓 10 秒启动日志到 ./tmp/run-log.txt
#   ./auto.sh push 01          # 上传 tests/miniapp/01-snake.lua 到设备 /miniapp/ 并运行
#   ./auto.sh push tests/miniapp/06-tts.lua   # 上传指定 Lua 脚本
#
# 环境变量 (均可覆盖):
#   BOARD        板型 (默认 arcs_mini; doll_v2 用 arcs_mini_doll_v2)
#   BUILD_DIR    构建目录 (默认 build-lua, 与 nes 分支的 build-nes 惯例一致;
#                注意仓库根目录的 build/ 是 ble_peripheral 的旧缓存, 勿混用)
#   RES_DIR      资源目录 (默认 res/arcs-mini)
#   DEVICE       显式指定 adb 序列号 (log/push 用; 烧录由 adb_download.sh 自己选)
#   LOG_SECONDS  log 模式采集秒数 (默认 10)
#   DEBUG=1      打印脚本执行轨迹 (set -x)
#
# 说明:
#   - 增量构建: 本脚本从不给 build.sh 传 -C, 构建目录原样保留, 只重编改动过的文件。
#     需要全新构建时手动执行:
#       ./build.sh -S ./apps/arcs-mini -B build-lua -DBOARD=arcs_mini -C
#   - 烧录统一委派给仓库自带的 adb_download.sh (按 partition_table.json 派发分区),
#     不再自己 adb push /RAW/NAND/*。
#   - 构建必须用仓库内工具链 listenai-dev-tools/gcc, 否则 lisa_player 的 objcopy 步骤
#     会因系统 Xuantie 工具链不认 SDK 预编译库而失败 (unsupported relocation type 0x3c)。
#   - build.sh 在 CMake 失败时也可能返回 0, 本脚本用产物时间戳兜底校验。
# =============================================================================

set -u
set -o pipefail
[ -n "${DEBUG:-}" ] && set -x

# -----------------------------------------------------------------------------
# 终端颜色
# -----------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# -----------------------------------------------------------------------------
# 定位仓库根目录
# -----------------------------------------------------------------------------
REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$REPO_ROOT" || exit 1

# -----------------------------------------------------------------------------
# 默认值
# -----------------------------------------------------------------------------
BOARD="${BOARD:-arcs_mini}"
BUILD_DIR="${BUILD_DIR:-build-lua}"
RESOURCE_DIR="${RES_DIR:-res/arcs-mini}"
LOG_SECONDS="${LOG_SECONDS:-10}"
DO_BUILD=0
NO_FLASH=0          # -n: 只构建不烧录
MODE="app"          # app=仅 app; default=除 boot 外全部; boot=含 boot
PUSH_ARG=""

# -----------------------------------------------------------------------------
# 用法: 直接复用文件头注释 (打印到第一个非注释行为止, 增删注释无需改这里)
# -----------------------------------------------------------------------------
usage() {
    awk 'NR<2{next} /^#/{sub(/^# ?/,""); print; next} {exit}' "${BASH_SOURCE[0]}"
    exit 0
}

# 模式别名归一化 -> adb_download.sh 的 -Mode 取值
normalize_mode() {
    case "$1" in
        app)            echo "app" ;;
        full|default)   echo "default" ;;
        all|boot)       echo "boot" ;;
        *)              return 1 ;;
    esac
}

# -----------------------------------------------------------------------------
# 参数解析
# -----------------------------------------------------------------------------
while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help|help)
            usage
            ;;
        -b|--build)
            DO_BUILD=1
            shift
            ;;
        -n|--build-only)
            DO_BUILD=1
            NO_FLASH=1
            shift
            ;;
        -B)
            [ "$#" -lt 2 ] && { echo -e "${RED}错误: -B 需要构建目录${NC}" >&2; exit 1; }
            BUILD_DIR="$2"; shift 2
            ;;
        -S)
            [ "$#" -lt 2 ] && { echo -e "${RED}错误: -S 需要资源目录${NC}" >&2; exit 1; }
            RESOURCE_DIR="$2"; shift 2
            ;;
        -d|--device)
            [ "$#" -lt 2 ] && { echo -e "${RED}错误: -d 需要 adb 序列号${NC}" >&2; exit 1; }
            DEVICE="$2"; shift 2
            ;;
        log)
            MODE="log"; shift
            ;;
        push)
            [ "$#" -lt 2 ] && { echo -e "${RED}错误: push 需要 Lua 脚本名或路径${NC}" >&2; exit 1; }
            MODE="push"; PUSH_ARG="$2"; shift 2
            ;;
        app|full|all|boot|default)
            MODE=$(normalize_mode "$1") || { echo -e "${RED}错误: 未知模式 $1${NC}" >&2; exit 1; }
            shift
            ;;
        build)
            DO_BUILD=1; shift
            ;;
        *)
            echo -e "${RED}错误: 不支持的参数: $1 (-h 查看用法)${NC}" >&2
            exit 1
            ;;
    esac
done

# log / push 模式下 build 参数无意义, 明确忽略
case "$MODE" in
    log|push) DO_BUILD=0 ;;
esac

# -----------------------------------------------------------------------------
# 通知 picocom: 释放串口, 避免与 adb push/烧录抢设备
# -----------------------------------------------------------------------------
signal_picocom() {
    if pgrep -x picocom >/dev/null 2>&1; then
        pkill -USR1 picocom 2>/dev/null || true
        sleep 0.3
        echo "picocom: 已发送 SIGUSR1"
    fi
}

# -----------------------------------------------------------------------------
# 选定目标设备 (log / push 用; 烧录交给 adb_download.sh)
#   优先: $DEVICE > 普通模式 listenai 开发板 > 任意非 BOOT- 设备
# -----------------------------------------------------------------------------
pick_device() {
    local dev="${DEVICE:-}"
    if [ -n "$dev" ]; then
        echo "$dev"
        return 0
    fi
    adb disconnect >/dev/null 2>&1 || true
    local candidate
    candidate=$(adb devices -l | awk '$2=="device" && !/^BOOT-/ && /model:listenai/ {print $1; exit}')
    if [ -z "$candidate" ]; then
        candidate=$(adb devices | awk '$2=="device" && $1 !~ /^BOOT-/ {print $1; exit}')
    fi
    if [ -z "$candidate" ]; then
        echo -e "${RED}错误: 未发现可用设备 (用 -d <serial> 或 DEVICE= 指定)${NC}" >&2
        return 1
    fi
    echo "$candidate"
}

# -----------------------------------------------------------------------------
# 构建
# -----------------------------------------------------------------------------
do_build() {
    echo "============================================================"
    echo "构建: apps/arcs-mini -> $BUILD_DIR (BOARD=$BOARD)"
    echo "============================================================"

    # 环境自检 (工具链 / 子模块 / 串口权限)
    if ! source ./arcs-sdk/env.sh >/dev/null 2>&1; then
        echo -e "${YELLOW}警告: arcs-sdk/env.sh 自检未通过, 继续尝试${NC}"
        ./arcs-sdk/env.sh check || true
    fi

    # 必须显式指向仓库内工具链
    local tc=""
    if [ -d "$REPO_ROOT/listenai-dev-tools/gcc/bin" ]; then
        tc="$REPO_ROOT/listenai-dev-tools/gcc"
    elif [ -d "$HOME/.listenai/gcc/bin" ]; then
        tc="$HOME/.listenai/gcc"
    fi
    if [ -n "$tc" ]; then
        export NUCLEI_TOOLCHAIN_PATH="$tc"
        export PATH="$tc/bin:$PATH"
        echo "工具链: $tc"
    fi

    mkdir -p ./tmp
    local started_at
    started_at=$(date +%s)
    local build_log="./tmp/build-${BUILD_DIR}.log"

    # 不用 set -e: build.sh 失败时也可能返回 0, 需自行判定
    ./build.sh -S ./apps/arcs-mini -B "$BUILD_DIR" -DBOARD="$BOARD" 2>&1 | tee "$build_log"
    local ret=${PIPESTATUS[0]}
    if [ "$ret" -ne 0 ]; then
        echo -e "${RED}错误: 构建失败 (exit $ret), 完整输出见 $build_log${NC}" >&2
        exit "$ret"
    fi

    # 兜底: 产物必须比本次构建更晚生成, 否则视作没真正产出 (CMake 静默失败)
    local bin="$BUILD_DIR/arcs-mini.bin"
    if [ ! -f "$bin" ]; then
        echo -e "${RED}错误: 未生成 $bin (见 $build_log)${NC}" >&2
        exit 1
    fi
    if [ "$(stat -c %Y "$bin" 2>/dev/null || echo 0)" -lt "$started_at" ]; then
        echo -e "${RED}错误: $bin 未更新, 构建可能中途失败 (见 $build_log)${NC}" >&2
        exit 1
    fi

    echo -e "${GREEN}构建完成: $bin${NC}"
}

# -----------------------------------------------------------------------------
# 抓启动日志 (重启设备, 等 USB 重新枚举, 采集 LOG_SECONDS 秒)
# -----------------------------------------------------------------------------
do_log() {
    local dev
    dev=$(pick_device) || exit 1
    mkdir -p ./tmp
    local out="./tmp/run-log.txt"
    echo "==> 抓取日志: $dev -> $out (${LOG_SECONDS}s)"
    : > "$out"
    adb -s "$dev" shell reboot >/dev/null 2>&1 || true
    adb -s "$dev" wait-for-device >/dev/null 2>&1 || true
    timeout "$LOG_SECONDS" adb -s "$dev" shell > "$out" 2>&1 || true
    echo -e "${GREEN}日志已写入 $out ($(wc -l < "$out") 行)${NC}"
}

# -----------------------------------------------------------------------------
# 上传 Lua 小应用并运行: 交给 lua.sh (脚本解析/设备选择只有一份实现)
# -----------------------------------------------------------------------------
do_push() {
    exec "$REPO_ROOT/lua.sh" push "$PUSH_ARG"
}

# -----------------------------------------------------------------------------
# 烧录: 委派 adb_download.sh
# -----------------------------------------------------------------------------
do_flash() {
    local bin="$BUILD_DIR/arcs-mini.bin"
    if [ "$MODE" = "app" ] && [ ! -f "$bin" ]; then
        echo -e "${RED}错误: $bin 不存在, 先构建 (./auto.sh build)${NC}" >&2
        exit 1
    fi

    echo "============================================================"
    echo "烧录开始 (模式: $MODE, 构建目录: $BUILD_DIR)"
    echo "============================================================"

    local out
    out=$(BUILD_DIR="$BUILD_DIR" RES_DIR="$RESOURCE_DIR" \
        bash adb_download.sh -S "$RESOURCE_DIR" -B "$BUILD_DIR" -Mode "$MODE" 2>&1)
    local ret=$?
    printf '%s\n' "$out"

    if [ "$ret" -ne 0 ]; then
        # 已知误报: 数据写完 NAND 后设备不回最终应答, adb 报 "read copy response: EOF",
        # 实际已烧录成功 —— 识别后放行, 建议用 ./auto.sh log 验证新固件是否运行
        if printf '%s' "$out" | grep -q "read copy response: EOF"; then
            echo -e "${YELLOW}警告: adb 报 EOF (已知误报), 按已烧录处理; 用 ./auto.sh log 确认${NC}"
        else
            echo -e "${RED}烧录失败 (exit $ret)${NC}" >&2
            exit "$ret"
        fi
    fi
    echo -e "${GREEN}烧录完成${NC}"
}

# -----------------------------------------------------------------------------
# 主流程
# -----------------------------------------------------------------------------
main() {
    case "$MODE" in
        log)  signal_picocom; do_log; exit 0 ;;
        push) signal_picocom; do_push; exit 0 ;;
    esac

    echo "============================================================"
    echo "auto.sh | 模式: $MODE | 构建: $([ "$DO_BUILD" -eq 1 ] && echo 是 || echo 否) | 目录: $BUILD_DIR"
    echo "============================================================"

    signal_picocom
    [ "$DO_BUILD" -eq 1 ] && do_build
    if [ "$NO_FLASH" -eq 1 ]; then
        echo -e "${GREEN}已构建 (未烧录, -n)${NC}"
        exit 0
    fi
    do_flash
}

main
