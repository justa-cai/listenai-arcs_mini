#!/bin/bash
# =============================================================================
# auto.sh - 一键烧录脚本（可选先构建）
#
# 流程：
#   1. 发送 SIGUSR1 给 picocom 进程（释放串口 / 结束日志采集会话）
#   2. 按需构建固件（-b 开启，默认不构建）
#   3. 调用 adb_download.sh 烧录（默认只烧 app 固件）
#
# 用法：
#   ./auto.sh                      # 默认：不构建，只烧 app 固件
#   ./auto.sh -b                   # 先构建，再烧 app 固件
#   ./auto.sh full                 # 烧全部分区但不含 boot（ap/tone/wake_word/emoji/respak/app）
#   ./auto.sh all                  # 烧所有分区（含 boot，走 upgrade enter）
#   ./auto.sh -b full              # 构建 + 烧全资源分区
#   ./auto.sh -B build-h264        # 指定构建目录（默认自动探测：build，否则最新的 build-*）
#   ./auto.sh -S res/arcs-mini     # 指定资源目录（默认 res/arcs-mini）
#   ./auto.sh -h                   # 帮助
#
# 环境变量（与 adb_download.sh 一致，均可覆盖）：
#   BUILD_DIR   构建目录（默认自动探测）
#   RES_DIR     资源目录（默认 res/arcs-mini）
#   BOARD       板型（默认 arcs_mini，仅构建时使用）
#   MAX_DEVICES / RECOVERY_TIMEOUT / POLL_INTERVAL / ADB_CMD  透传给 adb_download.sh
# =============================================================================

set -u

# -----------------------------------------------------------------------------
# 终端颜色
# -----------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# -----------------------------------------------------------------------------
# 定位仓库根目录（脚本可能在子目录被调用）
# -----------------------------------------------------------------------------
REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$REPO_ROOT" || exit 1

# -----------------------------------------------------------------------------
# 参数与默认值
# -----------------------------------------------------------------------------
MODE="app"          # app=仅 app；full=除 boot 外全部；all=含 boot 全部
DO_BUILD=0          # 默认不构建，只烧录
BUILD_DIR="${BUILD_DIR:-}"
RESOURCE_DIR="${RES_DIR:-res/arcs-mini}"
BOARD="${BOARD:-arcs_mini}"

usage() {
    sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 0
}

# 模式别名归一化：all/boot → boot（adb_download 的整包模式）
normalize_mode() {
    case "$1" in
        app)            echo "app" ;;
        full|default)   echo "default" ;;
        all|boot)       echo "boot" ;;
        *)              return 1 ;;
    esac
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help|help)
            usage
            ;;
        -b|--build)
            DO_BUILD=1
            shift
            ;;
        -m|--mode)
            [ "$#" -lt 2 ] && { echo -e "${RED}错误: -m 需要 app|full|all${NC}" >&2; exit 1; }
            MODE=$(normalize_mode "$2") || { echo -e "${RED}错误: 未知模式 $2${NC}" >&2; exit 1; }
            shift 2
            ;;
        -B)
            [ "$#" -lt 2 ] && { echo -e "${RED}错误: -B 需要构建目录${NC}" >&2; exit 1; }
            BUILD_DIR="$2"
            shift 2
            ;;
        -S)
            [ "$#" -lt 2 ] && { echo -e "${RED}错误: -S 需要资源目录${NC}" >&2; exit 1; }
            RESOURCE_DIR="$2"
            shift 2
            ;;
        app|full|all|boot|default)
            MODE=$(normalize_mode "$1") || exit 1
            shift
            ;;
        *)
            echo -e "${RED}错误: 不支持的参数: $1（-h 查看用法）${NC}" >&2
            exit 1
            ;;
    esac
done

# -----------------------------------------------------------------------------
# 构建目录探测：优先 $BUILD_DIR / ./build，否则取最新修改的 build-* 目录
# -----------------------------------------------------------------------------
detect_build_dir() {
    if [ -n "$BUILD_DIR" ]; then
        return
    fi
    if [ -d build ]; then
        BUILD_DIR="build"
        return
    fi
    local newest
    newest=$(ls -dt build-*/ 2>/dev/null | head -1)
    if [ -n "$newest" ]; then
        BUILD_DIR="${newest%/}"
        echo -e "${YELLOW}提示: 未发现 ./build，自动选用最新构建目录 ${BUILD_DIR}${NC}"
    else
        BUILD_DIR="build"
    fi
}

# -----------------------------------------------------------------------------
# 第 1 步：通知 picocom（SIGUSR1），释放串口给后续烧录/日志流程
# -----------------------------------------------------------------------------
signal_picocom() {
    local pids
    pids=$(pgrep -x picocom 2>/dev/null || true)
    if [ -z "$pids" ]; then
        echo "picocom: 未发现运行中的会话，跳过"
        return
    fi
    for pid in $pids; do
        if kill -USR1 "$pid" 2>/dev/null; then
            echo "picocom: 已发送 SIGUSR1 (pid $pid)"
        fi
    done
    # 给 picocom 一点时间响应（自行退出或释放串口）
    sleep 1
    pids=$(pgrep -x picocom 2>/dev/null || true)
    if [ -n "$pids" ]; then
        echo -e "${YELLOW}警告: picocom 仍在运行 (pid $pids)，若串口烧录失败请手动处理${NC}"
    fi
}

# -----------------------------------------------------------------------------
# 第 2 步：构建
# -----------------------------------------------------------------------------
do_build() {
    echo "============================================================"
    echo "构建: apps/arcs-mini -> $BUILD_DIR (BOARD=$BOARD)"
    echo "============================================================"

    # 环境自检（检测工具链/子模块）；静默成功，失败时打印详情
    if ! source ./arcs-sdk/env.sh >/dev/null 2>&1; then
        echo -e "${YELLOW}警告: arcs-sdk/env.sh 自检未通过，尝试继续${NC}"
        ./arcs-sdk/env.sh check || true
    fi

    # 必须显式指向仓库内工具链：系统 Xuantie 工具链处理不了 SDK 内
    # 预编译库（lisa_player objcopy 报 unsupported relocation 0x3c）
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

    ./build.sh -S ./apps/arcs-mini -B "$BUILD_DIR" -DBOARD="$BOARD"
    local ret=$?
    if [ $ret -ne 0 ]; then
        echo -e "${RED}错误: 构建失败 (exit $ret)${NC}" >&2
        exit $ret
    fi
    echo -e "${GREEN}构建完成${NC}"
}

# -----------------------------------------------------------------------------
# 主流程
# -----------------------------------------------------------------------------
main() {
    detect_build_dir

    echo "============================================================"
    echo "auto.sh | 模式: $MODE | 构建目录: $BUILD_DIR | 资源目录: $RESOURCE_DIR"
    [ "$DO_BUILD" -eq 1 ] && echo "        | 构建: 是" || echo "        | 构建: 否（-b 开启）"
    echo "============================================================"

    signal_picocom

    if [ "$DO_BUILD" -eq 1 ]; then
        do_build
    fi

    # 烧录前确认产物存在（app 模式与构建后场景都适用）
    if [ "$MODE" = "app" ] && [ ! -f "$BUILD_DIR/arcs-mini.bin" ]; then
        echo -e "${RED}错误: $BUILD_DIR/arcs-mini.bin 不存在，请先构建（-b）或检查 -B 目录${NC}" >&2
        exit 1
    fi

    echo "============================================================"
    echo "烧录开始（模式: $MODE）"
    echo "============================================================"
    BUILD_DIR="$BUILD_DIR" RES_DIR="$RESOURCE_DIR" \
        bash adb_download.sh -S "$RESOURCE_DIR" -B "$BUILD_DIR" -Mode "$MODE"
    local ret=$?
    if [ $ret -ne 0 ]; then
        echo -e "${RED}烧录失败 (exit $ret)${NC}" >&2
        exit $ret
    fi
    echo -e "${GREEN}烧录完成${NC}"
}

main
