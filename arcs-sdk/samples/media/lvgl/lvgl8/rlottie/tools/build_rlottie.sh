#!/usr/bin/env bash
# build_rlottie.sh — 为 ARCS/LS26 (rv32imac / ilp32) 目标交叉编译 Samsung rlottie 静态库
#
# 使用方式：
#   ./tools/build_rlottie.sh
#
# 构建产物默认安装到：
#   <本脚本目录>/../rlottie-riscv/
#     ├── include/rlottie_capi.h
#     └── lib/librlottie.a
#
# 构建完成后按提示设置 RLOTTIE_ROOT，再运行 build.sh 即可编译本示例。
#
# 可选环境变量：
#   NUCLEI_TOOLCHAIN_PATH   Nuclei 工具链根目录（默认从 LISTENAI_TOOLS_PATH 推导）
#   RLOTTIE_VERSION         git tag / branch / commit（默认固定 commit）
#   RLOTTIE_INSTALL_DIR     安装目录（默认 <script_dir>/../rlottie-riscv）

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAMPLE_DIR="$(dirname "${SCRIPT_DIR}")"
PATCH_FILE="${SCRIPT_DIR}/rlottie-baremetal.patch"

RLOTTIE_REPO="https://github.com/Samsung/rlottie.git"
RLOTTIE_VERSION="${RLOTTIE_VERSION:-671c561130ead1c6e44805a7ec1263573a3440fd}"
WORK_DIR="${SCRIPT_DIR}/_rlottie_build"
INSTALL_DIR="${RLOTTIE_INSTALL_DIR:-${SAMPLE_DIR}/rlottie-riscv}"
TOOLCHAIN_FILE="${SCRIPT_DIR}/riscv64-unknown-elf.cmake"
SRC_DIR="${WORK_DIR}/src"
BUILD_DIR="${WORK_DIR}/build-ilp32"

echo "=== build_rlottie.sh ==="
echo "版本  : ${RLOTTIE_VERSION}"
echo "安装到: ${INSTALL_DIR}"
echo ""

# ---------- 1. 获取源码 ----------
if [ ! -d "${SRC_DIR}/.git" ]; then
    echo "[1/4] 克隆 Samsung/rlottie (${RLOTTIE_VERSION})..."
    if [[ "${RLOTTIE_VERSION}" =~ ^[0-9a-fA-F]{40}$ ]]; then
        git clone "${RLOTTIE_REPO}" "${SRC_DIR}"
        git -C "${SRC_DIR}" checkout --detach "${RLOTTIE_VERSION}"
    else
        git clone --depth=1 --branch "${RLOTTIE_VERSION}" \
            "${RLOTTIE_REPO}" "${SRC_DIR}" 2>/dev/null || \
        git clone --depth=1 "${RLOTTIE_REPO}" "${SRC_DIR}"
    fi
else
    echo "[1/4] 已有源码，跳过克隆"
    git -C "${SRC_DIR}" checkout --detach "${RLOTTIE_VERSION}"
fi

echo "[1.5/4] 应用裸机兼容补丁..."
if git -C "${SRC_DIR}" apply --reverse --check "${PATCH_FILE}" >/dev/null 2>&1; then
    echo "补丁已存在，跳过"
else
    git -C "${SRC_DIR}" apply "${PATCH_FILE}"
fi

# ---------- 2. CMake 配置 ----------
echo "[2/4] CMake 配置..."
cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DLIB_INSTALL_DIR="${INSTALL_DIR}/lib" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_SHARED_LIBS=OFF \
    -DLOTTIE_THREAD=OFF \
    -DLOTTIE_CACHE=OFF \
    -DLOTTIE_DOTLOTTIE=OFF \
    -DLOTTIE_MODULE=OFF \
    -DBUILD_TESTING=OFF \
    -DLOTTIE_DEMO=OFF \
    ${NUCLEI_TOOLCHAIN_PATH:+-DNUCLEI_TOOLCHAIN_PATH="${NUCLEI_TOOLCHAIN_PATH}"}

# ---------- 3. 编译 ----------
echo "[3/4] 编译..."
cmake --build "${BUILD_DIR}" -j"$(nproc)"

# ---------- 4. 安装 ----------
echo "[4/4] 安装到 ${INSTALL_DIR}..."
cmake --install "${BUILD_DIR}"

echo ""
echo "=== 构建完成 ==="
echo ""
echo "产物："
ls -lh "${INSTALL_DIR}/lib/librlottie.a"   2>/dev/null || true
ls -lh "${INSTALL_DIR}/include/rlottie_capi.h" 2>/dev/null || true
echo ""
echo "下一步 — 构建本示例："
echo ""
echo "  export RLOTTIE_ROOT=\"${INSTALL_DIR}\""
echo "  # 在仓库根目录执行："
echo "  ./build.sh -S ./arcs-sdk/samples/media/lvgl/lvgl8/rlottie -DBOARD=arcs_mini"
echo ""
