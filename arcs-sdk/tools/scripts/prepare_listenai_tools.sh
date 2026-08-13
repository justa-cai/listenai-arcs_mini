#!/bin/bash
# 用法: bash prepare_listenai_tools.sh [安装目录] [版本号]
# 安装目录默认为 ~/.listenai，版本号默认为 v0.0.1

set -e

HOST_OS="${ARCS_TEST_HOST_OS:-$(/usr/bin/uname -s 2>/dev/null || uname -s)}"
HOST_ARCH="${ARCS_TEST_HOST_ARCH:-$(/usr/bin/uname -m 2>/dev/null || uname -m)}"
INSTALL_DIR="${1:-${HOME}/.listenai}"
LISTENAI_TOOLS_VERSION="${2:-v0.0.1}"

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

# ─── 依赖检查 ──────────────────────────────────────────────────────────────────
if ! command -v wget &>/dev/null && ! command -v curl &>/dev/null; then
    if [ "$HOST_OS" = "Darwin" ]; then
        echo "错误: 未找到 wget 或 curl，请先安装: brew install wget" >&2
    else
        echo "错误: 未找到 wget 或 curl，请先安装: sudo apt install wget" >&2
    fi
    exit 1
fi

if [ "$HOST_OS" = "Darwin" ]; then
    if [ "$HOST_ARCH" != "arm64" ]; then
        echo "错误: 当前仅配置了 macOS arm64 listenai-tools，当前架构: ${HOST_ARCH}" >&2
        exit 1
    fi
    TOOLS_PLATFORM="mac_arm64"
else
    TOOLS_PLATFORM="linux-amd64"
fi

URL_BASE=https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/
LISTENAI_TOOLS_URL=${URL_BASE}dev-tools/${TOOLS_PLATFORM}/${LISTENAI_TOOLS_VERSION}/listenai-tools.tar.gz
TMP_FILE="${INSTALL_DIR}/listenai-tools.tar.gz"

mkdir -p "${INSTALL_DIR}"
trap 'rm -f "${TMP_FILE}"' ERR
download_file "${LISTENAI_TOOLS_URL}" "${TMP_FILE}"
tar -xzf "${TMP_FILE}" -C "${INSTALL_DIR}"
rm -f "${TMP_FILE}"
