#!/bin/bash
# env.sh 隔离测试脚本
# 用临时目录模拟各种场景，mock prepare 脚本避免真实下载

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEST_TMPDIR=""
PASS=0
FAIL=0

# ─── 工具函数 ─────────────────────────────────────────────────────────────────

setup_test() {
    TEST_TMPDIR=$(mktemp -d)
    # 清空可能影响测试的环境变量
    unset NUCLEI_TOOLCHAIN_PATH LISTENAI_TOOLS_PATH ARCS_BASE 2>/dev/null
    unset ARCS_TEST_HOST_OS ARCS_TEST_HOST_ARCH 2>/dev/null
    # 确保 ~/.listenai 的存在不影响测试：备份 HOME
    export REAL_HOME="$HOME"
    export HOME="$TEST_TMPDIR/fakehome"
    mkdir -p "$HOME"
}

teardown_test() {
    export HOME="$REAL_HOME"
    unset REAL_HOME
    [ -n "$TEST_TMPDIR" ] && rm -rf "$TEST_TMPDIR"
    unset NUCLEI_TOOLCHAIN_PATH LISTENAI_TOOLS_PATH ARCS_BASE 2>/dev/null
    unset ARCS_TEST_HOST_OS ARCS_TEST_HOST_ARCH 2>/dev/null
}

# 创建一个可正常运行的 fake gcc（shell 脚本）
create_fake_gcc() {
    local dir="$1"
    mkdir -p "$dir/bin"
    local tool
    for tool in gcc g++ ld objcopy objdump readelf size nm ar ranlib; do
        cat > "$dir/bin/riscv64-unknown-elf-$tool" << 'FAKETOOL'
#!/bin/bash
case "$1" in
    -dumpmachine)
        echo "riscv64-unknown-elf"
        ;;
    --version)
        echo "$(basename "$0") (fake) 13.2.0"
        ;;
esac
exit 0
FAKETOOL
        chmod +x "$dir/bin/riscv64-unknown-elf-$tool"
    done
}

# 创建一个可正常运行的 fake cmake
create_fake_cmake() {
    local dir="$1"
    mkdir -p "$dir/cmake/bin" "$dir/ninja"
    cat > "$dir/cmake/bin/cmake" << 'FAKECMAKE'
#!/bin/bash
echo "cmake version 3.28.0"
FAKECMAKE
    chmod +x "$dir/cmake/bin/cmake"
    cat > "$dir/ninja/ninja" << 'FAKENINJA'
#!/bin/bash
echo "1.11.1"
FAKENINJA
    chmod +x "$dir/ninja/ninja"
}

# 创建损坏的 gcc（文件存在、有执行权限，但不可运行）
create_corrupted_gcc() {
    local dir="$1"
    mkdir -p "$dir/bin"
    echo "CORRUPTED_BINARY_DATA" > "$dir/bin/riscv64-unknown-elf-gcc"
    chmod +x "$dir/bin/riscv64-unknown-elf-gcc"
}

# 创建损坏的 cmake
create_corrupted_cmake() {
    local dir="$1"
    mkdir -p "$dir/cmake/bin"
    echo "CORRUPTED_BINARY_DATA" > "$dir/cmake/bin/cmake"
    chmod +x "$dir/cmake/bin/cmake"
}

# 创建 mock prepare 脚本（模拟成功安装）
create_mock_prepare_success() {
    local sdk="$1"
    # mock prepare_toolchain.sh
    cat > "$sdk/tools/scripts/prepare_toolchain.sh.bak" << 'MOCK'
#!/bin/bash
set -e
INSTALL_DIR="${1:-${HOME}/.listenai}"
GCC_DIR="${INSTALL_DIR}/gcc"
mkdir -p "${GCC_DIR}/bin"
for tool in gcc g++ ld objcopy objdump readelf size nm ar ranlib; do
cat > "${GCC_DIR}/bin/riscv64-unknown-elf-${tool}" << 'FAKETOOL'
#!/bin/bash
case "$1" in
    -dumpmachine)
        echo "riscv64-unknown-elf"
        ;;
    --version)
        echo "$(basename "$0") (fake) 13.2.0"
        ;;
esac
exit 0
FAKETOOL
chmod +x "${GCC_DIR}/bin/riscv64-unknown-elf-${tool}"
done
MOCK
    chmod +x "$sdk/tools/scripts/prepare_toolchain.sh.bak"

    # mock prepare_listenai_tools.sh
    cat > "$sdk/tools/scripts/prepare_listenai_tools.sh.bak" << 'MOCK'
#!/bin/bash
set -e
INSTALL_DIR="${1:-${HOME}/.listenai}"
mkdir -p "${INSTALL_DIR}/listenai-tools/cmake/bin" "${INSTALL_DIR}/listenai-tools/ninja"
cat > "${INSTALL_DIR}/listenai-tools/cmake/bin/cmake" << 'FAKECMAKE'
#!/bin/bash
echo "cmake version 3.28.0"
FAKECMAKE
chmod +x "${INSTALL_DIR}/listenai-tools/cmake/bin/cmake"
cat > "${INSTALL_DIR}/listenai-tools/ninja/ninja" << 'FAKENINJA'
#!/bin/bash
echo "1.11.1"
FAKENINJA
chmod +x "${INSTALL_DIR}/listenai-tools/ninja/ninja"
MOCK
    chmod +x "$sdk/tools/scripts/prepare_listenai_tools.sh.bak"
}

# 创建 mock prepare 脚本（模拟失败）
create_mock_prepare_fail() {
    local sdk="$1"
    cat > "$sdk/tools/scripts/prepare_toolchain.sh.bak" << 'MOCK'
#!/bin/bash
echo "模拟下载失败" >&2
exit 1
MOCK
    chmod +x "$sdk/tools/scripts/prepare_toolchain.sh.bak"

    cat > "$sdk/tools/scripts/prepare_listenai_tools.sh.bak" << 'MOCK'
#!/bin/bash
echo "模拟下载失败" >&2
exit 1
MOCK
    chmod +x "$sdk/tools/scripts/prepare_listenai_tools.sh.bak"
}

# 用 mock 替换真实脚本（保存原件）
swap_to_mock() {
    local sdk="$1"
    cp "$sdk/tools/scripts/prepare_toolchain.sh" "$sdk/tools/scripts/prepare_toolchain.sh.orig"
    cp "$sdk/tools/scripts/prepare_listenai_tools.sh" "$sdk/tools/scripts/prepare_listenai_tools.sh.orig"
    cp "$sdk/tools/scripts/prepare_toolchain.sh.bak" "$sdk/tools/scripts/prepare_toolchain.sh"
    cp "$sdk/tools/scripts/prepare_listenai_tools.sh.bak" "$sdk/tools/scripts/prepare_listenai_tools.sh"
}

# 恢复真实脚本
restore_real() {
    local sdk="$1"
    if [ -f "$sdk/tools/scripts/prepare_toolchain.sh.orig" ]; then
        mv "$sdk/tools/scripts/prepare_toolchain.sh.orig" "$sdk/tools/scripts/prepare_toolchain.sh"
    fi
    if [ -f "$sdk/tools/scripts/prepare_listenai_tools.sh.orig" ]; then
        mv "$sdk/tools/scripts/prepare_listenai_tools.sh.orig" "$sdk/tools/scripts/prepare_listenai_tools.sh"
    fi
    rm -f "$sdk/tools/scripts/prepare_toolchain.sh.bak" "$sdk/tools/scripts/prepare_listenai_tools.sh.bak"
}

assert_contains() {
    local output="$1"
    local expected="$2"
    local test_name="$3"
    if echo "$output" | grep -q "$expected"; then
        echo -e "  \033[32m[PASS]\033[0m $test_name"
        PASS=$((PASS + 1))
    else
        echo -e "  \033[31m[FAIL]\033[0m $test_name"
        echo "         期望包含: $expected"
        echo "         实际输出: $(echo "$output" | head -5)"
        FAIL=$((FAIL + 1))
    fi
}

assert_not_contains() {
    local output="$1"
    local unexpected="$2"
    local test_name="$3"
    if ! echo "$output" | grep -q "$unexpected"; then
        echo -e "  \033[32m[PASS]\033[0m $test_name"
        PASS=$((PASS + 1))
    else
        echo -e "  \033[31m[FAIL]\033[0m $test_name"
        echo "         不应包含: $unexpected"
        FAIL=$((FAIL + 1))
    fi
}

assert_env() {
    local var_name="$1"
    local expected="$2"
    local test_name="$3"
    local actual="${!var_name:-}"
    if [ "$actual" = "$expected" ]; then
        echo -e "  \033[32m[PASS]\033[0m $test_name"
        PASS=$((PASS + 1))
    else
        echo -e "  \033[31m[FAIL]\033[0m $test_name"
        echo "         期望 $var_name=$expected"
        echo "         实际 $var_name=$actual"
        FAIL=$((FAIL + 1))
    fi
}

assert_env_set() {
    local var_name="$1"
    local test_name="$2"
    local actual="${!var_name:-}"
    if [ -n "$actual" ]; then
        echo -e "  \033[32m[PASS]\033[0m $test_name"
        PASS=$((PASS + 1))
    else
        echo -e "  \033[31m[FAIL]\033[0m $test_name"
        echo "         期望 $var_name 已设置，实际为空"
        FAIL=$((FAIL + 1))
    fi
}

assert_env_empty() {
    local var_name="$1"
    local test_name="$2"
    local actual="${!var_name:-}"
    if [ -z "$actual" ]; then
        echo -e "  \033[32m[PASS]\033[0m $test_name"
        PASS=$((PASS + 1))
    else
        echo -e "  \033[31m[FAIL]\033[0m $test_name"
        echo "         期望 $var_name 为空，实际=$actual"
        FAIL=$((FAIL + 1))
    fi
}

# ─── 测试场景 ─────────────────────────────────────────────────────────────────

echo ""
echo "========================================"
echo " env.sh 隔离测试"
echo "========================================"

# ── 场景 1: 环境变量已设且有效 ──
echo ""
echo "── 场景 1: 环境变量已设且有效，直接使用 ──"
setup_test
create_fake_gcc "$TEST_TMPDIR/my-gcc"
create_fake_cmake "$TEST_TMPDIR/my-tools"
export NUCLEI_TOOLCHAIN_PATH="$TEST_TMPDIR/my-gcc"
export LISTENAI_TOOLS_PATH="$TEST_TMPDIR/my-tools"
_test_output_file="$TEST_TMPDIR/output.txt"
source "$SDK_DIR/env.sh" > "$_test_output_file" 2>&1
output=$(cat "$_test_output_file")
assert_contains "$output" "GCC toolchain" "显示 GCC 版本信息"
assert_contains "$output" "listenai-tools" "显示 listenai-tools 版本信息"
assert_contains "$output" "环境就绪" "环境就绪"
assert_env "ARCS_BASE" "$SDK_DIR" "ARCS_BASE 已设置"
teardown_test

# ── 场景 2: 环境变量未设，~/.listenai 下有可用工具 ──
echo ""
echo "── 场景 2: 环境变量未设，~/.listenai 下有可用工具 ──"
setup_test
create_fake_gcc "$HOME/.listenai/gcc"
create_fake_cmake "$HOME/.listenai/listenai-tools"
source "$SDK_DIR/env.sh" > "$TEST_TMPDIR/output.txt" 2>&1; output=$(cat "$TEST_TMPDIR/output.txt")
assert_contains "$output" "GCC toolchain" "自动发现并显示 GCC"
assert_contains "$output" "listenai-tools" "自动发现并显示 listenai-tools"
assert_contains "$output" "环境就绪" "环境就绪"
assert_env_set "NUCLEI_TOOLCHAIN_PATH" "NUCLEI_TOOLCHAIN_PATH 已设置"
assert_env_set "LISTENAI_TOOLS_PATH" "LISTENAI_TOOLS_PATH 已设置"
teardown_test

# ── 场景 3: 环境变量未设，~/.listenai 不存在，父目录有 listenai-dev-tools ──
echo ""
echo "── 场景 3: 父目录有 listenai-dev-tools，路径传递验证 ──"
setup_test
# 在 SDK 的父目录创建 listenai-dev-tools（模拟旧路径结构）
# 注意：SDK_DIR 的父目录可能是真实目录，所以在临时目录模拟
# 这个场景需要 SDK_DIR 的父目录有 listenai-dev-tools，但我们不能修改真实目录
# 所以我们创建一个有部分工具的 ~/.listenai 来触发安装逻辑
mkdir -p "$HOME/.listenai"
# 只有 gcc，没有 listenai-tools → 触发安装 listenai-tools
create_fake_gcc "$HOME/.listenai/gcc"
# mock prepare 脚本（成功安装 listenai-tools 到传入路径）
create_mock_prepare_success "$SDK_DIR"
swap_to_mock "$SDK_DIR"
source "$SDK_DIR/env.sh" > "$TEST_TMPDIR/output.txt" 2>&1; output=$(cat "$TEST_TMPDIR/output.txt")
assert_contains "$output" "GCC toolchain" "已有的 GCC 被识别"
# 关键验证：listenai-tools 应该被安装到 ~/.listenai/listenai-tools（而非硬编码路径）
assert_contains "$output" "listenai-tools 安装完成\|listenai-tools.*CMake" "listenai-tools 安装或识别成功"
assert_env_set "LISTENAI_TOOLS_PATH" "LISTENAI_TOOLS_PATH 已设置"
# 验证安装到了正确的路径
if [ -f "$HOME/.listenai/listenai-tools/cmake/bin/cmake" ]; then
    echo -e "  \033[32m[PASS]\033[0m listenai-tools 安装到 ~/.listenai（路径一致）"
    PASS=$((PASS + 1))
else
    echo -e "  \033[31m[FAIL]\033[0m listenai-tools 未安装到 ~/.listenai"
    FAIL=$((FAIL + 1))
fi
restore_real "$SDK_DIR"
teardown_test

# ── 场景 3b: PATH 中已有 GCC，仅缺 listenai-tools ──
echo ""
echo "── 场景 3b: PATH 中已有 GCC，仅安装 listenai-tools ──"
setup_test
create_fake_gcc "$TEST_TMPDIR/path-gcc"
old_path="$PATH"
export PATH="$TEST_TMPDIR/path-gcc/bin:$PATH"
create_mock_prepare_success "$SDK_DIR"
cat > "$SDK_DIR/tools/scripts/prepare_toolchain.sh.bak" << 'MOCK'
#!/bin/bash
echo "不应下载 GCC" >&2
exit 1
MOCK
chmod +x "$SDK_DIR/tools/scripts/prepare_toolchain.sh.bak"
swap_to_mock "$SDK_DIR"
source "$SDK_DIR/env.sh" > "$TEST_TMPDIR/output.txt" 2>&1; output=$(cat "$TEST_TMPDIR/output.txt")
assert_contains "$output" "GCC 工具链已存在，跳过" "PATH 中的 GCC 触发跳过下载"
assert_not_contains "$output" "GCC 工具链安装失败" "缺 listenai-tools 时不重新安装已有 GCC"
assert_env "NUCLEI_TOOLCHAIN_PATH" "$TEST_TMPDIR/path-gcc" "NUCLEI_TOOLCHAIN_PATH 保持 PATH GCC"
assert_env_set "LISTENAI_TOOLS_PATH" "缺失的 listenai-tools 已安装"
restore_real "$SDK_DIR"
export PATH="$old_path"
teardown_test

# ── 场景 4: 损坏的二进制文件（下载中断） ──
echo ""
echo "── 场景 4: 损坏的二进制文件应触发重新安装 ──"
setup_test
mkdir -p "$HOME/.listenai"
create_corrupted_gcc "$HOME/.listenai/gcc"
create_corrupted_cmake "$HOME/.listenai/listenai-tools"
create_mock_prepare_success "$SDK_DIR"
swap_to_mock "$SDK_DIR"
source "$SDK_DIR/env.sh" > "$TEST_TMPDIR/output.txt" 2>&1; output=$(cat "$TEST_TMPDIR/output.txt")
# 损坏的文件应该被识别为无效，触发重新安装
assert_contains "$output" "正在下载 GCC\|GCC 工具链安装完成" "损坏的 GCC 触发重新安装"
assert_contains "$output" "正在下载 listenai-tools\|listenai-tools 安装完成" "损坏的 listenai-tools 触发重新安装"
assert_contains "$output" "环境就绪" "重新安装后环境就绪"
restore_real "$SDK_DIR"
teardown_test

# ── 场景 5: 安装失败（网络问题） ──
echo ""
echo "── 场景 5: 安装失败应正确报错 ──"
setup_test
mkdir -p "$HOME/.listenai"
create_mock_prepare_fail "$SDK_DIR"
swap_to_mock "$SDK_DIR"
source "$SDK_DIR/env.sh" > "$TEST_TMPDIR/output.txt" 2>&1; output=$(cat "$TEST_TMPDIR/output.txt")
assert_contains "$output" "GCC 工具链安装失败" "GCC 安装失败有提示"
assert_contains "$output" "listenai-tools 安装失败" "listenai-tools 安装失败有提示（独立于 GCC）"
assert_not_contains "$output" "环境就绪" "安装失败不显示环境就绪"
restore_real "$SDK_DIR"
teardown_test

# ── 场景 6: setup 命令强制重新安装 ──
echo ""
echo "── 场景 6: setup 强制重新安装（即使工具已存在） ──"
setup_test
mkdir -p "$HOME/.listenai"
create_fake_gcc "$HOME/.listenai/gcc"
create_fake_cmake "$HOME/.listenai/listenai-tools"
create_mock_prepare_success "$SDK_DIR"
swap_to_mock "$SDK_DIR"
source "$SDK_DIR/env.sh" setup > "$TEST_TMPDIR/output.txt" 2>&1; output=$(cat "$TEST_TMPDIR/output.txt")
# setup 应该触发重新安装，而非"已存在，跳过"
assert_contains "$output" "正在下载 GCC\|GCC 工具链安装完成" "setup 强制重新安装 GCC"
assert_contains "$output" "正在下载 listenai-tools\|listenai-tools 安装完成" "setup 强制重新安装 listenai-tools"
assert_contains "$output" "工具链安装完成" "setup 安装成功提示"
restore_real "$SDK_DIR"
teardown_test

# ── 场景 7: setup 失败不应打印"安装完成" ──
echo ""
echo "── 场景 7: setup 安装失败应提示错误 ──"
setup_test
mkdir -p "$HOME/.listenai"
create_mock_prepare_fail "$SDK_DIR"
swap_to_mock "$SDK_DIR"
source "$SDK_DIR/env.sh" setup > "$TEST_TMPDIR/output.txt" 2>&1; output=$(cat "$TEST_TMPDIR/output.txt")
assert_not_contains "$output" "工具链安装完成，环境变量已设置" "失败时不显示安装完成"
assert_contains "$output" "部分工具安装失败" "显示安装失败提示"
restore_real "$SDK_DIR"
teardown_test

# ── 场景 8: check 命令能检测损坏的工具 ──
echo ""
echo "── 场景 8: check 命令检测损坏工具 ──"
setup_test
mkdir -p "$HOME/.listenai"
create_corrupted_gcc "$HOME/.listenai/gcc"
create_fake_cmake "$HOME/.listenai/listenai-tools"
source "$SDK_DIR/env.sh" check > "$TEST_TMPDIR/output.txt" 2>&1; output=$(cat "$TEST_TMPDIR/output.txt")
assert_contains "$output" "GCC toolchain.*未找到\|MISS.*GCC" "check 识别损坏的 GCC"
assert_contains "$output" "CMake" "check 识别正常的 cmake"
teardown_test

# ── 场景 9: bzip2 未安装时给出清晰错误 ──
echo ""
echo "── 场景 9: bzip2 未安装时报错清晰 ──"
setup_test
FAKE_BIN="$TEST_TMPDIR/fakebin"
mkdir -p "$FAKE_BIN" "$HOME/.listenai"
# fake wget（正常），不创建 bzip2 模拟其缺失
cat > "$FAKE_BIN/wget" << 'FAKEWGET'
#!/bin/bash
echo "fake wget ok"
FAKEWGET
chmod +x "$FAKE_BIN/wget"
# 用绝对路径调用 bash，避免 PATH 修改影响 bash 自身
output=$(ARCS_TEST_HOST_OS=Linux PATH="$FAKE_BIN" /bin/bash "$SDK_DIR/tools/scripts/prepare_toolchain.sh" "$HOME/.listenai" 2>&1)
exit_code=$?
assert_contains "$output" "bzip2" "bzip2 缺失时报错包含 bzip2"
assert_contains "$output" "sudo apt install" "bzip2 缺失时给出安装命令"
if [ $exit_code -ne 0 ]; then
    echo -e "  \033[32m[PASS]\033[0m bzip2 缺失时退出码非 0"
    PASS=$((PASS + 1))
else
    echo -e "  \033[31m[FAIL]\033[0m bzip2 缺失时应退出码非 0，实际为 0"
    FAIL=$((FAIL + 1))
fi
teardown_test

# ── 场景 10: wget 未安装时给出清晰错误 ──
echo ""
echo "── 场景 10: wget 未安装时报错清晰 ──"
setup_test
EMPTY_BIN="$TEST_TMPDIR/empty_bin"
mkdir -p "$EMPTY_BIN" "$HOME/.listenai"
# 用绝对路径调用 bash，只是 PATH 里不含 wget
output_tc=$(ARCS_TEST_HOST_OS=Linux PATH="$EMPTY_BIN" /bin/bash "$SDK_DIR/tools/scripts/prepare_toolchain.sh" "$HOME/.listenai" 2>&1)
exit_tc=$?
output_lt=$(ARCS_TEST_HOST_OS=Linux PATH="$EMPTY_BIN" /bin/bash "$SDK_DIR/tools/scripts/prepare_listenai_tools.sh" "$HOME/.listenai" 2>&1)
exit_lt=$?
assert_contains "$output_tc" "wget" "toolchain: wget 缺失时报错包含 wget"
assert_contains "$output_tc" "sudo apt install" "toolchain: wget 缺失时给出安装命令"
assert_contains "$output_lt" "wget" "listenai-tools: wget 缺失时报错包含 wget"
assert_contains "$output_lt" "sudo apt install" "listenai-tools: wget 缺失时给出安装命令"
if [ $exit_tc -ne 0 ] && [ $exit_lt -ne 0 ]; then
    echo -e "  \033[32m[PASS]\033[0m wget 缺失时两个脚本均退出码非 0"
    PASS=$((PASS + 1))
else
    echo -e "  \033[31m[FAIL]\033[0m wget 缺失时脚本退出码应非 0"
    FAIL=$((FAIL + 1))
fi
teardown_test

# ── 场景 11: 下载中断后不残留 tar 文件 ──
echo ""
echo "── 场景 11: 下载失败后不残留不完整的 tar 文件 ──"
setup_test
FAKE_BIN2="$TEST_TMPDIR/fakebin2"
mkdir -p "$FAKE_BIN2" "$HOME/.listenai"
# fake wget：写入部分内容后失败
cat > "$FAKE_BIN2/wget" << 'FAKEWGET'
#!/bin/bash
OUTFILE=""
while [[ $# -gt 0 ]]; do
    if [ "$1" = "-O" ]; then OUTFILE="$2"; shift 2; else shift; fi
done
[ -n "$OUTFILE" ] && echo "PARTIAL_DATA" > "$OUTFILE"
exit 1
FAKEWGET
chmod +x "$FAKE_BIN2/wget"
# fake bzip2（让依赖检查通过）
cat > "$FAKE_BIN2/bzip2" << 'FAKEBZIP'
#!/bin/bash
echo "fake bzip2"
FAKEBZIP
chmod +x "$FAKE_BIN2/bzip2"
ARCS_TEST_HOST_OS=Linux PATH="$FAKE_BIN2" /bin/bash "$SDK_DIR/tools/scripts/prepare_toolchain.sh" "$HOME/.listenai" 2>/dev/null || true
ARCS_TEST_HOST_OS=Linux PATH="$FAKE_BIN2" /bin/bash "$SDK_DIR/tools/scripts/prepare_listenai_tools.sh" "$HOME/.listenai" 2>/dev/null || true
if [ ! -f "$HOME/.listenai/nuclei-toolchain.tar.bz2" ]; then
    echo -e "  \033[32m[PASS]\033[0m 下载失败后不残留 nuclei-toolchain.tar.bz2"
    PASS=$((PASS + 1))
else
    echo -e "  \033[31m[FAIL]\033[0m 下载失败后残留了 nuclei-toolchain.tar.bz2"
    FAIL=$((FAIL + 1))
fi
if [ ! -f "$HOME/.listenai/listenai-tools.tar.gz" ]; then
    echo -e "  \033[32m[PASS]\033[0m 下载失败后不残留 listenai-tools.tar.gz"
    PASS=$((PASS + 1))
else
    echo -e "  \033[31m[FAIL]\033[0m 下载失败后残留了 listenai-tools.tar.gz"
    FAIL=$((FAIL + 1))
fi
teardown_test

# ── 场景 12: macOS arm64 GCC 使用专用下载包 ──
echo ""
echo "── 场景 12: macOS arm64 GCC 使用专用下载包 ──"
setup_test
FAKE_BIN_MAC_TC="$TEST_TMPDIR/fake_bin_mac_tc"
FAKE_TAR="$TEST_TMPDIR/fake-toolchain.tar.gz"
FAKE_ROOT="$TEST_TMPDIR/fake-root/riscv-gnu-toolchain-macos-arm64-nuclei-2025.02-r1"
mkdir -p "$FAKE_BIN_MAC_TC" "$FAKE_ROOT/bin"
cat > "$FAKE_ROOT/bin/riscv64-unknown-elf-gcc" << 'FAKEGCC'
#!/bin/bash
echo "riscv64-unknown-elf-gcc (fake mac arm64) 14.2.1"
FAKEGCC
chmod +x "$FAKE_ROOT/bin/riscv64-unknown-elf-gcc"
tar -czf "$FAKE_TAR" -C "$TEST_TMPDIR/fake-root" "riscv-gnu-toolchain-macos-arm64-nuclei-2025.02-r1"
cat > "$FAKE_BIN_MAC_TC/curl" << 'FAKECURL'
#!/bin/bash
out=""
url=""
while [ $# -gt 0 ]; do
    case "$1" in
        -o) out="$2"; shift 2 ;;
        http*) url="$1"; shift ;;
        *) shift ;;
    esac
done
echo "$url" > "$ARCS_TEST_URL_LOG"
cp "$ARCS_TEST_FAKE_TAR" "$out"
FAKECURL
chmod +x "$FAKE_BIN_MAC_TC/curl"
output=$(ARCS_TEST_HOST_OS=Darwin ARCS_TEST_HOST_ARCH=arm64 ARCS_TEST_FAKE_TAR="$FAKE_TAR" \
    ARCS_TEST_URL_LOG="$TEST_TMPDIR/url.log" PATH="$FAKE_BIN_MAC_TC:/usr/bin:/bin" \
    /bin/bash "$SDK_DIR/tools/scripts/prepare_toolchain.sh" "$HOME/.listenai" 2>&1)
exit_code=$?
assert_contains "$(cat "$TEST_TMPDIR/url.log")" "mac_arm64/riscv-gnu-toolchain-macos-arm64-nuclei-2025.02-r1.tar.gz" "macOS arm64 URL 拼接正确"
if [ $exit_code -eq 0 ] && [ -x "$HOME/.listenai/gcc/bin/riscv64-unknown-elf-gcc" ]; then
    echo -e "  \033[32m[PASS]\033[0m macOS arm64 GCC 解压到 ~/.listenai/gcc"
    PASS=$((PASS + 1))
else
    echo -e "  \033[31m[FAIL]\033[0m macOS arm64 GCC 未正确安装"
    echo "         输出: $output"
    FAIL=$((FAIL + 1))
fi
teardown_test

# ── 场景 13: macOS arm64 listenai-tools 下载现成包 ──
echo ""
echo "── 场景 13: macOS arm64 listenai-tools 下载现成包 ──"
setup_test
FAKE_BIN_MAC_LT="$TEST_TMPDIR/fake_bin_mac_lt"
FAKE_TOOLS_TAR="$TEST_TMPDIR/fake-listenai-tools.tar.gz"
FAKE_TOOLS_ROOT="$TEST_TMPDIR/fake-tools-root/listenai-tools"
mkdir -p "$FAKE_BIN_MAC_LT" "$FAKE_TOOLS_ROOT/cmake/bin"
cat > "$FAKE_TOOLS_ROOT/cmake/bin/cmake" << 'FAKECMAKE'
#!/bin/bash
echo "cmake version 3.31.0"
FAKECMAKE
chmod +x "$FAKE_TOOLS_ROOT/cmake/bin/cmake"
tar -czf "$FAKE_TOOLS_TAR" -C "$TEST_TMPDIR/fake-tools-root" "listenai-tools"
cat > "$FAKE_BIN_MAC_LT/wget" << 'FAKEWGET'
#!/bin/bash
out=""
url=""
while [ $# -gt 0 ]; do
    case "$1" in
        -O) out="$2"; shift 2 ;;
        http*) url="$1"; shift ;;
        *) shift ;;
    esac
done
echo "$url" > "$ARCS_TEST_URL_LOG"
cp "$ARCS_TEST_FAKE_TAR" "$out"
FAKEWGET
chmod +x "$FAKE_BIN_MAC_LT/wget"
cat > "$FAKE_BIN_MAC_LT/curl" << 'FAKECURL'
#!/bin/bash
out=""
url=""
while [ $# -gt 0 ]; do
    case "$1" in
        -o) out="$2"; shift 2 ;;
        http*) url="$1"; shift ;;
        *) shift ;;
    esac
done
echo "$url" > "$ARCS_TEST_URL_LOG"
cp "$ARCS_TEST_FAKE_TAR" "$out"
FAKECURL
chmod +x "$FAKE_BIN_MAC_LT/curl"
output=$(ARCS_TEST_HOST_OS=Darwin ARCS_TEST_HOST_ARCH=arm64 ARCS_TEST_FAKE_TAR="$FAKE_TOOLS_TAR" \
    ARCS_TEST_URL_LOG="$TEST_TMPDIR/listenai-tools-url.log" PATH="$FAKE_BIN_MAC_LT:/usr/bin:/bin" \
    /bin/bash "$SDK_DIR/tools/scripts/prepare_listenai_tools.sh" "$HOME/.listenai" 2>&1)
exit_code=$?
assert_contains "$(cat "$TEST_TMPDIR/listenai-tools-url.log")" "dev-tools/mac_arm64/v0.0.1/listenai-tools.tar.gz" "macOS arm64 listenai-tools URL 拼接正确"
if [ $exit_code -eq 0 ] && [ -x "$HOME/.listenai/listenai-tools/cmake/bin/cmake" ]; then
    echo -e "  \033[32m[PASS]\033[0m macOS arm64 listenai-tools 解压到 ~/.listenai/listenai-tools"
    PASS=$((PASS + 1))
else
    echo -e "  \033[31m[FAIL]\033[0m macOS arm64 listenai-tools 未正确安装"
    echo "         输出: $output"
    FAIL=$((FAIL + 1))
fi
teardown_test

# ── 结果汇总 ──
echo ""
echo "========================================"
echo " 测试结果: $PASS 通过, $FAIL 失败"
echo "========================================"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
