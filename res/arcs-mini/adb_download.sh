#!/bin/bash

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 可通过环境变量覆盖的运行参数。
MAX_DEVICES="${MAX_DEVICES:-10}"
RECOVERY_TIMEOUT="${RECOVERY_TIMEOUT:-120}"
POLL_INTERVAL="${POLL_INTERVAL:-2}"
# 使用一个极少出现在普通文本中的分隔符，方便在 bash 中安全拆字段。
DEVICE_FIELD_SEP=$'\037'

# 从脚本所在目录向上查找仓库根目录，确保在任意位置执行都能定位到资源文件。
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=""
search_dir="$SCRIPT_DIR"
while [ "$search_dir" != "/" ]; do
    if [ -f "$search_dir/res/arcs-mini/adb_download.sh" ]; then
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

# 用法：
#   ./adb_download.sh
#       默认不烧录 boot，也不会执行 `upgrade enter`
#   ./adb_download.sh boot
#       会额外烧录 boot，并在 push 前执行 `root;listenai;upgrade enter`
INCLUDE_BOOT=0
if [ "$#" -gt 1 ]; then
    echo -e "${RED}错误: 参数过多${NC}"
    echo "用法: $0 [boot]"
    exit 1
fi

case "${1:-}" in
    "")
        ;;
    boot)
        INCLUDE_BOOT=1
        ;;
    -h|--help|help)
        echo "用法: $0 [boot]"
        echo "  不带参数: 烧录 ap/tone/wake_word/emoji/respak/app，不烧录 boot"
        echo "  boot:     额外烧录 boot，并执行 upgrade enter"
        exit 0
        ;;
    *)
        echo -e "${RED}错误: 不支持的参数: $1${NC}"
        echo "用法: $0 [boot]"
        exit 1
        ;;
esac

# 完整烧录表。默认会跳过第 1 项 boot；传入 `boot` 参数时才会包含它。
ALL_LOCAL_FILES=(
    "res/arcs-mini/boot.bin"
    "res/arcs-mini/ap.bin"
    "res/arcs-mini/tone.bin"
    "res/arcs-mini/wake_word.bin"
    "res/arcs-mini/emoji.bin"
    "res/arcs-mini/respak.bin"
    "build/arcs-mini.bin"
)

ALL_REMOTE_PATHS=(
    "/RAW/NAND/0"
    "/RAW/NAND/40000"
    "/RAW/NAND/100000"
    "/RAW/NAND/200000"
    "/RAW/NAND/380000"
    "/RAW/NAND/440000"
    "/RAW/NAND/600000"
)

if [ "${#ALL_LOCAL_FILES[@]}" -ne "${#ALL_REMOTE_PATHS[@]}" ]; then
    echo -e "${RED}错误: 本地文件和目标地址数量不一致${NC}"
    exit 1
fi

declare -a LOCAL_FILES=()
declare -a REMOTE_PATHS=()

# 默认只在烧录表第 0 项确实是 boot.bin 时跳过它。
start_idx=0
if [ "$INCLUDE_BOOT" -eq 1 ]; then
    echo "当前模式: 包含 boot 烧录"
elif [ "${#ALL_LOCAL_FILES[@]}" -gt 0 ] && [[ "${ALL_LOCAL_FILES[0]}" == *"/boot.bin" ]]; then
    start_idx=1
fi

for idx in "${!ALL_LOCAL_FILES[@]}"; do
    if [ "$idx" -lt "$start_idx" ]; then
        continue
    fi
    LOCAL_FILES+=("${ALL_LOCAL_FILES[$idx]}")
    REMOTE_PATHS+=("${ALL_REMOTE_PATHS[$idx]}")
done

if [ "${#LOCAL_FILES[@]}" -eq 0 ]; then
    echo -e "${RED}错误: 没有待烧录文件，请检查 ALL_LOCAL_FILES / INCLUDE_BOOT 配置${NC}"
    exit 1
fi

echo "本次待烧录文件:"
for idx in "${!LOCAL_FILES[@]}"; do
    echo "- ${LOCAL_FILES[$idx]} -> ${REMOTE_PATHS[$idx]}"
done

echo "检查待烧录文件..."
for local_path in "${LOCAL_FILES[@]}"; do
    if [ ! -f "$local_path" ]; then
        echo -e "${RED}错误: 本地文件不存在: $local_path${NC}"
        exit 1
    fi
done
echo -e "${GREEN}文件检查完成${NC}"

# 先识别当前是否运行在 WSL 中，后续会尝试复用 Windows 侧的 adb。
adb_cmd=""
is_wsl=0
if uname -a | grep -qi "WSL"; then
    is_wsl=1
    echo "检测到WSL环境"
fi

declare -a adb_candidates=()
declare -A seen_adb_candidates=()

# 收集可用的 adb 候选路径，并做去重。
add_adb_candidate() {
    local candidate="$1"
    [ -z "$candidate" ] && return
    if [ -z "${seen_adb_candidates[$candidate]:-}" ]; then
        adb_candidates+=("$candidate")
        seen_adb_candidates["$candidate"]=1
    fi
}

if [ -n "${ADB_CMD:-}" ]; then
    add_adb_candidate "$ADB_CMD"
fi

if type -P adb >/dev/null 2>&1; then
    add_adb_candidate "$(type -P adb)"
fi

if [ "$is_wsl" -eq 1 ] && type -P powershell.exe >/dev/null 2>&1; then
    adb_win_path=$(powershell.exe -Command "(Get-Command adb).Source" 2>/dev/null | tr -d '\r')
    adb_win_path_wsl=$(echo "$adb_win_path" | sed -e 's/\\/\//g' -e 's/^\(.\):/\/mnt\/\L\1/')
    add_adb_candidate "$adb_win_path_wsl"
fi

add_adb_candidate "adb"

# 逐个探测 adb，优先选择当前能看到更多设备的那个实例。
best_adb_cmd=""
best_adb_count=-1
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

# 统一收敛 adb 输出，去掉回车和空字符，降低不同平台下的解析差异。
collect_devices_raw() {
    "$adb_cmd" devices -l 2>/dev/null | tr -d '\r\000'
}

# 从 `adb devices -l` 中提取 serial / usb / transport_id 三个字段。
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

# 普通模式设备依赖 transport_id 发送 recovery 指令；
# 已经处于 recovery 的设备通常以 BOOT-* 形式出现。
declare -a normal_transport_ids=()
declare -a initial_boot_serials=()
declare -A transport_to_usb=()
declare -A seen_tid=()
declare -A seen_boot=()
missing_tid_count=0
mapfile -t scanned_devices < <(scan_connected_devices)

for scanned_line in "${scanned_devices[@]}"; do
    IFS="$DEVICE_FIELD_SEP" read -r serial usb tid <<< "$scanned_line"
    [ -z "$serial" ] && continue

    if [[ "$serial" == BOOT-* ]]; then
        if [ -z "${seen_boot[$serial]:-}" ]; then
            initial_boot_serials+=("$serial")
            seen_boot["$serial"]=1
        fi
        continue
    fi

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

target_count=$(( ${#normal_transport_ids[@]} + ${#initial_boot_serials[@]} ))

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

if [ "$target_count" -gt "$MAX_DEVICES" ]; then
    echo -e "${RED}错误: 检测到 $target_count 台设备，超过 MAX_DEVICES=$MAX_DEVICES${NC}"
    echo "如需继续请设置更大值，例如: MAX_DEVICES=$target_count ./z_thw_temp/download_multi_transport.sh"
    exit 1
fi

echo "检测到设备总数: $target_count"
echo "- 普通模式设备(需进recovery): ${#normal_transport_ids[@]}"
echo "- 已在recovery(BOOT-*)设备: ${#initial_boot_serials[@]}"
if [ "$missing_tid_count" -gt 0 ]; then
    echo -e "${YELLOW}警告: 有 $missing_tid_count 台普通模式设备缺少 transport_id，已忽略${NC}"
fi

# 对普通模式设备并发发送进入 recovery 的命令，尽量减少多机等待时间。
if [ "${#normal_transport_ids[@]}" -gt 0 ]; then
    echo "发送进入recovery命令..."
    declare -A reboot_pid_to_tid=()

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

    reboot_failures=0
    for pid in "${!reboot_pid_to_tid[@]}"; do
        if ! wait "$pid"; then
            reboot_failures=$((reboot_failures + 1))
        fi
    done

    if [ "$reboot_failures" -gt 0 ]; then
        echo -e "${RED}错误: 有 $reboot_failures 台设备发送recovery命令失败${NC}"
        exit 1
    fi
fi

declare -a selected_boot_serials=()
declare -A selected_boot_set=()
declare -A tid_to_boot_serial=()

# 记录本次要参与烧录的 BOOT 设备，避免重复加入。
add_selected_boot() {
    local serial="$1"
    if [ -z "${selected_boot_set[$serial]:-}" ]; then
        selected_boot_serials+=("$serial")
        selected_boot_set["$serial"]=1
    fi
}

for boot_serial in "${initial_boot_serials[@]}"; do
    add_selected_boot "$boot_serial"
done

# recovery 之后 serial 可能变化，优先按 USB 端口把普通模式设备和 BOOT 设备重新对应起来；
# 如果拿不到 USB 信息，再回退到“未分配的 BOOT 设备顺序匹配”。
echo "等待设备进入recovery并识别BOOT序列号..."
deadline=$((SECONDS + RECOVERY_TIMEOUT))
while [ "$SECONDS" -lt "$deadline" ]; do
    mapfile -t scan_lines < <(scan_connected_devices)
    declare -a scan_boot_serials=()
    declare -A scan_boot_by_usb=()

    for line in "${scan_lines[@]}"; do
        IFS="$DEVICE_FIELD_SEP" read -r serial usb tid <<< "$line"
        if [[ "$serial" == BOOT-* ]]; then
            scan_boot_serials+=("$serial")
            if [ -n "$usb" ]; then
                scan_boot_by_usb["$usb"]="$serial"
            fi
        fi
    done

    for tid in "${normal_transport_ids[@]}"; do
        if [ -n "${tid_to_boot_serial[$tid]:-}" ]; then
            continue
        fi

        usb="${transport_to_usb[$tid]:-}"
        if [ -n "$usb" ] && [ -n "${scan_boot_by_usb[$usb]:-}" ]; then
            boot_serial="${scan_boot_by_usb[$usb]}"
            tid_to_boot_serial["$tid"]="$boot_serial"
            add_selected_boot "$boot_serial"
        fi
    done

    for tid in "${normal_transport_ids[@]}"; do
        if [ -n "${tid_to_boot_serial[$tid]:-}" ]; then
            continue
        fi

        for boot_serial in "${scan_boot_serials[@]}"; do
            if [ -z "${selected_boot_set[$boot_serial]:-}" ]; then
                tid_to_boot_serial["$tid"]="$boot_serial"
                add_selected_boot "$boot_serial"
                break
            fi
        done
    done

    if [ "${#selected_boot_serials[@]}" -ge "$target_count" ]; then
        break
    fi

    remaining=$((target_count - ${#selected_boot_serials[@]}))
    echo "仍在等待 $remaining 台设备进入recovery..."
    sleep "$POLL_INTERVAL"
done

if [ "${#selected_boot_serials[@]}" -lt "$target_count" ]; then
    echo -e "${RED}错误: recovery设备数量不足，期望 $target_count，实际 ${#selected_boot_serials[@]}${NC}"
    echo "当前识别到的BOOT设备:"
    for serial in "${selected_boot_serials[@]}"; do
        echo "- $serial"
    done
    exit 1
fi

echo "本次烧录目标设备:"
for serial in "${selected_boot_serials[@]}"; do
    echo "- $serial"
done

# 单台设备的完整烧录流程：进入升级态、按顺序 push 文件、最后重启。
flash_one_device() {
    local serial="$1"
    local idx local_path remote_path

    if [ "$INCLUDE_BOOT" -eq 1 ]; then
        echo "[$serial] 执行升级准备命令"
        if ! "$adb_cmd" -s "$serial" shell "root;listenai;upgrade enter" >/dev/null; then
            echo "[$serial] 执行升级准备命令失败" >&2
            return 1
        fi
    fi

    for idx in "${!LOCAL_FILES[@]}"; do
        local_path="${LOCAL_FILES[$idx]}"
        remote_path="${REMOTE_PATHS[$idx]}"

        echo "[$serial] push $local_path -> $remote_path"
        if ! "$adb_cmd" -s "$serial" push "$local_path" "$remote_path" >/dev/null; then
            echo "[$serial] push失败: $local_path" >&2
            return 1
        fi
    done

    if "$adb_cmd" -s "$serial" shell reboot hard >/dev/null 2>&1; then
        echo "[$serial] 烧录完成，设备正在重启"
        return 0
    fi

    if "$adb_cmd" -s "$serial" reboot >/dev/null 2>&1; then
        echo "[$serial] 烧录完成，已执行adb reboot"
        return 0
    fi

    echo "[$serial] 文件已push，但重启命令失败" >&2
    return 2
}

# 多台设备并发烧录，最后统一汇总结果。
echo "开始并发烧录..."
declare -A flash_pid_to_serial=()
for serial in "${selected_boot_serials[@]}"; do
    flash_one_device "$serial" &
    pid=$!
    flash_pid_to_serial["$pid"]="$serial"
done

declare -a success_serials=()
declare -a failed_serials=()
for pid in "${!flash_pid_to_serial[@]}"; do
    serial="${flash_pid_to_serial[$pid]}"
    if wait "$pid"; then
        success_serials+=("$serial")
    else
        failed_serials+=("$serial")
    fi
done

echo
echo "烧录结果汇总:"
echo "- 成功: ${#success_serials[@]} 台"
for serial in "${success_serials[@]}"; do
    echo "  [OK] $serial"
done

echo "- 失败: ${#failed_serials[@]} 台"
for serial in "${failed_serials[@]}"; do
    echo "  [FAIL] $serial"
done

if [ "${#failed_serials[@]}" -gt 0 ]; then
    echo -e "${RED}存在失败设备，请检查USB连接、授权弹窗和设备日志${NC}"
    exit 1
fi

echo -e "${GREEN}全部设备烧录完成${NC}"
