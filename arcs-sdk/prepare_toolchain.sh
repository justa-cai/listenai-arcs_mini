#!/bin/bash

set -e

TOOLCHAIN_URL=https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/toolchain/linux-amd64/nuclei_riscv_newlibc_prebuilt_linux64_2025.02.tar.bz2

DOWNLOAD_DIR=listenai-dev-tools

mkdir -p ${DOWNLOAD_DIR}
wget -O ${DOWNLOAD_DIR}/nuclei-toolchain.tar.bz2 ${TOOLCHAIN_URL}
tar -xvjf ${DOWNLOAD_DIR}/nuclei-toolchain.tar.bz2 -C ${DOWNLOAD_DIR}
rm -rf ${DOWNLOAD_DIR}/nuclei-toolchain.tar.bz2
