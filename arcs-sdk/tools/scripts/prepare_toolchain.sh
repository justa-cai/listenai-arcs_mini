#!/bin/bash
# 用法: bash prepare_toolchain.sh [安装目录] [版本号]
# 安装目录默认为 ~/.listenai，版本号默认为 2025.02

set -e

HOST_OS="${ARCS_TEST_HOST_OS:-$(/usr/bin/uname -s 2>/dev/null || uname -s)}"
HOST_ARCH="${ARCS_TEST_HOST_ARCH:-$(/usr/bin/uname -m 2>/dev/null || uname -m)}"
INSTALL_DIR="${1:-${HOME}/.listenai}"
TOOLCHAIN_VERSION="${2:-2025.02}"
TOOLCHAIN_PATH="${INSTALL_DIR}/gcc"

download_file() {
    local url="$1"
    local output="$2"

    if command -v wget &>/dev/null; then
        wget -O "$output" "$url"
    elif command -v curl &>/dev/null; then
        curl -L --fail -o "$output" "$url"
    else
        echo "错误: 未找到 wget 或 curl" >&2
        if [ "$HOST_OS" = "Darwin" ]; then
            echo "请先安装: brew install wget" >&2
        else
            echo "请先安装: sudo apt install wget" >&2
        fi
        return 1
    fi
}

prepare_macos_toolchain() {
    if [ "$HOST_ARCH" != "arm64" ]; then
        echo "错误: 当前仅配置了 macOS arm64 工具链，当前架构: ${HOST_ARCH}" >&2
        return 1
    fi

    local toolchain_url="https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/toolchain/mac_arm64/riscv-gnu-toolchain-macos-arm64-nuclei-${TOOLCHAIN_VERSION}-r1.tar.gz"
    local tmp_file="${INSTALL_DIR}/riscv-gnu-toolchain-macos-arm64.tar.gz"
    local tmp_dir="${INSTALL_DIR}/.gcc-macos-unpack"

    mkdir -p "${INSTALL_DIR}" "$tmp_dir"
    trap 'chmod -R u+rwX "${tmp_dir}" 2>/dev/null || true; rm -rf "${tmp_file}" "${tmp_dir}"' ERR EXIT
    echo "下载 macOS arm64 GCC 工具链 ${TOOLCHAIN_VERSION}..."
    download_file "$toolchain_url" "$tmp_file"

    rm -rf "$tmp_dir"
    mkdir -p "$tmp_dir"
    tar -xzf "$tmp_file" -C "$tmp_dir"
    local extracted_root
    extracted_root=$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | head -1)
    if [ -z "$extracted_root" ] || [ ! -x "$extracted_root/bin/riscv64-unknown-elf-gcc" ]; then
        echo "错误: macOS GCC 工具链包结构异常" >&2
        return 1
    fi

    if [ -e "$TOOLCHAIN_PATH" ]; then
        local backup_path="${TOOLCHAIN_PATH}.backup.$(/bin/date +%Y%m%d%H%M%S 2>/dev/null || date +%s)"
        mv "$TOOLCHAIN_PATH" "$backup_path"
        echo "已备份旧工具链: ${backup_path}"
    fi
    mv "$extracted_root" "$TOOLCHAIN_PATH"
    rm -rf "$tmp_file" "$tmp_dir"
    trap - ERR EXIT
    echo "GCC 工具链 ${TOOLCHAIN_VERSION} 安装完成: ${TOOLCHAIN_PATH}"
}

if [ "$HOST_OS" = "Darwin" ]; then
    prepare_macos_toolchain
    exit $?
fi

# ─── 依赖检查 ──────────────────────────────────────────────────────────────────
if ! command -v wget &>/dev/null && ! command -v curl &>/dev/null; then
    echo "错误: 未找到 wget 或 curl，请先安装: sudo apt install wget" >&2
    exit 1
fi
if ! command -v bzip2 &>/dev/null; then
    echo "错误: 未找到 bzip2，请先安装: sudo apt install bzip2" >&2
    exit 1
fi

TOOLCHAIN_URL="https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/toolchain/linux-amd64/nuclei_riscv_newlibc_prebuilt_linux64_${TOOLCHAIN_VERSION}.tar.bz2"
TMP_FILE="${INSTALL_DIR}/nuclei-toolchain.tar.bz2"

mkdir -p "${INSTALL_DIR}"
trap 'rm -f "${TMP_FILE}"' ERR
download_file "${TOOLCHAIN_URL}" "${TMP_FILE}"
tar -xjf "${TMP_FILE}" -C "${INSTALL_DIR}"
rm -f "${TMP_FILE}"
