#!/bin/bash

# =============================================================================
# adb_download.sh - ARCS 固件批量烧录脚本
#
# 功能：通过 ADB 将固件并发烧录到多台 ARCS 设备。
#
# 执行流程：
#   Phase 1 - 环境与 ADB 检测：解析参数、定位仓库根目录、构建烧录表、检测 ADB
#   Phase 2 - 设备发现：扫描 ADB 设备，区分普通设备与 recovery (BOOT-*) 设备
#   Phase 3 - 进入 recovery：向普通设备并发发送 recovery 命令
#   Phase 4 - 等待 BOOT 设备：轮询等待设备进入 recovery，按 USB 端口/顺序匹配
#   Phase 5 - 烧录：多设备并发推送固件
#   Phase 6 - 重启与汇总：重启已完成设备，输出每台设备的烧录结果
#
# 用法：
#   bash adb_download.sh -S res/arcs-mini          # 默认模式：烧录除 boot 外的镜像
#   bash adb_download.sh -S res/arcs-mini app      # 只烧录 app 镜像
#   bash adb_download.sh -S res/arcs-mini boot     # 完整模式：包含 boot 并执行 upgrade enter
#   bash adb_download.sh --help                    # 显示帮助信息
#
# 环境变量：
#   MAX_DEVICES      最多支持的设备数量（默认 10）
#   RECOVERY_TIMEOUT 等待设备进入 recovery 的超时秒数（默认 120）
#   POLL_INTERVAL    轮询 recovery 状态的间隔秒数（默认 2）
#   ADB_CMD          手动指定 ADB 工具路径（默认自动检测）
#   BUILD_DIR        app 固件所在构建目录（默认 build）
#   RES_DIR          资源目录（默认 res/arcs-mini）
# =============================================================================

set -u

# =============================================================================
# 终端颜色定义
# =============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# =============================================================================
# 运行参数（可通过环境变量覆盖）
# =============================================================================
MAX_DEVICES="${MAX_DEVICES:-10}"
RECOVERY_TIMEOUT="${RECOVERY_TIMEOUT:-120}"
POLL_INTERVAL="${POLL_INTERVAL:-2}"
RECOVERY_HANDSHAKE_SETTLE_SECONDS="${RECOVERY_HANDSHAKE_SETTLE_SECONDS:-4}"
BUILD_DIR="${BUILD_DIR:-build}"
RESOURCE_DIR="${RES_DIR:-res/arcs-mini}"

# =============================================================================
# 全局常量
# =============================================================================
# 使用极少出现在普通文本中的 ASCII 单元分隔符，方便在 bash 中安全拆字段
readonly DEVICE_FIELD_SEP=$'\037'
# 通过 adb shell 发送的升级准备命令，使设备进入可烧录模式
readonly UPGRADE_ENTER_CMD="root;listenai;upgrade enter"

# =============================================================================
# 全局状态变量
# =============================================================================
FLASH_MODE="default"    # default=资源+app, app=仅 app, boot=包含 boot
REPO_ROOT=""            # 仓库根目录路径（由 find_repo_root 设置）
adb_cmd=""              # 最终选定的 ADB 工具命令（由 detect_adb_tool 设置）
target_count=0          # 目标设备总数（由 categorize_connected_devices 计算）
missing_tid_count=0     # 缺少 transport_id 的设备数

# 烧录文件表（由 build_flash_table 填充）
declare -a LOCAL_FILES=()
declare -a REMOTE_PATHS=()

# 已连接的普通模式设备信息（由 categorize_connected_devices 填充）
declare -a normal_transport_ids=()
declare -a initial_boot_targets=()
declare -A transport_to_usb=()

# BOOT 设备匹配结果（由 wait_for_boot_devices 填充）
declare -a selected_boot_targets=()
declare -A selected_boot_set=()
declare -A tid_to_boot_target=()
declare -A recovery_handshake_set=()

# 扫描到的设备行缓存（由 main 中的 scan_connected_devices 填充）
declare -a scanned_devices=()


# =============================================================================
# Phase 1: 环境与 ADB 检测
# =============================================================================

# ---------------------------------------------------------------------------
# 定位仓库根目录
# 从脚本所在目录向上查找，确保在任意位置执行都能定位到资源文件。
# 找到后切换工作目录到仓库根目录。
# ---------------------------------------------------------------------------
find_repo_root() {
    local script_dir
    script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

    local search_dir="$script_dir"
    while [ "$search_dir" != "/" ]; do
        if [ -f "$search_dir/adb_download.sh" ]; then
            REPO_ROOT="$search_dir"
            break
        fi
        search_dir=$(dirname "$search_dir")
    done

    if [ -z "$REPO_ROOT" ]; then
        echo -e "${RED}错误: 无法定位仓库根目录${NC}"
        exit 1
    fi

    cd "$REPO_ROOT" || exit 1
}

# ---------------------------------------------------------------------------
# 解析命令行参数
# 支持：无参数（默认模式）、app（仅 app）、boot（完整模式）、-S 资源目录、-B 构建目录、--help（帮助）
# ---------------------------------------------------------------------------
parse_args() {
    while [ "$#" -gt 0 ]; do
        case "$1" in
            -h|--help|help)
                echo "用法: $0 [-S <res-dir>] [-B <build-dir>] [-Mode default|app|boot] [app|boot]"
                echo "  不带模式/default: 按 partition_table.json 烧录带 lpk 标签、且非 boot 的镜像"
                echo "  app:              只烧录带 lpk 标签的 app 镜像"
                echo "  boot:             烧录带 lpk 标签的所有镜像，并执行 upgrade enter"
                echo "  -S <res-dir>:     包含 partition_table.json 的资源目录，默认 res/arcs-mini"
                echo "  -B <build-dir>:   \${BUILD_DIR} 对应的构建目录，默认 build"
                exit 0
                ;;
            -S)
                if [ "$#" -lt 2 ]; then
                    echo -e "${RED}错误: -S 需要指定资源目录${NC}"
                    exit 1
                fi
                RESOURCE_DIR="$2"
                shift 2
                ;;
            -B)
                if [ "$#" -lt 2 ]; then
                    echo -e "${RED}错误: -B 需要指定构建目录${NC}"
                    exit 1
                fi
                BUILD_DIR="$2"
                shift 2
                ;;
            -Mode|--mode)
                if [ "$#" -lt 2 ]; then
                    echo -e "${RED}错误: -Mode 需要指定 default、app 或 boot${NC}"
                    exit 1
                fi
                FLASH_MODE="$2"
                shift 2
                ;;
            default|app|boot)
                FLASH_MODE="$1"
                shift
                ;;
            *)
                echo -e "${RED}错误: 不支持的参数: $1${NC}"
                echo "用法: $0 [-S <res-dir>] [-B <build-dir>] [-Mode default|app|boot] [app|boot]"
                exit 1
                ;;
        esac
    done

    case "$FLASH_MODE" in
        default|app|boot)
            ;;
        *)
            echo -e "${RED}错误: 不支持的模式: $FLASH_MODE${NC}"
            exit 1
            ;;
    esac
}

# ---------------------------------------------------------------------------
# 查找可用 Python 3，用于从 partition_table.json 读取分区映射。
# ---------------------------------------------------------------------------
find_python_cmd() {
    local candidate candidate_path
    for candidate in python3 python; do
        if ! type -P "$candidate" >/dev/null 2>&1; then
            continue
        fi

        candidate_path=$(type -P "$candidate")
        if "$candidate_path" -c 'import sys; sys.exit(0 if sys.version_info[0] >= 3 else 1)' >/dev/null 2>&1; then
            echo "$candidate_path"
            return 0
        fi
    done

    return 1
}

# ---------------------------------------------------------------------------
# 构建烧录文件表
# 根据 FLASH_MODE 决定烧录范围，分区地址始终来自 partition_table.json。
# ---------------------------------------------------------------------------
build_flash_table() {
    local resource_dir="${RESOURCE_DIR%/}"
    local partition_file="$resource_dir/partition_table.json"
    local python_cmd

    if [ ! -f "$partition_file" ]; then
        echo -e "${RED}错误: 分区表不存在: $partition_file${NC}"
        exit 1
    fi

    if ! python_cmd=$(find_python_cmd); then
        echo -e "${RED}错误: 需要 Python 3 来读取 $partition_file${NC}"
        exit 1
    fi

    case "$FLASH_MODE" in
        app)
            echo "当前模式: 仅烧录 app 固件"
            ;;
        boot)
            echo "当前模式: 包含 boot 烧录"
            ;;
        *)
            echo "当前模式: 烧录静态资源和 app 固件，不烧录 boot"
            ;;
    esac
    echo "资源目录: $resource_dir"
    echo "分区表: $partition_file"

    local line name local_path remote_path
    while IFS= read -r line; do
        IFS="$DEVICE_FIELD_SEP" read -r name local_path remote_path <<< "$line"
        if [ -z "$name" ] || [ -z "$local_path" ] || [ -z "$remote_path" ]; then
            continue
        fi

        LOCAL_FILES+=("$local_path")
        REMOTE_PATHS+=("$remote_path")
    done < <("$python_cmd" - "$partition_file" "$FLASH_MODE" "$BUILD_DIR" "$resource_dir" "$DEVICE_FIELD_SEP" <<'PY'
import json
import os
import sys

partition_file, flash_mode, build_dir, resource_dir, sep = sys.argv[1:6]

with open(partition_file, "r", encoding="utf-8") as f:
    partition_table = json.load(f)

base_dir = os.path.dirname(partition_file)

for image in partition_table.get("images", []):
    name = image.get("name")
    if not name or not image.get("file") or not image.get("addr"):
        continue
    if "lpk" not in image.get("tags", []):
        continue

    if flash_mode == "app" and name != "app":
        continue
    if name == "boot" and flash_mode != "boot":
        continue

    addr = str(image["addr"])
    if addr.lower().startswith("0x"):
        addr = addr[2:]
    remote_path = "/RAW/NAND/" + (addr.lstrip("0") or "0")

    raw_file = str(image["file"])
    uses_build_dir = "${BUILD_DIR}" in raw_file
    local_path = raw_file.replace("${BUILD_DIR}", build_dir).replace("${RES_DIR}", resource_dir)
    if not os.path.isabs(local_path) and not uses_build_dir:
        local_path = os.path.normpath(os.path.join(base_dir, local_path))

    print(sep.join([name, local_path.replace("\\", "/"), remote_path]))
PY
    )

    if [ "${#LOCAL_FILES[@]}" -eq 0 ]; then
        echo -e "${RED}错误: 没有待烧录文件，请检查 $partition_file / FLASH_MODE 配置${NC}"
        exit 1
    fi

    echo "本次待烧录文件:"
    for idx in "${!LOCAL_FILES[@]}"; do
        echo "- ${LOCAL_FILES[$idx]} -> ${REMOTE_PATHS[$idx]}"
    done
}

# ---------------------------------------------------------------------------
# 验证待烧录的本地文件是否存在
# ---------------------------------------------------------------------------
verify_local_files() {
    echo "检查待烧录文件..."
    local local_path
    for local_path in "${LOCAL_FILES[@]}"; do
        if [ ! -f "$local_path" ]; then
            echo -e "${RED}错误: 本地文件不存在: $local_path${NC}"
            exit 1
        fi
    done
    echo -e "${GREEN}文件检查完成${NC}"
}

# ---------------------------------------------------------------------------
# 检测并选择最优的 ADB 工具
# 支持：
#   - 环境变量 ADB_CMD 指定的路径
#   - PATH 中的 adb
#   - WSL 下使用 powershell 查找 Windows 侧的 adb
#   - 兜底使用 "adb" 命令名
# 优先选用当前能看到最多设备的实例。
# ---------------------------------------------------------------------------
detect_adb_tool() {
    local is_wsl=0
    declare -a adb_candidates=()
    declare -A seen_adb_candidates=()

    # 辅助函数：去重添加候选 ADB 路径
    add_candidate() {
        local candidate="$1"
        [ -z "$candidate" ] && return
        if [ -z "${seen_adb_candidates[$candidate]:-}" ]; then
            adb_candidates+=("$candidate")
            seen_adb_candidates["$candidate"]=1
        fi
    }

    # 收集候选 ADB 路径
    if [ -n "${ADB_CMD:-}" ]; then
        add_candidate "$ADB_CMD"
    fi

    if [ -n "$REPO_ROOT" ]; then
        add_candidate "$REPO_ROOT/tools/adb/adb"
        add_candidate "$REPO_ROOT/tools/adb/adb.exe"
    fi

    if type -P adb >/dev/null 2>&1; then
        add_candidate "$(type -P adb)"
    fi

    # WSL 环境：尝试复用 Windows 侧的 adb
    if uname -a | grep -qi "WSL"; then
        is_wsl=1
        echo "检测到WSL环境"
        if type -P powershell.exe >/dev/null 2>&1; then
            local adb_win_path
            adb_win_path=$(powershell.exe -Command "(Get-Command adb).Source" 2>/dev/null | tr -d '\r')
            local adb_win_path_wsl
            adb_win_path_wsl=$(echo "$adb_win_path" | sed -e 's/\\/\//g' -e 's/^\(.\):/\/mnt\/\L\1/')
            add_candidate "$adb_win_path_wsl"
        fi
    fi

    add_candidate "adb"

    # 逐个探测 adb 候选，选择能看到最多设备的实例
    local best_adb_cmd=""
    local best_adb_count=-1
    local candidate candidate_count
    for candidate in "${adb_candidates[@]}"; do
        if ! "$candidate" version >/dev/null 2>&1; then
            continue
        fi
        candidate_count=$("$candidate" devices -l 2>/dev/null | tr -d '\r' | awk 'NR > 1 && $2 == "device" {c++} END {print c + 0}')
        echo "ADB候选: $candidate (检测到设备: $candidate_count)"
        if [ "$candidate_count" -gt "$best_adb_count" ]; then
            best_adb_cmd="$candidate"
            best_adb_count="$candidate_count"
        fi
    done

    if [ -z "$best_adb_cmd" ]; then
        echo -e "${RED}错误: ADB工具不可用, 请确保已安装 Android SDK Platform-Tools 并将其添加到环境变量中${NC}"
        exit 1
    fi

    adb_cmd="$best_adb_cmd"
    echo "选择ADB工具: $adb_cmd (当前可见设备: $best_adb_count)"
    "$adb_cmd" version
}


# =============================================================================
# Phase 2: 设备发现
# =============================================================================

# ---------------------------------------------------------------------------
# 获取 ADB 设备列表原始输出
# 统一收敛输出，去掉回车和空字符，降低不同平台下的解析差异。
# ---------------------------------------------------------------------------
collect_devices_raw() {
    "$adb_cmd" devices -l 2>/dev/null | tr -d '\r\000'
}

# ---------------------------------------------------------------------------
# 扫描已连接的 ADB 设备
# 从 `adb devices -l` 输出中提取 serial / usb / transport_id 三个字段，
# 以 DEVICE_FIELD_SEP 分隔输出，每行一台设备。
# ---------------------------------------------------------------------------
scan_connected_devices() {
    local line serial state usb tid

    while IFS= read -r line; do
        [ -z "$line" ] && continue
        [[ "$line" == "List of devices attached"* ]] && continue

        if [[ ! "$line" =~ ^([^[:space:]]+)[[:space:]]+([^[:space:]]+) ]]; then
            continue
        fi

        serial="${BASH_REMATCH[1]}"
        state="${BASH_REMATCH[2]}"
        if [ "$state" != "device" ]; then
            continue
        fi

        usb=""
        tid=""

        if [[ "$line" =~ usb:([^[:space:]]+) ]]; then
            usb="${BASH_REMATCH[1]}"
        fi
        if [[ "$line" =~ transport_id:([^[:space:]]+) ]]; then
            tid="${BASH_REMATCH[1]}"
        fi

        printf "%s%s%s%s%s\n" "$serial" "$DEVICE_FIELD_SEP" "$usb" "$DEVICE_FIELD_SEP" "$tid"
    done < <(collect_devices_raw)
}

# ---------------------------------------------------------------------------
# BOOT 设备在量产时可能出现相同 serial，优先使用 transport_id 区分。
# target 结构：serial <sep> usb <sep> transport_id <sep> key。
# ---------------------------------------------------------------------------
make_boot_target_key() {
    local serial="$1"
    local tid="$2"

    if [ -n "$tid" ]; then
        printf "tid:%s" "$tid"
    else
        printf "serial:%s" "$serial"
    fi
}

make_boot_target() {
    local serial="$1"
    local usb="$2"
    local tid="$3"
    local key

    key=$(make_boot_target_key "$serial" "$tid")
    printf "%s%s%s%s%s%s%s\n" "$serial" "$DEVICE_FIELD_SEP" "$usb" "$DEVICE_FIELD_SEP" "$tid" "$DEVICE_FIELD_SEP" "$key"
}

boot_target_display() {
    local target="$1"
    local serial usb tid key

    IFS="$DEVICE_FIELD_SEP" read -r serial usb tid key <<< "$target"
    if [ -n "$tid" ]; then
        printf "%s (transport_id:%s)" "$serial" "$tid"
    else
        printf "%s" "$serial"
    fi
}

# ---------------------------------------------------------------------------
# 对扫描结果进行分类
# - 普通模式设备（有 transport_id）：归入 normal_transport_ids
# - 已在 recovery 的设备（序列号以 BOOT- 开头）：归入 initial_boot_targets
# - 缺少 transport_id 的设备：记录警告计数，忽略
# 同时校验设备数量不超过 MAX_DEVICES。
# ---------------------------------------------------------------------------
categorize_connected_devices() {
    # 临时去重表，仅在本函数内使用
    declare -A seen_tid=()
    declare -A seen_boot=()

    missing_tid_count=0

    local scanned_line serial usb tid
    for scanned_line in "${scanned_devices[@]}"; do
        IFS="$DEVICE_FIELD_SEP" read -r serial usb tid <<< "$scanned_line"
        [ -z "$serial" ] && continue

        # 已在 recovery 模式的设备（BOOT-* 序列号）
        if [[ "$serial" == BOOT-* ]]; then
            local boot_key
            boot_key=$(make_boot_target_key "$serial" "$tid")
            if [ -z "${seen_boot[$boot_key]:-}" ]; then
                initial_boot_targets+=("$(make_boot_target "$serial" "$usb" "$tid")")
                seen_boot["$boot_key"]=1
            fi
            continue
        fi

        # 缺少 transport_id 的普通设备无法可靠通信，记录后忽略
        if [ -z "$tid" ]; then
            missing_tid_count=$((missing_tid_count + 1))
            continue
        fi

        if [ -z "${seen_tid[$tid]:-}" ]; then
            normal_transport_ids+=("$tid")
            transport_to_usb["$tid"]="$usb"
            seen_tid["$tid"]=1
        fi
    done

    target_count=$(( ${#normal_transport_ids[@]} + ${#initial_boot_targets[@]} ))

    # 无可用设备
    if [ "$target_count" -eq 0 ]; then
        echo -e "${RED}错误: 没有检测到可用设备(状态必须是 device)${NC}"
        echo "当前 \"${adb_cmd} devices -l\" 输出:"
        "$adb_cmd" devices -l
        echo "脚本解析后的设备行(serial | usb | transport_id):"
        if [ "${#scanned_devices[@]}" -eq 0 ]; then
            echo "(empty)"
        else
            printf '%s\n' "${scanned_devices[@]}" | sed $'s/\x1f/ | /g'
        fi
        exit 1
    fi

    # 设备数量超限
    if [ "$target_count" -gt "$MAX_DEVICES" ]; then
        echo -e "${RED}错误: 检测到 $target_count 台设备，超过 MAX_DEVICES=$MAX_DEVICES${NC}"
        echo "如需继续请设置更大值，例如: MAX_DEVICES=$target_count bash adb_download.sh -S $RESOURCE_DIR"
        exit 1
    fi

    echo "检测到设备总数: $target_count"
    echo "- 普通模式设备(需进recovery): ${#normal_transport_ids[@]}"
    echo "- 已在recovery(BOOT-*)设备: ${#initial_boot_targets[@]}"
    if [ "$missing_tid_count" -gt 0 ]; then
        echo -e "${YELLOW}警告: 有 $missing_tid_count 台普通模式设备缺少 transport_id，已忽略${NC}"
    fi
}


# =============================================================================
# Phase 3: 进入 recovery
# =============================================================================

# ---------------------------------------------------------------------------
# 向所有普通模式设备并发发送进入 recovery 的命令
# 优先尝试 `adb reboot recovery`，失败时回退到 `adb shell recovery`。
# ---------------------------------------------------------------------------
send_recovery_commands() {
    if [ "${#normal_transport_ids[@]}" -eq 0 ]; then
        return
    fi

    echo "发送进入recovery命令..."
    declare -A reboot_pid_to_tid=()

    local tid pid
    for tid in "${normal_transport_ids[@]}"; do
        (
            if "$adb_cmd" -t "$tid" reboot recovery >/dev/null 2>&1; then
                echo "[transport_id:$tid] 已发送: reboot recovery"
                exit 0
            fi

            if "$adb_cmd" -t "$tid" shell recovery >/dev/null 2>&1; then
                echo "[transport_id:$tid] 已发送: shell recovery"
                exit 0
            fi

            echo "[transport_id:$tid] 进入recovery失败" >&2
            exit 1
        ) &
        pid=$!
        reboot_pid_to_tid["$pid"]="$tid"
    done

    local reboot_failures=0
    for pid in "${!reboot_pid_to_tid[@]}"; do
        if ! wait "$pid"; then
            reboot_failures=$((reboot_failures + 1))
        fi
    done

    if [ "$reboot_failures" -gt 0 ]; then
        echo -e "${RED}错误: 有 $reboot_failures 台设备发送recovery命令失败${NC}"
        exit 1
    fi
}


# =============================================================================
# Phase 4: 等待 BOOT 设备
# =============================================================================

# ---------------------------------------------------------------------------
# 将 BOOT 设备加入选中列表（自动去重）。
# transport_id 存在时用它作为选择器，避免多台设备 BOOT serial 相同时误烧。
# ---------------------------------------------------------------------------
add_selected_boot_target() {
    local target="$1"
    local serial usb tid key

    IFS="$DEVICE_FIELD_SEP" read -r serial usb tid key <<< "$target"
    if [ -z "${selected_boot_set[$key]:-}" ]; then
        selected_boot_targets+=("$target")
        selected_boot_set["$key"]=1
    fi
}

recovery_compatibility_handshake() {
    local target="$1"
    local serial usb tid key device_id

    IFS="$DEVICE_FIELD_SEP" read -r serial usb tid key <<< "$target"
    [[ "$serial" == BOOT-* ]] || return 1

    if [ -n "${recovery_handshake_set[$key]:-}" ]; then
        return 0
    fi

    device_id="${serial#BOOT-}"
    [ -n "$device_id" ] || return 1

    if ! adb_for_target "$serial" "$tid" shell "$device_id" >/dev/null 2>&1; then
        return 1
    fi

    # Old boot clears the retry marker from a three-second timer callback.
    sleep "$RECOVERY_HANDSHAKE_SETTLE_SECONDS"
    if ! adb_for_target "$serial" "$tid" get-state >/dev/null 2>&1; then
        return 1
    fi

    recovery_handshake_set["$key"]=1
    echo "[$(boot_target_display "$target")] recovery compatibility handshake complete"
}

# ---------------------------------------------------------------------------
# 轮询等待所有设备进入 recovery 并识别 BOOT 序列号
# 匹配策略：
#   1. USB 端口匹配：如果进入 recovery 前后的 USB 端口一致，直接关联
#   2. 顺序回退匹配：未匹配的设备按顺序分配给未分配的 BOOT 设备
# 超时由 RECOVERY_TIMEOUT 控制。
# ---------------------------------------------------------------------------
wait_for_boot_devices() {
    # 先计入初始就已处于 recovery 的设备
    local boot_target
    for boot_target in "${initial_boot_targets[@]}"; do
        if recovery_compatibility_handshake "$boot_target"; then
            add_selected_boot_target "$boot_target"
        fi
    done

    echo "等待设备进入recovery并识别BOOT序列号..."
    local deadline=$((SECONDS + RECOVERY_TIMEOUT))

    while [ "$SECONDS" -lt "$deadline" ]; do
        # 重新扫描当前设备列表
        local -a scan_lines=()
        mapfile -t scan_lines < <(scan_connected_devices)

        local -a scan_boot_targets=()
        local -A scan_boot_by_usb=()

        local line serial usb tid
        for line in "${scan_lines[@]}"; do
            IFS="$DEVICE_FIELD_SEP" read -r serial usb tid <<< "$line"
            if [[ "$serial" == BOOT-* ]]; then
                local current_boot_target
                current_boot_target=$(make_boot_target "$serial" "$usb" "$tid")
                if ! recovery_compatibility_handshake "$current_boot_target"; then
                    continue
                fi
                scan_boot_targets+=("$current_boot_target")
                if [ -n "$usb" ]; then
                    scan_boot_by_usb["$usb"]="$current_boot_target"
                fi
            fi
        done

        # 第一轮匹配：按 USB 端口将普通设备与新出现的 BOOT 序列号关联
        for tid in "${normal_transport_ids[@]}"; do
            if [ -n "${tid_to_boot_target[$tid]:-}" ]; then
                continue
            fi

            local dev_usb="${transport_to_usb[$tid]:-}"
            if [ -n "$dev_usb" ] && [ -n "${scan_boot_by_usb[$dev_usb]:-}" ]; then
                local matched_boot_target matched_serial matched_usb matched_tid matched_key
                matched_boot_target="${scan_boot_by_usb[$dev_usb]}"
                IFS="$DEVICE_FIELD_SEP" read -r matched_serial matched_usb matched_tid matched_key <<< "$matched_boot_target"
                if [ -n "${selected_boot_set[$matched_key]:-}" ]; then
                    continue
                fi

                tid_to_boot_target["$tid"]="$matched_boot_target"
                add_selected_boot_target "$matched_boot_target"
            fi
        done

        # 第二轮匹配：回退到顺序匹配（未分配的设备按顺序配对未分配的 BOOT）
        for tid in "${normal_transport_ids[@]}"; do
            if [ -n "${tid_to_boot_target[$tid]:-}" ]; then
                continue
            fi

            for boot_target in "${scan_boot_targets[@]}"; do
                local target_serial target_usb target_tid target_key
                IFS="$DEVICE_FIELD_SEP" read -r target_serial target_usb target_tid target_key <<< "$boot_target"
                if [ -z "${selected_boot_set[$target_key]:-}" ]; then
                    tid_to_boot_target["$tid"]="$boot_target"
                    add_selected_boot_target "$boot_target"
                    break
                fi
            done
        done

        # 所有设备已匹配，退出等待
        if [ "${#selected_boot_targets[@]}" -ge "$target_count" ]; then
            break
        fi

        local remaining=$((target_count - ${#selected_boot_targets[@]}))
        echo "仍在等待 $remaining 台设备进入recovery..."
        sleep "$POLL_INTERVAL"
    done

    # 超时检查
    if [ "${#selected_boot_targets[@]}" -lt "$target_count" ]; then
        echo -e "${RED}错误: recovery设备数量不足，期望 $target_count，实际 ${#selected_boot_targets[@]}${NC}"
        echo "当前识别到的BOOT设备:"
        for boot_target in "${selected_boot_targets[@]}"; do
            echo "- $(boot_target_display "$boot_target")"
        done
        exit 1
    fi

    echo "本次烧录目标设备:"
    for boot_target in "${selected_boot_targets[@]}"; do
        echo "- $(boot_target_display "$boot_target")"
    done
}


# =============================================================================
# Phase 5: 烧录 / Phase 6: 重启与汇总
# =============================================================================

# ---------------------------------------------------------------------------
# 针对单个目标设备执行 adb 命令。
# BOOT serial 可能重复，优先使用 transport_id 选择设备。
# ---------------------------------------------------------------------------
adb_for_target() {
    local serial="$1"
    local tid="$2"
    shift 2

    if [ -n "$tid" ]; then
        "$adb_cmd" -t "$tid" "$@"
    else
        "$adb_cmd" -s "$serial" "$@"
    fi
}

# ---------------------------------------------------------------------------
# 单台设备的完整烧录流程
# 1. (boot 模式) 发送升级准备命令
# 2. 按顺序推送所有文件到对应分区
# 3. 重启设备
# 返回值：0=成功，1=升级/push 失败，2=文件已推送但重启失败
# ---------------------------------------------------------------------------
flash_one_device() {
    local target="$1"
    local serial usb tid key display
    local idx local_path remote_path

    IFS="$DEVICE_FIELD_SEP" read -r serial usb tid key <<< "$target"
    display=$(boot_target_display "$target")

    # boot 模式：先发送升级准备命令使设备进入可烧录状态
    if [ "$FLASH_MODE" = "boot" ]; then
        echo "[$display] 执行升级准备命令"
        if ! adb_for_target "$serial" "$tid" shell "$UPGRADE_ENTER_CMD" >/dev/null; then
            echo "[$display] 执行升级准备命令失败" >&2
            return 1
        fi
    fi

    # 逐个推送固件文件
    for idx in "${!LOCAL_FILES[@]}"; do
        local_path="${LOCAL_FILES[$idx]}"
        remote_path="${REMOTE_PATHS[$idx]}"

        echo "[$display] push $local_path -> $remote_path"
        if ! adb_for_target "$serial" "$tid" push "$local_path" "$remote_path" >/dev/null; then
            echo "[$display] push失败: $local_path" >&2
            return 1
        fi
    done

    # 烧录完成，尝试重启设备
    if ! adb_for_target "$serial" "$tid" shell recovery exit >/dev/null 2>&1; then
        echo "[$display] warning: unable to clear recovery request before reboot" >&2
    fi

    if adb_for_target "$serial" "$tid" shell reboot hard >/dev/null 2>&1; then
        echo "[$display] 烧录完成，设备正在重启"
        return 0
    fi

    if adb_for_target "$serial" "$tid" reboot >/dev/null 2>&1; then
        echo "[$display] 烧录完成，已执行adb reboot"
        return 0
    fi

    echo "[$display] 文件已push，但重启命令失败" >&2
    return 2
}

# ---------------------------------------------------------------------------
# 多设备并发烧录
# 对所有选中的 BOOT 设备同时执行 flash_one_device，
# 最后汇总输出每台设备的烧录结果。
# ---------------------------------------------------------------------------
flash_all_devices() {
    # =========================================================================
    # Phase 5: 并发烧录
    # =========================================================================
    echo "开始并发烧录..."
    declare -A flash_pid_to_display=()

    local target display pid
    for target in "${selected_boot_targets[@]}"; do
        display=$(boot_target_display "$target")
        flash_one_device "$target" &
        pid=$!
        flash_pid_to_display["$pid"]="$display"
    done

    # =========================================================================
    # Phase 6: 重启与汇总
    # =========================================================================
    local -a success_serials=()
    local -a failed_serials=()

    for pid in "${!flash_pid_to_display[@]}"; do
        display="${flash_pid_to_display[$pid]}"
        if wait "$pid"; then
            success_serials+=("$display")
        else
            failed_serials+=("$display")
        fi
    done

    echo
    echo "烧录结果汇总:"
    echo "- 成功: ${#success_serials[@]} 台"
    for display in "${success_serials[@]}"; do
        echo "  [OK] $display"
    done

    echo "- 失败: ${#failed_serials[@]} 台"
    for display in "${failed_serials[@]}"; do
        echo "  [FAIL] $display"
    done

    if [ "${#failed_serials[@]}" -gt 0 ]; then
        echo -e "${RED}存在失败设备，请检查USB连接、授权弹窗和设备日志${NC}"
        exit 1
    fi

    echo -e "${GREEN}全部设备烧录完成${NC}"
}

# =============================================================================
# 主流程入口
# =============================================================================
main() {
    # =========================================================================
    # Phase 1: 环境与 ADB 检测
    # =========================================================================
    find_repo_root
    parse_args "$@"
    build_flash_table
    verify_local_files
    detect_adb_tool

    # =========================================================================
    # Phase 2: 设备发现
    # =========================================================================
    mapfile -t scanned_devices < <(scan_connected_devices)
    categorize_connected_devices

    # =========================================================================
    # Phase 3: 进入 recovery
    # =========================================================================
    send_recovery_commands

    # =========================================================================
    # Phase 4: 等待 BOOT 设备
    # =========================================================================
    wait_for_boot_devices

    # =========================================================================
    # Phase 5: 烧录
    # Phase 6: 重启与汇总
    # =========================================================================
    flash_all_devices
}

main "$@"
