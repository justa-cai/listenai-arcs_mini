#!/bin/bash
# ARCS SDK 开发环境配置脚本
# 用法: source env.sh [command]
#
# 默认行为（无参数）: 检测环境 → 缺失则安装 → 设置环境变量 → 检测子模块并提示
# 子命令:
#   check              完整环境检测报告
#   setup              安装工具链（即使已存在也重新安装）
#   submodule sync     清理孤儿子模块 + 初始化/更新
#   submodule status   查看子模块状态
#   submodule clean    清理孤儿子模块（交互式）
#   info               显示 SDK 和工具链版本信息

# ─── 防止直接执行（必须 source） ───────────────────────────────────────────────

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "请使用 source 执行此脚本:"
    echo "  source env.sh [command]"
    exit 1
fi

# ─── 基础变量 ─────────────────────────────────────────────────────────────────

_ENV_SDK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_ENV_DEV_TOOLS_DIR_NAME="listenai-dev-tools"
_ENV_TOOLCHAIN_DIR_NAME="gcc"
_ENV_TOOLS_DIR_NAME="listenai-tools"

# ─── 颜色输出 ─────────────────────────────────────────────────────────────────

_env_color_reset="\033[0m"
_env_color_green="\033[32m"
_env_color_yellow="\033[33m"
_env_color_red="\033[31m"
_env_color_cyan="\033[36m"
_env_color_bold="\033[1m"

_env_ok()   { echo -e "  ${_env_color_green}[OK]${_env_color_reset}   $1"; }
_env_warn() { echo -e "  ${_env_color_yellow}[WARN]${_env_color_reset} $1"; }
_env_miss() { echo -e "  ${_env_color_red}[MISS]${_env_color_reset} $1"; }
_env_info() { echo -e "  ${_env_color_cyan}[INFO]${_env_color_reset} $1"; }
_env_header() { echo -e "\n${_env_color_bold}$1${_env_color_reset}"; }

# ─── 工具链验证 ───────────────────────────────────────────────────────────────

_env_toolchain_valid() {
    # 检查 NUCLEI_TOOLCHAIN_PATH 是否已设置且 gcc 可实际运行
    [ -n "${NUCLEI_TOOLCHAIN_PATH:-}" ] && \
    "$NUCLEI_TOOLCHAIN_PATH/bin/riscv64-unknown-elf-gcc" --version &>/dev/null
}

_env_tools_valid() {
    # 检查 LISTENAI_TOOLS_PATH 是否已设置且 cmake 可实际运行
    [ -n "${LISTENAI_TOOLS_PATH:-}" ] && \
    "$LISTENAI_TOOLS_PATH/cmake/bin/cmake" --version &>/dev/null
}

# ─── 在父目录中查找 dev-tools ─────────────────────────────────────────────────

_env_find_dev_tools() {
    # 优先检查 ~/.listenai
    if [ -d "${HOME}/.listenai" ]; then
        _ENV_DEV_TOOLS_PATH="${HOME}/.listenai"
        return 0
    fi
    # 向上查找 listenai-dev-tools（兼容旧路径）
    local current_dir="$_ENV_SDK_DIR"
    while [ "$current_dir" != "/" ]; do
        if [ -d "$current_dir/$_ENV_DEV_TOOLS_DIR_NAME" ]; then
            _ENV_DEV_TOOLS_PATH="$current_dir/$_ENV_DEV_TOOLS_DIR_NAME"
            return 0
        fi
        current_dir="$(dirname "$current_dir")"
    done
    return 1
}

# ─── 从 dev-tools 目录设置环境变量 ────────────────────────────────────────────

_env_set_from_dev_tools() {
    local dev_tools_path="$1"

    local toolchain_path="$dev_tools_path/$_ENV_TOOLCHAIN_DIR_NAME"
    if "$toolchain_path/bin/riscv64-unknown-elf-gcc" --version &>/dev/null; then
        export NUCLEI_TOOLCHAIN_PATH="$toolchain_path"
    fi

    local tools_path="$dev_tools_path/$_ENV_TOOLS_DIR_NAME"
    if "$tools_path/cmake/bin/cmake" --version &>/dev/null; then
        export LISTENAI_TOOLS_PATH="$tools_path"
    fi
}

# ─── PATH 注入 ────────────────────────────────────────────────────────────────

_env_setup_path() {
    export ARCS_BASE="$_ENV_SDK_DIR"

    local paths_to_add=()

    # cskburn — macOS 优先使用 listenai-tools 下的原生二进制
    if [[ "$(uname)" == "Darwin" ]] && [ -n "${LISTENAI_TOOLS_PATH:-}" ] && \
       [ -x "$LISTENAI_TOOLS_PATH/cskburn/cskburn" ]; then
        paths_to_add+=("$LISTENAI_TOOLS_PATH/cskburn")
    else
        local burn_dir="$_ENV_SDK_DIR/tools/burn"
        [ -d "$burn_dir" ] && paths_to_add+=("$burn_dir")
    fi

    # toolchain bin
    if [ -n "${NUCLEI_TOOLCHAIN_PATH:-}" ] && [ -d "$NUCLEI_TOOLCHAIN_PATH/bin" ]; then
        paths_to_add+=("$NUCLEI_TOOLCHAIN_PATH/bin")
    fi

    # listenai-tools cmake/ninja
    if [ -n "${LISTENAI_TOOLS_PATH:-}" ]; then
        [ -d "$LISTENAI_TOOLS_PATH/cmake/bin" ] && paths_to_add+=("$LISTENAI_TOOLS_PATH/cmake/bin")
        [ -d "$LISTENAI_TOOLS_PATH/ninja" ] && paths_to_add+=("$LISTENAI_TOOLS_PATH/ninja")
    fi

    for p in "${paths_to_add[@]}"; do
        case ":$PATH:" in
            *":$p:"*) ;;
            *) export PATH="$p:$PATH" ;;
        esac
    done
}

# ─── 工具链安装 ───────────────────────────────────────────────────────────────

_env_check_deps() {
    # 检查系统依赖，返回缺失的包列表
    local missing=()
    command -v wget &>/dev/null || command -v curl &>/dev/null || missing+=("wget")
    command -v bzip2 &>/dev/null || missing+=("bzip2")
    command -v git &>/dev/null || missing+=("git")
    if [ ${#missing[@]} -gt 0 ]; then
        _env_miss "缺少系统依赖: ${missing[*]}"
        if [[ "$(uname)" == "Darwin" ]]; then
            _env_info "请运行: brew install ${missing[*]}"
        else
            _env_info "请运行: sudo apt install -y ${missing[*]}"
        fi
        return 1
    fi
    return 0
}

_env_install_toolchain() {
    local dev_tools_path="$1"
    local force="${2:-false}"
    local has_error=false

    # 前置依赖检查
    if ! _env_check_deps; then
        return 1
    fi

    # GCC 交叉编译器
    local toolchain_path="$dev_tools_path/$_ENV_TOOLCHAIN_DIR_NAME"
    if [ "$force" = false ] && "$toolchain_path/bin/riscv64-unknown-elf-gcc" --version &>/dev/null; then
        _env_ok "GCC 工具链已存在，跳过"
    else
        if [[ "$(uname)" == "Darwin" ]]; then
            _env_miss "GCC 工具链未安装（macOS 需手动编译）"
            _env_info "请运行: bash /tmp/build-riscv-toolchain.sh"
            _env_info "或设置 NUCLEI_TOOLCHAIN_PATH 指向已编译的工具链"
            has_error=true
        else
            _env_info "正在下载 GCC 工具链..."
            if bash "$_ENV_SDK_DIR/tools/scripts/prepare_toolchain.sh" "$dev_tools_path"; then
                _env_ok "GCC 工具链安装完成"
            else
                _env_miss "GCC 工具链安装失败"
                _env_info "可尝试手动安装: bash tools/scripts/prepare_toolchain.sh"
                has_error=true
            fi
        fi
    fi

    # listenai-tools（独立于 GCC，不因 GCC 失败而跳过）
    local tools_path="$dev_tools_path/$_ENV_TOOLS_DIR_NAME"
    if [ "$force" = false ] && "$tools_path/cmake/bin/cmake" --version &>/dev/null; then
        _env_ok "listenai-tools 已存在，跳过"
    else
        if [[ "$(uname)" == "Darwin" ]]; then
            _env_miss "listenai-tools 未安装（macOS 需手动配置）"
            _env_info "请参考 macOS 适配文档配置 listenai-tools"
            has_error=true
        else
            _env_info "正在下载 listenai-tools..."
            if bash "$_ENV_SDK_DIR/tools/scripts/prepare_listenai_tools.sh" "$dev_tools_path"; then
                _env_ok "listenai-tools 安装完成"
            else
                _env_miss "listenai-tools 安装失败"
                _env_info "可尝试手动安装: bash tools/scripts/prepare_listenai_tools.sh"
                has_error=true
            fi
        fi
    fi

    [ "$has_error" = true ] && return 1
    return 0
}

# ─── 子模块快速检测 ───────────────────────────────────────────────────────────

_env_submodule_check_quick() {
    # 快速检测未初始化的子模块和孤儿，返回提示信息
    local uninit=0
    local uninit_list=()

    while IFS= read -r line; do
        if [[ "$line" == -* ]]; then
            uninit=$((uninit + 1))
            uninit_list+=("$(echo "$line" | awk '{print $2}')")
        fi
    done < <(git -C "$_ENV_SDK_DIR" submodule status 2>/dev/null)

    # 检查孤儿
    local orphan_output
    orphan_output=$(python3 "$_ENV_SDK_DIR/tools/scripts/clean_git_repos.py" --root-path "$_ENV_SDK_DIR" --dry-run 2>&1)
    local orphan_count
    orphan_count=$(echo "$orphan_output" | sed -n '/not in .gitmodules/,$ { /^  - /p }' | wc -l)

    if [ $uninit -gt 0 ] || [ "$orphan_count" -gt 0 ]; then
        if [ $uninit -gt 0 ]; then
            _env_warn "子模块: $uninit 个未初始化"
            for p in "${uninit_list[@]}"; do
                echo "         - $p"
            done
        fi
        if [ "$orphan_count" -gt 0 ]; then
            _env_warn "子模块: $orphan_count 个孤儿（不在 .gitmodules 中）"
        fi
        _env_info "运行 'source env.sh submodule sync' 修复"
    else
        _env_ok "子模块状态正常"
    fi
}

# ─── 子模块管理 ───────────────────────────────────────────────────────────────

_env_submodule_sync() {
    _env_header "同步子模块"

    _env_info "清理孤儿子模块..."
    python3 "$_ENV_SDK_DIR/tools/scripts/clean_git_repos.py" --root-path "$_ENV_SDK_DIR" -y

    _env_info "更新子模块..."
    git -C "$_ENV_SDK_DIR" submodule update --init --recursive
    if [ $? -eq 0 ]; then
        _env_ok "子模块同步完成"
    else
        _env_warn "部分子模块更新失败，请检查网络"
    fi
}

_env_submodule_status() {
    _env_header "子模块状态"

    local total=0 initialized=0 uninitialized=0
    local uninit_list=()

    while IFS= read -r line; do
        total=$((total + 1))
        if [[ "$line" == -* ]]; then
            uninitialized=$((uninitialized + 1))
            uninit_list+=("$(echo "$line" | awk '{print $2}')")
        else
            initialized=$((initialized + 1))
        fi
    done < <(git -C "$_ENV_SDK_DIR" submodule status)

    _env_info "总计: $total  已初始化: $initialized  未初始化: $uninitialized"

    if [ $uninitialized -gt 0 ]; then
        echo ""
        _env_warn "未初始化的子模块:"
        for p in "${uninit_list[@]}"; do
            echo "         - $p"
        done
    fi

    local orphan_output
    orphan_output=$(python3 "$_ENV_SDK_DIR/tools/scripts/clean_git_repos.py" --root-path "$_ENV_SDK_DIR" --dry-run 2>&1)
    local orphans
    orphans=$(echo "$orphan_output" | sed -n '/not in .gitmodules/,$ { /^  - /s/^  - //p }')
    if [ -n "$orphans" ]; then
        echo ""
        _env_warn "孤儿子模块（不在 .gitmodules 中）:"
        while IFS= read -r p; do
            echo "         - $p"
        done <<< "$orphans"
    fi
}

_env_submodule_clean() {
    _env_header "清理孤儿子模块"
    python3 "$_ENV_SDK_DIR/tools/scripts/clean_git_repos.py" --root-path "$_ENV_SDK_DIR"
}

# ─── 环境检测 ─────────────────────────────────────────────────────────────────

_env_check() {
    _env_header "环境检测"

    local has_error=false

    # GCC 工具链
    if _env_toolchain_valid; then
        local gcc_ver
        gcc_ver=$("$NUCLEI_TOOLCHAIN_PATH/bin/riscv64-unknown-elf-gcc" --version 2>/dev/null | head -1 | sed 's/.*) //')
        _env_ok "GCC toolchain       $gcc_ver"
        _env_info "                    $NUCLEI_TOOLCHAIN_PATH"
    else
        _env_miss "GCC toolchain       未找到"
        has_error=true
    fi

    # listenai-tools
    if _env_tools_valid; then
        local cmake_ver
        cmake_ver=$("$LISTENAI_TOOLS_PATH/cmake/bin/cmake" --version 2>/dev/null | head -1 | awk '{print $NF}')
        _env_ok "CMake               $cmake_ver"
        local ninja_ver
        ninja_ver=$("$LISTENAI_TOOLS_PATH/ninja/ninja" --version 2>/dev/null)
        _env_ok "Ninja               $ninja_ver"
        _env_info "                    $LISTENAI_TOOLS_PATH"
    else
        _env_miss "listenai-tools      未找到"
        has_error=true
    fi

    # ccache（可选）
    if command -v ccache &>/dev/null; then
        local ccache_ver
        ccache_ver=$(ccache --version 2>/dev/null | head -1 | awk '{print $NF}')
        _env_ok "ccache              $ccache_ver"
    else
        _env_warn "ccache              未安装 (可选，建议 apt install ccache)"
    fi

    # Python3
    if command -v python3 &>/dev/null; then
        local py_ver
        py_ver=$(python3 --version 2>/dev/null | awk '{print $2}')
        _env_ok "Python3             $py_ver"
    else
        _env_miss "Python3             未安装"
        has_error=true
    fi

    # cskburn
    if [ -x "$_ENV_SDK_DIR/tools/burn/cskburn" ]; then
        _env_ok "cskburn             tools/burn/cskburn"
    else
        _env_warn "cskburn             未找到"
    fi

    # 串口权限
    if groups 2>/dev/null | grep -q dialout; then
        _env_ok "串口权限            用户在 dialout 组"
    else
        _env_warn "串口权限            用户不在 dialout 组 (sudo usermod -aG dialout \$USER)"
    fi

    # 子模块
    _env_submodule_check_quick

    if [ "$has_error" = true ]; then
        return 1
    fi
    return 0
}

# ─── 版本信息 ─────────────────────────────────────────────────────────────────

_env_info_cmd() {
    _env_header "ARCS SDK 信息"

    local version_file="$_ENV_SDK_DIR/VERSION"
    if [ -f "$version_file" ]; then
        local major minor patch
        major=$(grep 'VERSION_MAJOR' "$version_file" | awk -F= '{print $2}' | tr -d ' ')
        minor=$(grep 'VERSION_MINOR' "$version_file" | awk -F= '{print $2}' | tr -d ' ')
        patch=$(grep 'PATCHLEVEL' "$version_file" | awk -F= '{print $2}' | tr -d ' ')
        _env_info "SDK 版本            v${major}.${minor}.${patch}"
    fi

    local branch
    branch=$(git -C "$_ENV_SDK_DIR" branch --show-current 2>/dev/null)
    [ -n "$branch" ] && _env_info "Git 分支            $branch"

    _env_info "ARCS_BASE           ${ARCS_BASE:-未设置}"
    _env_info "NUCLEI_TOOLCHAIN    ${NUCLEI_TOOLCHAIN_PATH:-未设置}"
    _env_info "LISTENAI_TOOLS      ${LISTENAI_TOOLS_PATH:-未设置}"
}

# ─── 默认行为 ─────────────────────────────────────────────────────────────────

_env_default() {
    local has_problem=false

    # ── 1. 工具链 ──
    # 优先级: 已有环境变量 → 查找 listenai-dev-tools/ → 下载安装

    if _env_toolchain_valid && _env_tools_valid; then
        # 环境变量已设且有效，直接用
        :
    elif _env_find_dev_tools; then
        # 从 dev-tools 目录补全缺失的环境变量
        _env_set_from_dev_tools "$_ENV_DEV_TOOLS_PATH"

        # 补全后仍有缺失，安装缺失部分
        if ! _env_toolchain_valid || ! _env_tools_valid; then
            _env_install_toolchain "$_ENV_DEV_TOOLS_PATH"
            _env_set_from_dev_tools "$_ENV_DEV_TOOLS_PATH"
        fi
    else
        # 都没有，下载安装
        _ENV_DEV_TOOLS_PATH="${HOME}/.listenai"
        _env_info "工具链未找到，正在下载安装..."
        _env_install_toolchain "$_ENV_DEV_TOOLS_PATH"
        _env_set_from_dev_tools "$_ENV_DEV_TOOLS_PATH"
    fi

    # 最终验证
    if _env_toolchain_valid; then
        local gcc_ver
        gcc_ver=$("$NUCLEI_TOOLCHAIN_PATH/bin/riscv64-unknown-elf-gcc" --version 2>/dev/null | head -1 | sed 's/.*) //')
        _env_ok "GCC toolchain       $gcc_ver"
    else
        _env_miss "GCC toolchain       未就绪"
        has_problem=true
    fi

    if _env_tools_valid; then
        local cmake_ver
        cmake_ver=$("$LISTENAI_TOOLS_PATH/cmake/bin/cmake" --version 2>/dev/null | head -1 | awk '{print $NF}')
        _env_ok "listenai-tools      CMake $cmake_ver"
    else
        _env_miss "listenai-tools      未就绪"
        has_problem=true
    fi

    if [ "$has_problem" = true ]; then
        echo ""
        _env_info "解决方法:"
        _env_info "  方法一: 安装依赖后自动下载"
        _env_info "    sudo apt install -y wget bzip2"
        _env_info "    source env.sh setup"
        echo ""
        _env_info "  方法二: 已有工具链时手动设置"
        _env_info "    export NUCLEI_TOOLCHAIN_PATH=\$HOME/.listenai/gcc"
        _env_info "    export LISTENAI_TOOLS_PATH=\$HOME/.listenai/listenai-tools"
        _env_info "    source env.sh"
    fi

    # ── 2. 设置 PATH ──
    _env_setup_path

    # ── 3. 子模块检测（仅报告） ──
    _env_submodule_check_quick

    # ── 摘要 ──
    if [ "$has_problem" = false ]; then
        echo ""
        _env_ok "环境就绪"
    fi
}

# ─── 命令路由 ─────────────────────────────────────────────────────────────────

_env_main() {
    local cmd="${1:-}"
    local subcmd="${2:-}"

    case "$cmd" in
        "")
            _env_default
            ;;
        check)
            # check 先尝试补全环境变量，确保检测结果准确
            if ! _env_toolchain_valid || ! _env_tools_valid; then
                if _env_find_dev_tools; then
                    _env_set_from_dev_tools "$_ENV_DEV_TOOLS_PATH"
                fi
            fi
            _env_setup_path
            _env_check
            ;;
        setup)
            if ! _env_find_dev_tools; then
                _ENV_DEV_TOOLS_PATH="${HOME}/.listenai"
            fi
            if _env_install_toolchain "$_ENV_DEV_TOOLS_PATH" true; then
                _env_set_from_dev_tools "$_ENV_DEV_TOOLS_PATH"
                _env_setup_path
                _env_ok "工具链安装完成，环境变量已设置"
            else
                _env_set_from_dev_tools "$_ENV_DEV_TOOLS_PATH"
                _env_setup_path
                _env_miss "部分工具安装失败，请检查网络后重试"
            fi
            ;;
        submodule)
            case "$subcmd" in
                sync)   _env_submodule_sync ;;
                status) _env_submodule_status ;;
                clean)  _env_submodule_clean ;;
                *)      echo "用法: source env.sh submodule {sync|status|clean}" ;;
            esac
            ;;
        info)
            if ! _env_toolchain_valid || ! _env_tools_valid; then
                if _env_find_dev_tools; then
                    _env_set_from_dev_tools "$_ENV_DEV_TOOLS_PATH"
                fi
            fi
            _env_info_cmd
            ;;
        help|-h|--help)
            echo "ARCS SDK 环境配置脚本"
            echo ""
            echo "用法: source env.sh [command]"
            echo ""
            echo "命令:"
            echo "  (无参数)           检测环境 → 缺失则安装 → 设置变量 → 检测子模块"
            echo "  check              完整环境检测报告"
            echo "  setup              安装工具链"
            echo "  submodule sync     清理孤儿 + 初始化/更新子模块"
            echo "  submodule status   查看子模块状态"
            echo "  submodule clean    清理孤儿子模块（交互式）"
            echo "  info               显示版本信息"
            echo "  help               显示此帮助"
            ;;
        *)
            echo "未知命令: $cmd"
            echo "使用 'source env.sh help' 查看帮助"
            ;;
    esac
}

# 执行
_env_main "$@"

# 清理内部函数，避免污染用户 shell
unset -f _env_main _env_default _env_check _env_check_deps _env_install_toolchain
unset -f _env_set_from_dev_tools _env_setup_path _env_find_dev_tools _env_info_cmd
unset -f _env_toolchain_valid _env_tools_valid
unset -f _env_submodule_sync _env_submodule_status _env_submodule_clean _env_submodule_check_quick
unset -f _env_ok _env_warn _env_miss _env_info _env_header
unset _env_color_reset _env_color_green _env_color_yellow _env_color_red _env_color_cyan _env_color_bold
unset _ENV_SDK_DIR _ENV_DEV_TOOLS_DIR_NAME _ENV_TOOLCHAIN_DIR_NAME _ENV_TOOLS_DIR_NAME _ENV_DEV_TOOLS_PATH
