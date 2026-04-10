#!/bin/bash
# 用法: bash prepare_toolchain.sh [安装目录]
# 安装目录默认为 ~/.listenai

set -e

# ─── 依赖检查 ──────────────────────────────────────────────────────────────────
if ! command -v wget &>/dev/null; then
    echo "错误: 未找到 wget，请先安装: sudo apt install wget" >&2
    exit 1
fi
if ! command -v bzip2 &>/dev/null; then
    echo "错误: 未找到 bzip2，请先安装: sudo apt install bzip2" >&2
    exit 1
fi

TOOLCHAIN_URL=https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/toolchain/linux-amd64/nuclei_riscv_newlibc_prebuilt_linux64_2025.02.tar.bz2

INSTALL_DIR="${1:-${HOME}/.listenai}"

mkdir -p "${INSTALL_DIR}"
trap 'rm -f "${INSTALL_DIR}/nuclei-toolchain.tar.bz2"' ERR
wget -O "${INSTALL_DIR}/nuclei-toolchain.tar.bz2" ${TOOLCHAIN_URL}
tar -xvjf "${INSTALL_DIR}/nuclei-toolchain.tar.bz2" -C "${INSTALL_DIR}"
rm -f "${INSTALL_DIR}/nuclei-toolchain.tar.bz2"
