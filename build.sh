#!/bin/bash

set -e

usage() {
    echo "使用方式: $0 [选项]"
    echo "选项:"
    echo "  -S, --Source <path>    指定项目源码路径 (默认为当前脚本所在目录)"
    echo "  -t, --target <target>  指定构建目标 (如 menuconfig)"
    echo "  -C, --Clean            清理构建目录"
    echo "  -B, --build            构建输出目录"
    echo "  -h, --help             显示此帮助信息"
    echo "  -w, --warnings-as-errors 将警告视为错误"
    echo ""
    echo "示例:"
    echo "  $0                                   默认构建"
    echo "  $0 -S samples/hello-world                指定源码目录构建"
    echo "  $0 -t menuconfig                     运行menuconfig"
    echo "  $0 -C                                清理并重新构建"
    echo "  $0 -S samples/hello-world -t menuconfig  指定源码目录并运行menuconfig"
    exit 1
}

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_PATH="$SCRIPT_DIR"
TARGET=""
CLEAN=false
OUTPUT="build"
WARNINGS_AS_ERRORS=false
ARCS_BASE_DIR_NAME="arcs-sdk"
ARCS_DEV_TOOLS_DIR_NAME="listenai-dev-tools"
ARCS_DEV_TOOL_TOOLCHAIN_DIR_NAME="gcc"
ARCS_DEV_TOOL_LISTENAI_TOOLS_DIR_NAME="listenai-tools"

find_arcs_base() {
    local current_dir=$(cd "$(dirname "$0")" && pwd)
    local dir_name="$ARCS_BASE_DIR_NAME"

    while [ "$current_dir" != "/" ]; do
        if [ -d "$current_dir/$dir_name" ]; then
            echo "Found ARCS_BASE: $current_dir/$dir_name"
            export ARCS_BASE="$current_dir/$dir_name"
            return 0
        fi
        current_dir=$(dirname "$current_dir")
    done

    echo "ARCS_BASE not found, Please add ARCS_BASE environment variable or set ARCS_BASE_DIR_NAME to the correct directory."
    echo "Current Target ARCS_BASE directory name: $ARCS_BASE_DIR_NAME."
    exit 1
}

find_dev_tools() {
    local current_dir=$(cd "$(dirname "$0")" && pwd)
    local dir_name="$ARCS_DEV_TOOLS_DIR_NAME"

    echo "trying to find $dir_name in parent directories..."

    while [ "$current_dir" != "/" ]; do
        if [ -d "$current_dir/$dir_name" ]; then
            echo "Found $dir_name: $current_dir/$dir_name"
            if [ -d "$current_dir/$dir_name/$ARCS_DEV_TOOL_LISTENAI_TOOLS_DIR_NAME" ]; then
                echo "Found LISTENAI_TOOLS_PATH: $current_dir/$dir_name/$ARCS_DEV_TOOL_LISTENAI_TOOLS_DIR_NAME"
                export LISTENAI_TOOLS_PATH="$current_dir/$dir_name/$ARCS_DEV_TOOL_LISTENAI_TOOLS_DIR_NAME"
            fi
            if [ -d "$current_dir/$dir_name/$ARCS_DEV_TOOL_TOOLCHAIN_DIR_NAME" ]; then
                echo "Found NUCLEI_TOOLCHAIN_PATH: $current_dir/$dir_name/$ARCS_DEV_TOOL_TOOLCHAIN_DIR_NAME"
                export NUCLEI_TOOLCHAIN_PATH="$current_dir/$dir_name/$ARCS_DEV_TOOL_TOOLCHAIN_DIR_NAME"
            fi
            return 0
        fi
        current_dir=$(dirname "$current_dir")
    done
}

while [[ $# -gt 0 ]]; do
  case $1 in
    -S|--Source)
      PROJECT_PATH="$2"
      shift 2
      ;;
    -B|--build)
      OUTPUT="$2"
      shift 2
      ;;
    -t|--target)
      TARGET="$2"
      shift 2
      ;;
    -C|--Clean)
      CLEAN=true
      shift 1
      ;;
    -w|--warnings-as-errors)
      WARNINGS_AS_ERRORS=true
      shift 1
      ;;
    -h|--help)
      usage
      ;;
    *)
      echo "未知参数: $1"
      usage
      ;;
  esac
done

echo "Source: $PROJECT_PATH"
echo "Target: $TARGET"
echo "Clean : $CLEAN"

if [ -z "$LISTENAI_TOOLS_PATH" ] || [ -z "$NUCLEI_TOOLCHAIN_PATH" ]; then
    find_dev_tools
fi

if [ -z "${LISTENAI_TOOLS_PATH}" ]; then
    export LISTENAI_TOOLS_PATH="请添加 LISTENAI_TOOLS_PATH 环境变量,或在此行设置正确的路径"
    echo "请添加 LISTENAI_TOOLS_PATH 环境变量或者修改脚本后, 注释脚本第 $LINENO 行";exit 1;
fi

if [ -z "${NUCLEI_TOOLCHAIN_PATH}" ]; then
    export NUCLEI_TOOLCHAIN_PATH="请添加 NUCLEI_TOOLCHAIN_PATH 环境变量,或在此行设置正确的路径"
    echo "请添加 NUCLEI_TOOLCHAIN_PATH 环境变量或者修改脚本后, 注释脚本第 $LINENO 行";exit 1;
fi

############### 下面代码不用修改 ##################

# 构建工具的位置
CMAKE_PROGRAM="$LISTENAI_TOOLS_PATH/cmake/bin/cmake"
NINJA_PROGRAM="$LISTENAI_TOOLS_PATH/ninja/ninja"


# 配置环境变量 ARCS_BASE
if [ -z "$ARCS_BASE" ]; then
    find_arcs_base
fi

if [ "$CLEAN" = true ]; then
    rm -rf $OUTPUT
fi 

# Initialize CMAKE_VARS array if it doesn't exist
declare -a CMAKE_VARS

# Add warnings-as-errors flag if enabled
if [ "$WARNINGS_AS_ERRORS" = true ]; then
    CMAKE_VARS+=("-DCMAKE_C_FLAGS=-Werror")
    CMAKE_VARS+=("-DCMAKE_CXX_FLAGS=-Werror")
    echo "Treating warnings as errors"
fi

$CMAKE_PROGRAM -B "$OUTPUT" -G Ninja -S "$PROJECT_PATH" \
    -DCMAKE_MAKE_PROGRAM="$NINJA_PROGRAM" \
    "${CMAKE_VARS[@]}"

if [ -z "$TARGET" ]; then
    $CMAKE_PROGRAM --build "$OUTPUT" -j4
else
    $CMAKE_PROGRAM --build "$OUTPUT" --target "$TARGET"
fi
