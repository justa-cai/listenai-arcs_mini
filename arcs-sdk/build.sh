#!/bin/bash

set -e

usage() {
    echo "使用方式: $0 [选项]"
    echo "选项:"
    echo "  -S, --Source <path>    指定项目源码路径 (默认为当前脚本所在目录)"
    echo "  -t, --target <target>  指定构建目标 (如 menuconfig)"
    echo "  -C, --Clean            清理构建目录"
    echo "  -B, --build            构建输出目录"
    echo "  -j<N>, --jobs <N>      指定并发构建任务数 (默认: 4)"
    echo "  -h, --help             显示此帮助信息"
    echo "  -r, --release          以 Release 模式构建 (移除 DEBUG_PATH 信息)"
    echo "  -w, --warnings-as-errors 将警告视为错误"
    echo "  -v, --verbose          显示详细的编译命令 (ninja -v)"
    echo "  -d, --debug            启用调试模式 (ninja -d explain + 错误诊断)"
    echo "  -G, --generator <type> 指定构建工具 (Ninja 或 Makefile, 默认: Ninja)"
    echo "  -D<var>=<value>        传递 CMake 变量 (可多次使用)"
    echo ""
    echo "示例:"
    echo "  $0 -S samples/helloworld -DBOARD=arcs_mini                      指定板型构建"
    echo "  $0 -S samples/helloworld -DBOARD=arcs_evb                       使用 EVB 板型"
    echo "  $0 -S samples/helloworld -t menuconfig -DBOARD=arcs_mini        运行 menuconfig"
    echo "  $0 -C -S samples/helloworld -DBOARD=arcs_mini                   清理并重新构建"
    echo "  $0 -S samples/helloworld -j1                                    单线程构建（或 -j 1）"
    echo "  $0 -S samples/helloworld -DBOARD=my_board -DBOARD_SEARCH_PATH=/path/to/boards  使用自定义板型"
    exit 1
}

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_PATH="$SCRIPT_DIR"
TARGET=""
CLEAN=false
OUTPUT="build"
JOBS=4
WARNINGS_AS_ERRORS=false
RELEASE=false
VERBOSE=false
DEBUG=false
GENERATOR="Ninja"
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
    -j|--jobs)
      JOBS="$2"
      shift 2
      ;;
    -j*)
      JOBS="${1#-j}"
      shift 1
      ;;
    -C|--Clean)
      CLEAN=true
      shift 1
      ;;
    -w|--warnings-as-errors)
      WARNINGS_AS_ERRORS=true
      shift 1
      ;;
    -v|--verbose)
      VERBOSE=true
      shift 1
      ;;
    -d|--debug)
      DEBUG=true
      VERBOSE=true  # debug 模式自动启用 verbose
      shift 1
      ;;
    -G|--generator)
      GENERATOR="$2"
      shift 2
      ;;
    -h|--help)
      usage
      ;;
    -r|--release)
      RELEASE=true
      shift 1
      ;;
    -D*)
      CMAKE_VARS+=("$1")
      shift 1
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

# 根据选择的构建工具设置生成器和构建程序
if [ "$GENERATOR" = "Makefile" ] || [ "$GENERATOR" = "Unix Makefiles" ]; then
    CMAKE_GENERATOR="Unix Makefiles"
    BUILD_PROGRAM="make"
    echo "Using Makefile generator"
else
    CMAKE_GENERATOR="Ninja"
    BUILD_PROGRAM="$NINJA_PROGRAM"
    echo "Using Ninja generator"
fi


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

# Add release flags if enabled
if [ "$RELEASE" = true ]; then
    CMAKE_VARS+=("-DENABLE_DEBUG_PATH=OFF")
    echo "Release mode enabled (-DENABLE_DEBUG_PATH=OFF)"
fi

if [ "$CMAKE_GENERATOR" = "Ninja" ]; then
    $CMAKE_PROGRAM -B "$OUTPUT" -G "$CMAKE_GENERATOR" -S "$PROJECT_PATH" \
        -DCMAKE_MAKE_PROGRAM="$BUILD_PROGRAM" \
        "${CMAKE_VARS[@]}"
else
    $CMAKE_PROGRAM -B "$OUTPUT" -G "$CMAKE_GENERATOR" -S "$PROJECT_PATH" \
        "${CMAKE_VARS[@]}"
fi

# Prepare ninja debug flags
NINJA_DEBUG_FLAGS=""
if [ "$DEBUG" = true ]; then
    NINJA_DEBUG_FLAGS="-d explain"
    echo "Debug mode enabled (ninja $NINJA_DEBUG_FLAGS)"
fi

# Build with optional verbose/debug flags
set +e  # 临时允许命令失败

if [ "$VERBOSE" = true ]; then
    echo "Verbose mode enabled (ninja -v)"
    if [ -z "$TARGET" ]; then
        $CMAKE_PROGRAM --build "$OUTPUT" -j${JOBS} -- -v $NINJA_DEBUG_FLAGS
        BUILD_EXIT_CODE=$?
    else
        $CMAKE_PROGRAM --build "$OUTPUT" --target "$TARGET" -j${JOBS} -- -v $NINJA_DEBUG_FLAGS
        BUILD_EXIT_CODE=$?
    fi
else
    if [ -z "$TARGET" ]; then
        $CMAKE_PROGRAM --build "$OUTPUT" -j${JOBS} -- $NINJA_DEBUG_FLAGS
        BUILD_EXIT_CODE=$?
    else
        $CMAKE_PROGRAM --build "$OUTPUT" --target "$TARGET" -j${JOBS} -- $NINJA_DEBUG_FLAGS
        BUILD_EXIT_CODE=$?
    fi
fi

set -e  # 恢复严格模式
if [ "${BUILD_EXIT_CODE:-0}" -ne 0 ]; then
    echo "Build failed (exit code: $BUILD_EXIT_CODE)" >&2
    exit "$BUILD_EXIT_CODE"
fi
