#!/bin/bash
# ARCS SDK Zig Adapter 构建验证脚本
# 用法: bash .agents/skills/arcs-zig-dev/scripts/validate_zig_build.sh
#
# 验证项:
# 1. Zig 编译器可用
# 2. zig build 交叉编译成功
# 3. zig build test 主机测试通过
# 4. 静态库生成正确

set -e

SDK_ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
ZIG_DIR="$SDK_ROOT/labs/zig/adapter"

echo "=== ARCS SDK Zig Adapter Build Validation ==="
echo "SDK Root: $SDK_ROOT"
echo "Zig Dir:  $ZIG_DIR"
echo ""

# 1. 检查 Zig 编译器
if ! command -v zig &>/dev/null; then
    echo "ERROR: Zig compiler not found in PATH"
    echo "Install from: https://ziglang.org/download/"
    exit 1
fi
echo "[✓] Zig compiler: $(zig version)"

# 2. 交叉编译
echo ""
echo "--- Cross-compile (RISC-V 64) ---"
cd "$ZIG_DIR"
if zig build --summary all 2>&1; then
    echo "[✓] Cross-compile succeeded"
else
    echo "[✗] Cross-compile FAILED"
    exit 1
fi

# 3. 检查静态库
if [ -f "zig-out/lib/libarcs-zig.a" ]; then
    LIB_SIZE=$(stat -c%s "zig-out/lib/libarcs-zig.a" 2>/dev/null || stat -f%z "zig-out/lib/libarcs-zig.a" 2>/dev/null)
    echo "[✓] Static library: zig-out/lib/libarcs-zig.a (${LIB_SIZE} bytes)"
else
    echo "[✗] Static library not found"
    exit 1
fi

# 4. 主机测试
echo ""
echo "--- Host tests ---"
if zig build test --summary all 2>&1; then
    echo "[✓] All tests passed"
else
    echo "[✗] Tests FAILED"
    exit 1
fi

# 5. 文件完整性检查
echo ""
echo "--- File integrity ---"
MISSING=0
for f in src/root.zig build.zig build.zig.zon CMakeLists.txt; do
    if [ ! -f "$ZIG_DIR/$f" ]; then
        echo "[✗] Missing: $f"
        MISSING=$((MISSING + 1))
    fi
done

BINDINGS_COUNT=$(find "$ZIG_DIR/src/bindings" -name '*.zig' | wc -l)
HAL_COUNT=$(find "$ZIG_DIR/src/hal" -name '*.zig' | wc -l)
EXAMPLE_COUNT=$(find "$ZIG_DIR/examples" -name '*.zig' | wc -l)
TEST_COUNT=$(find "$ZIG_DIR/tests" -name '*.zig' | wc -l)

echo "[✓] Bindings: ${BINDINGS_COUNT} modules"
echo "[✓] HAL:      ${HAL_COUNT} modules"
echo "[✓] Examples: ${EXAMPLE_COUNT} files"
echo "[✓] Tests:    ${TEST_COUNT} files"

if [ $MISSING -gt 0 ]; then
    echo ""
    echo "RESULT: FAILED ($MISSING files missing)"
    exit 1
fi

echo ""
echo "=== ALL CHECKS PASSED ==="
