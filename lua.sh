#!/bin/bash
# =============================================================================
# lua.sh - miniapp Lua 脚本的推送 / 回捞 / 看日志
#
# 用法:
#   ./lua.sh                    # 列出 tests/miniapp 下的本地脚本 + 当前设备
#   ./lua.sh 07                 # 推送匹配 07 的脚本并运行 (最常用; 等同于 push 07)
#   ./lua.sh 07-api-smoke       # 按文件名前缀匹配
#   ./lua.sh path/to/foo.lua    # 按路径推送
#   ./lua.sh push 07            # 同上, 显式写 push
#   ./lua.sh pull               # 把设备上正在运行的脚本拉回 ./tmp/device-app.lua
#   ./lua.sh pull my.lua        # 拉到指定文件
#   ./lua.sh log                # 抓 10 秒设备日志 (不重启), 看脚本运行输出
#   ./lua.sh log 30             # 抓 30 秒
#   ./lua.sh log -r             # 先重启设备再抓 (脚本不落盘, 重启后需重新 push)
#   ./lua.sh watch 07           # 推送后持续跟踪日志, Ctrl-C 退出
#   ./lua.sh bundle 20-desktop  # 把 tests/miniapp/desktop/*.lua 拼成单个可推送文件
#
# 环境变量:
#   DEVICE       显式指定 adb 序列号 (默认自动选 listenai 开发板)
#   LOG_SECONDS  log 模式采集秒数 (默认 10)
#   SCRIPT_DIR   本地脚本目录 (默认 tests/miniapp)
#   DEBUG=1      打印脚本执行轨迹 (set -x)
#
# 说明:
#   - 设备端 /miniapp/<id>.lua, <id> 由文件名主干决定 (字母/数字/连字符/下划线),
#     相同目标路径 = 同一个本地调试应用 (存档也按此 id 隔离)。
#   - push 后脚本立即运行; 源码只在内存里, 设备重启后失效, 需重新 push。
#   - 长按功能键 3 秒退出小应用。
# =============================================================================

set -u
[ -n "${DEBUG:-}" ] && set -x

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$REPO_ROOT" || exit 1

SCRIPT_DIR="${SCRIPT_DIR:-tests/miniapp}"
LOG_SECONDS="${LOG_SECONDS:-10}"
DEVICE_TARGET="/miniapp"

usage() {
    awk 'NR<2{next} /^#/{sub(/^# ?/,""); print; next} {exit}' "${BASH_SOURCE[0]}"
    exit 0
}

# ------------------------------------------------------------------ 设备选择
pick_device() {
    local dev="${DEVICE:-}"
    if [ -n "$dev" ]; then echo "$dev"; return 0; fi

    adb disconnect >/dev/null 2>&1 || true
    local c
    c=$(adb devices -l | awk '$2=="device" && !/^BOOT-/ && /model:listenai/ {print $1; exit}')
    [ -z "$c" ] && c=$(adb devices | awk '$2=="device" && $1 !~ /^BOOT-/ {print $1; exit}')
    if [ -z "$c" ]; then
        echo -e "${RED}错误: 未发现可用设备 (用 -d/-DEVICE= 指定)${NC}" >&2
        return 1
    fi
    echo "$c"
}

# ------------------------------------------------------------------ 脚本解析
# 支持: 完整路径 / 文件名 / 序号前缀 (01..)
resolve_script() {
    local arg="$1"
    if [ -f "$arg" ]; then echo "$arg"; return 0; fi

    local hit
    hit=$(find "$SCRIPT_DIR" -maxdepth 1 -name "${arg}*.lua" 2>/dev/null | sort | head -1)
    [ -z "$hit" ] && hit=$(find "$SCRIPT_DIR" -maxdepth 1 -name "*${arg}*.lua" 2>/dev/null | sort | head -1)
    if [ -z "$hit" ]; then
        echo -e "${RED}错误: 找不到脚本 '$arg'${NC}" >&2
        echo "  可用的:" >&2
        list_scripts >&2
        return 1
    fi
    echo "$hit"
}

list_scripts() {
    if [ ! -d "$SCRIPT_DIR" ]; then
        echo "  (无 $SCRIPT_DIR 目录)"
        return
    fi
    local n=0
    for f in "$SCRIPT_DIR"/*.lua; do
        [ -f "$f" ] || continue
        n=$((n + 1))
        printf '  %-24s %5s 行  %s\n' "$(basename "$f")" "$(wc -l < "$f")" \
            "$(sed -n '1{s/^-- *//;p;}' "$f")"
    done
    [ "$n" -eq 0 ] && echo "  (无 .lua 文件)"
    return 0
}

# ------------------------------------------------------------------ 动作
do_list() {
    local dev
    dev=$(pick_device 2>/dev/null) || dev="(未连接)"
    echo -e "${GREEN}本地脚本 ($SCRIPT_DIR):${NC}"
    list_scripts
    echo
    echo -e "${GREEN}设备:${NC} $dev"
    echo "  pull 取回当前运行的脚本: ./lua.sh pull"
}

do_push() {
    local arg="$1" src base id dev
    src=$(resolve_script "$arg") || exit 1
    base=$(basename "$src")
    id="${base%.lua}"

    # 设备端只认 字母/数字/连字符/下划线 构成的 id
    if ! printf '%s' "$id" | grep -Eq '^[A-Za-z0-9_-]{1,121}$'; then
        echo -e "${RED}错误: 文件名主干 '$id' 不能作为设备端 id (仅字母/数字/-/_)${NC}" >&2
        exit 1
    fi

    dev=$(pick_device) || exit 1
    echo "==> push $src -> $dev:$DEVICE_TARGET/$base"
    timeout 20 adb -s "$dev" push "$src" "$DEVICE_TARGET/$base" || {
        echo -e "${RED}推送失败${NC}" >&2; exit 1; }
    echo -e "${GREEN}已上传并立即运行 (id: local:$id, 长按功能键 3 秒退出)${NC}"
}

do_pull() {
    local dest="${1:-./tmp/device-app.lua}" dev
    dev=$(pick_device) || exit 1
    mkdir -p "$(dirname "$dest")"
    echo "==> pull $dev:/miniapp/miniapp.lua -> $dest"
    if ! timeout 20 adb -s "$dev" pull /miniapp/miniapp.lua "$dest"; then
        echo -e "${RED}拉取失败 (设备上没有正在运行的小应用?)${NC}" >&2
        exit 1
    fi
    echo -e "${GREEN}已取回: $dest ($(wc -l < "$dest") 行)${NC}"
    echo "   注意: 只有源码, 不含运行进度与存档。"
}

do_log() {
    local secs="$LOG_SECONDS" reboot=0 dev
    for a in "$@"; do
        case "$a" in
            -r|--reboot) reboot=1 ;;
            ''|*[!0-9]*) echo -e "${RED}错误: 未知参数 $a${NC}" >&2; exit 1 ;;
            *) secs="$a" ;;
        esac
    done
    dev=$(pick_device) || exit 1
    mkdir -p ./tmp
    local out=./tmp/run-log.txt
    : > "$out"
    if [ "$reboot" -eq 1 ]; then
        echo "==> 重启设备 (脚本不落盘, 需重新 push)"
        adb -s "$dev" shell reboot >/dev/null 2>&1 || true
        adb -s "$dev" wait-for-device >/dev/null 2>&1 || true
    fi
    echo "==> 抓取日志 $dev -> $out (${secs}s, Ctrl-C 提前结束)"
    timeout "$secs" adb -s "$dev" shell >> "$out" 2>&1 || true
    echo -e "${GREEN}已写入 $out ($(wc -l < "$out") 行)${NC}"
}

do_watch() {
    local arg="$1" dev
    src=$(resolve_script "$arg") || exit 1
    base=$(basename "$src")
    dev=$(pick_device) || exit 1
    echo "==> push $src -> $dev:$DEVICE_TARGET/$base"
    timeout 20 adb -s "$dev" push "$src" "$DEVICE_TARGET/$base" >/dev/null || {
        echo -e "${RED}推送失败${NC}" >&2; exit 1; }
    echo -e "${GREEN}已运行, 开始跟踪日志 (Ctrl-C 退出)${NC}"
    # 只显示与小应用/音频焦点相关的行, 其余(心跳、SD 卡重试、adb 会话簿记)噪声过滤掉
    adb -s "$dev" shell 2>&1 | sed -u \
        -e 's/\x1b\[[0-9;]*[a-zA-Z]//g' \
        -e '/sd_music\|lisa_sdmmc\|wakeup_algo\|adb\.core\|adb\.srv\|adb\.sh\|adb_rx_d/d'
}

# ------------------------------------------------------------------ 打包
# 运行环境只能加载单个 Lua 文件, 所以桌面 + 全部应用必须拼成一个文件再推送。
# 源按文件名顺序拼接: 00-shell.lua 必须先, 它声明 ui 表供后续文件 register。
#
# 源码里带大量中文设计说明, 原样拼会顶满源码上限; 设备只认代码, 所以
# 打包时把"整行注释"清成空行 —— 只清空内容不清掉换行, 于是产物与源的行号一一
# 对应(产物行号 = 拼接后的行号), 设备报的 cloud-miniapp:N 行号仍能定位回源文件。
#
# 安全性: 行首 -- 一定是注释, 除非它落在跨行字符串里 (Lua 长括号 [[ ]])。
# 下面显式检查长括号, 有就整体放弃剥离, 只按原样拼接。
do_bundle() {
    local name="${1:-}"
    local src_dir="$SCRIPT_DIR/desktop"
    [ -n "$name" ] || { echo -e "${RED}错误: bundle 需要产物名, 如 ./lua.sh bundle 20-desktop${NC}" >&2; exit 1; }
    [ -d "$src_dir" ] || { echo -e "${RED}错误: 找不到源目录 $src_dir${NC}" >&2; exit 1; }

    local files
    files=$(find "$src_dir" -maxdepth 1 -name '*.lua' | sort)
    [ -n "$files" ] || { echo -e "${RED}错误: $src_dir 下没有 .lua${NC}" >&2; exit 1; }

    # 顶层 return (行首, 无缩进) 只结束它所在的文件, 拼接后会截断后续所有文件
    local f
    for f in $files; do
        if grep -qE '^return([[:space:]]|$)' "$f"; then
            echo -e "${RED}错误: $f 顶层出现 return, 拼接会截断后续文件${NC}" >&2
            exit 1
        fi
    done

    local strip=1
    if grep -qE '\[\[|--\[\[' $files; then
        strip=0
        echo -e "${YELLOW}注意: 源码含 Lua 长括号, 跳过注释剥离 (按原样拼接)${NC}" >&2
    elif grep -qE '\\$' $files; then
        strip=0
        echo -e "${YELLOW}注意: 源码含行尾反斜杠续行, 跳过注释剥离 (按原样拼接)${NC}" >&2
    fi

    local out="$SCRIPT_DIR/${name}.lua"
    # 与 apps/arcs-mini/prj.conf 的 CONFIG_MINIAPP_SOURCE_MAX_BYTES 保持一致
    local limit=131072
    local nfiles
    nfiles=$(printf '%s\n' "$files" | wc -l)

    # 头部固定 2 行, 所以源文件里的行号 + 2 = 产物行号
    {
        printf -- '-- 由 ./lua.sh bundle %s 生成, 请勿手改 (源: %s)\n' "$name" "$src_dir"
        printf -- '-- 产物行号 - 2 = 拼接后的源行号; 注释已在打包时清空\n'
        if [ "$strip" -eq 1 ]; then
            # ① 整行注释清空; ② 行尾注释: 只在不含引号的行上剥 —— Lua 里除了字符串
            #    没有别的用法会产生 --, 所以"这行没有引号"⇒ 这个 -- 一定是注释。
            #    (含引号的行保守跳过, 免得把字符串里的 -- 当注释切了)
            sed -e 's/^[[:space:]]*--.*$//' \
                -e '/["'"'"']/!s/[[:space:]]\+--.*$//' $files
        else
            cat $files
        fi
    } > "$out"

    local bytes
    bytes=$(wc -c < "$out")
    echo "==> 打包: $nfiles 个源文件 -> $out"
    printf '     %s / %s 字节 (%s%%)\n' "$bytes" "$limit" "$((bytes * 100 / limit))"
    if [ "$bytes" -gt "$limit" ]; then
        echo -e "${RED}错误: 超过源码上限 $limit 字节, 需要精简应用${NC}" >&2
        exit 1
    fi

    # 行号对照表: 设备报错 (cloud-miniapp:N) 据此回查源文件
    local line=3
    for f in $files; do
        local cnt
        cnt=$(wc -l < "$f")
        printf '     %-22s 产物行 %4d..%4d\n' "$(basename "$f")" "$line" "$((line + cnt - 1))"
        line=$((line + cnt))
    done
    echo -e "${GREEN}打包完成, 推送: ./lua.sh $name${NC}"
}


# ------------------------------------------------------------------ 主流程
CMD="${1:-list}"
case "$CMD" in
    -h|--help|help) usage ;;
    list|ls)        do_list ;;
    push)           [ "$#" -lt 2 ] && { echo -e "${RED}错误: push 需要脚本名${NC}" >&2; exit 1; }; do_push "$2" ;;
    pull)           shift; do_pull "${1:-}" ;;
    log)            shift; do_log "$@" ;;
    watch)          [ "$#" -lt 2 ] && { echo -e "${RED}错误: watch 需要脚本名${NC}" >&2; exit 1; }; do_watch "$2" ;;
    bundle)         shift; do_bundle "${1:-}" ;;
    *)              do_push "$CMD" ;;   # 直接用脚本名/序号 = push
esac
