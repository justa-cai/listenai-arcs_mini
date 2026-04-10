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
    echo "  -c, --config <file>    指定配置文件 (默认: prj.conf)"
    echo "  -v, --verbose          显示详细的编译命令 (Ninja: -v, Makefile: VERBOSE=1)"
    echo "  -d, --debug            启用调试模式 (仅 Ninja: -d explain)"
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
CONFIG_FILE="prj.conf"
DEV_TOOLS_DIR_NAME="listenai-dev-tools"
DEV_TOOL_TOOLCHAIN_DIR_NAME="gcc"
DEV_TOOL_LISTENAI_TOOLS_DIR_NAME="listenai-tools"

# 自动查找 SDK 根目录（通过 cmake/listenai-cmake-config.cmake 标志文件识别）
find_arcs_base() {
    local current_dir="$SCRIPT_DIR"

    while [ "$current_dir" != "/" ]; do
        # 检查当前目录本身是否是 SDK 根目录
        if [ -f "$current_dir/cmake/listenai-cmake-config.cmake" ]; then
            echo "Found ARCS_BASE: $current_dir"
            export ARCS_BASE="$current_dir"
            return 0
        fi

        # 检查当前目录的子目录中是否包含 SDK
        for subdir in "$current_dir"/*/; do
            if [ -f "${subdir}cmake/listenai-cmake-config.cmake" ]; then
                local resolved
                resolved=$(cd "$subdir" && pwd)
                echo "Found ARCS_BASE: $resolved"
                export ARCS_BASE="$resolved"
                return 0
            fi
        done

        current_dir=$(dirname "$current_dir")
    done

    echo "Error: ARCS_BASE not found. Please set ARCS_BASE environment variable to the SDK root directory."
    exit 1
}

find_dev_tools() {
    local current_dir="$SCRIPT_DIR"
    local dir_name="$DEV_TOOLS_DIR_NAME"

    # 优先检查 ~/.listenai（env.sh 默认安装路径）
    if [ -d "${HOME}/.listenai" ]; then
        local home_tools="${HOME}/.listenai"
        if [ -d "$home_tools/$DEV_TOOL_LISTENAI_TOOLS_DIR_NAME" ]; then
            echo "Found LISTENAI_TOOLS_PATH: $home_tools/$DEV_TOOL_LISTENAI_TOOLS_DIR_NAME"
            export LISTENAI_TOOLS_PATH="$home_tools/$DEV_TOOL_LISTENAI_TOOLS_DIR_NAME"
        fi
        if [ -d "$home_tools/$DEV_TOOL_TOOLCHAIN_DIR_NAME" ]; then
            echo "Found NUCLEI_TOOLCHAIN_PATH: $home_tools/$DEV_TOOL_TOOLCHAIN_DIR_NAME"
            export NUCLEI_TOOLCHAIN_PATH="$home_tools/$DEV_TOOL_TOOLCHAIN_DIR_NAME"
        fi
        return 0
    fi

    echo "trying to find $dir_name in parent directories..."

    while [ "$current_dir" != "/" ]; do
        if [ -d "$current_dir/$dir_name" ]; then
            echo "Found $dir_name: $current_dir/$dir_name"
            if [ -d "$current_dir/$dir_name/$DEV_TOOL_LISTENAI_TOOLS_DIR_NAME" ]; then
                echo "Found LISTENAI_TOOLS_PATH: $current_dir/$dir_name/$DEV_TOOL_LISTENAI_TOOLS_DIR_NAME"
                export LISTENAI_TOOLS_PATH="$current_dir/$dir_name/$DEV_TOOL_LISTENAI_TOOLS_DIR_NAME"
            fi
            if [ -d "$current_dir/$dir_name/$DEV_TOOL_TOOLCHAIN_DIR_NAME" ]; then
                echo "Found NUCLEI_TOOLCHAIN_PATH: $current_dir/$dir_name/$DEV_TOOL_TOOLCHAIN_DIR_NAME"
                export NUCLEI_TOOLCHAIN_PATH="$current_dir/$dir_name/$DEV_TOOL_TOOLCHAIN_DIR_NAME"
            fi
            return 0
        fi
        current_dir=$(dirname "$current_dir")
    done
}

resolve_config_path() {
    case "$CONFIG_FILE" in
        /*)
            echo "$CONFIG_FILE"
            ;;
        *)
            echo "$PROJECT_PATH/$CONFIG_FILE"
            ;;
    esac
}

resolve_project_path() {
    if ! PROJECT_PATH=$(cd "$PROJECT_PATH" 2>/dev/null && pwd); then
        echo "错误: 项目源码路径不存在: $PROJECT_PATH"
        exit 1
    fi
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
      if [ -z "$2" ] || [[ "$2" == -* ]]; then
          echo "错误: -j/--jobs 需要一个数值参数"
          exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]] || [ "$2" -eq 0 ]; then
          echo "错误: -j/--jobs 参数必须是正整数，收到: $2"
          exit 1
      fi
      JOBS="$2"
      shift 2
      ;;
    -j*)
      JOBS="${1#-j}"
      if ! [[ "$JOBS" =~ ^[0-9]+$ ]] || [ "$JOBS" -eq 0 ]; then
          echo "错误: -j 参数必须是正整数，收到: $JOBS"
          exit 1
      fi
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
      if [ -z "$2" ] || [[ "$2" == -* ]]; then
          echo "错误: -G/--generator 需要一个参数 (Ninja 或 Makefile)"
          exit 1
      fi
      case "$2" in
          Ninja|ninja)       GENERATOR="Ninja" ;;
          Makefile|makefile|"Unix Makefiles") GENERATOR="Makefile" ;;
          *)
              echo "错误: 不支持的生成器类型: $2 (支持: Ninja, Makefile)"
              exit 1
              ;;
      esac
      shift 2
      ;;
    -h|--help)
      usage
      ;;
    -r|--release)
      RELEASE=true
      shift 1
      ;;
    -c|--config)
      if [ -z "$2" ] || [[ "$2" == -* ]]; then
          echo "错误: -c/--config 需要一个配置文件参数"
          exit 1
      fi
      CONFIG_FILE="$2"
      shift 2
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

resolve_project_path

CONFIG_PATH=$(resolve_config_path)

if [ ! -f "$CONFIG_PATH" ]; then
    echo "错误: 配置文件 $CONFIG_FILE 不存在"
    exit 1
fi

echo "Source: $PROJECT_PATH"
echo "Target: $TARGET"
echo "Clean : $CLEAN"
echo "Config: $CONFIG_PATH"

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


# 配置环境变量 ARCS_BASE（自动查找 SDK 根目录）
if [ -z "$ARCS_BASE" ]; then
    find_arcs_base
fi

if [ "$CLEAN" = true ]; then
    rm -rf "$OUTPUT"
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

CMAKE_VARS+=("-DCONFIG_DEFAULT=$CONFIG_PATH")

if [ "$CMAKE_GENERATOR" = "Ninja" ]; then
    $CMAKE_PROGRAM -B "$OUTPUT" -G "$CMAKE_GENERATOR" -S "$PROJECT_PATH" \
        -DCMAKE_MAKE_PROGRAM="$BUILD_PROGRAM" \
        "${CMAKE_VARS[@]}"
else
    $CMAKE_PROGRAM -B "$OUTPUT" -G "$CMAKE_GENERATOR" -S "$PROJECT_PATH" \
        "${CMAKE_VARS[@]}"
fi

# Prepare build tool flags (区分 Ninja 和 Makefile)
BUILD_TOOL_FLAGS=""
if [ "$CMAKE_GENERATOR" = "Ninja" ]; then
    if [ "$DEBUG" = true ]; then
        BUILD_TOOL_FLAGS="-d explain"
        echo "Debug mode enabled (ninja -d explain)"
    fi
    if [ "$VERBOSE" = true ]; then
        BUILD_TOOL_FLAGS="-v $BUILD_TOOL_FLAGS"
        echo "Verbose mode enabled (ninja -v)"
    fi
else
    # Makefile 模式：使用 VERBOSE=1 实现详细输出
    if [ "$DEBUG" = true ]; then
        echo "注意: -d/--debug 的 explain 功能仅在 Ninja 生成器下可用，Makefile 模式已忽略"
    fi
    if [ "$VERBOSE" = true ]; then
        BUILD_TOOL_FLAGS="VERBOSE=1"
        echo "Verbose mode enabled (make VERBOSE=1)"
    fi
fi

# Build
set +e  # 临时允许构建命令失败

BUILD_CMD=("$CMAKE_PROGRAM" --build "$OUTPUT" -j"${JOBS}")
if [ -n "$TARGET" ]; then
    BUILD_CMD+=(--target "$TARGET")
fi
if [ -n "$BUILD_TOOL_FLAGS" ]; then
    BUILD_CMD+=(-- $BUILD_TOOL_FLAGS)
fi

"${BUILD_CMD[@]}"
BUILD_EXIT_CODE=$?

set -e  # 恢复严格模式
if [ "${BUILD_EXIT_CODE:-0}" -ne 0 ]; then
    echo "Build failed (exit code: $BUILD_EXIT_CODE)" >&2
    exit "$BUILD_EXIT_CODE"
fi
