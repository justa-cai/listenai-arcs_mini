# CMake toolchain file — 用于在宿主机上交叉编译 ARCS/LS26 (rv32imac / ilp32) 目标的静态库
# 供 tools/build_rlottie.sh 使用，不属于 ARCS 固件构建系统的一部分。
#
# 使用方式（通常由 build_rlottie.sh 自动传递）：
#   cmake -DCMAKE_TOOLCHAIN_FILE=riscv64-unknown-elf.cmake \
#         [-DNUCLEI_TOOLCHAIN_PATH=/path/to/nuclei-toolchain] ...

cmake_minimum_required(VERSION 3.19)

set(CMAKE_SYSTEM_NAME  Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)

# --------------------------------------------------------------------------
# 工具链路径解析（优先级：cmake 变量 > 环境变量）
# --------------------------------------------------------------------------
if(DEFINED NUCLEI_TOOLCHAIN_PATH)
    set(_TC_BIN "${NUCLEI_TOOLCHAIN_PATH}/bin")
elseif(DEFINED ENV{NUCLEI_TOOLCHAIN_PATH})
    set(_TC_BIN "$ENV{NUCLEI_TOOLCHAIN_PATH}/bin")
elseif(DEFINED ENV{LISTENAI_TOOLS_PATH})
    set(_TC_BIN "$ENV{LISTENAI_TOOLS_PATH}/nuclei-toolchain/bin")
else()
    # 假设工具链已在 PATH 中
    set(_TC_BIN "")
endif()

set(_PREFIX riscv64-unknown-elf-)

if(_TC_BIN)
    set(CMAKE_C_COMPILER   "${_TC_BIN}/${_PREFIX}gcc"   CACHE FILEPATH "C compiler")
    set(CMAKE_CXX_COMPILER "${_TC_BIN}/${_PREFIX}g++"   CACHE FILEPATH "C++ compiler")
    set(CMAKE_AR           "${_TC_BIN}/${_PREFIX}ar"     CACHE FILEPATH "Archiver")
    set(CMAKE_RANLIB       "${_TC_BIN}/${_PREFIX}ranlib" CACHE FILEPATH "Ranlib")
    set(CMAKE_STRIP        "${_TC_BIN}/${_PREFIX}strip"  CACHE FILEPATH "Strip")
else()
    set(CMAKE_C_COMPILER   "${_PREFIX}gcc"   CACHE FILEPATH "C compiler")
    set(CMAKE_CXX_COMPILER "${_PREFIX}g++"   CACHE FILEPATH "C++ compiler")
endif()

# --------------------------------------------------------------------------
# 编译器目标标志（与当前 ARCS SDK `arcs_mini` 默认构建保持一致）
# 当前 sample 链接使用 soft-float ABI，因此这里也必须使用 `-mabi=ilp32`
# --------------------------------------------------------------------------
set(_ARCH_FLAGS "-march=rv32imac_zba_zbb_zbc_zbs -mabi=ilp32 -mtune=nuclei-300-series -msave-restore")

set(CMAKE_C_FLAGS_INIT   "${_ARCH_FLAGS}")
# 裸机环境下用 -fno-exceptions -fno-rtti 可减小代码体积；
# 但 rlottie 内部有异常使用，保留异常支持（工具链通过 libgcc 提供 SJLJ）。
set(CMAKE_CXX_FLAGS_INIT "${_ARCH_FLAGS}")

# 交叉编译时无法运行目标程序，把 try_compile 目标设为静态库
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# --------------------------------------------------------------------------
# 搜索路径策略
# --------------------------------------------------------------------------
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
