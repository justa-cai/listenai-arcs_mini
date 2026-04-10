#!/bin/bash
# 用法: bash prepare_listenai_tools.sh [安装目录]
# 安装目录默认为 ~/.listenai

set -e

# ─── 依赖检查 ──────────────────────────────────────────────────────────────────
if ! command -v wget &>/dev/null; then
    echo "错误: 未找到 wget，请先安装: sudo apt install wget" >&2
    exit 1
fi

URL_BASE=http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/
LISTENAI_TOOLS_VERSION=v0.0.1
LISTENAI_TOOLS_URL=${URL_BASE}dev-tools/linux-amd64/${LISTENAI_TOOLS_VERSION}/listenai-tools.tar.gz

INSTALL_DIR="${1:-${HOME}/.listenai}"

mkdir -p "${INSTALL_DIR}"
trap 'rm -f "${INSTALL_DIR}/listenai-tools.tar.gz"' ERR
wget -O "${INSTALL_DIR}/listenai-tools.tar.gz" ${LISTENAI_TOOLS_URL}
tar -xzf "${INSTALL_DIR}/listenai-tools.tar.gz" -C "${INSTALL_DIR}"
rm -f "${INSTALL_DIR}/listenai-tools.tar.gz"
