#!/bin/bash
# 此脚本用于修改跟目录的build.sh后
# 自动将build.sh复制到各个项目目录

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
directories=()
while IFS= read -r cmake_file; do
    if grep -q "find_package(listenai-cmake" "$cmake_file"; then
        project_dir=$(dirname "$cmake_file")
        directories+=("$project_dir")
    fi
done < <(find "${SCRIPT_DIR}" -type f -name "CMakeLists.txt")
for dir in "${directories[@]}"; do
    cp $SCRIPT_DIR/build.sh $dir
done
