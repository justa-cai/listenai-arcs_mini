#!/bin/bash

set -e

URL_BASE=http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/
LISTENAI_TOOLS_VERSION=v0.0.1
LISTENAI_TOOLS_URL=${URL_BASE}dev-tools/linux-amd64/${LISTENAI_TOOLS_VERSION}/listenai-tools.tar.gz

DOWNLOAD_DIR=listenai-dev-tools

mkdir -p ${DOWNLOAD_DIR}
wget -O ${DOWNLOAD_DIR}/listenai-tools.tar.gz ${LISTENAI_TOOLS_URL}
tar -xzf ${DOWNLOAD_DIR}/listenai-tools.tar.gz -C ${DOWNLOAD_DIR}
rm -rf ${DOWNLOAD_DIR}/listenai-tools.tar.gz
