cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(
    ./
)

add_subdirectory(src)

listenai_add_executable(${PROJECT_NAME})

# 从 CP（本核）prj.conf 提取 flash 基地址，透传给 AP 子构建做一致性校验
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/prj.conf _cp_prj_conf)
string(REGEX MATCH "CONFIG_MEM_FLASH_BASE=(0x[0-9A-Fa-f]+)" _ "${_cp_prj_conf}")
set(_CP_FLASH_BASE "${CMAKE_MATCH_1}")
if(NOT _CP_FLASH_BASE)
    message(FATAL_ERROR "无法从 prj.conf 解析 CONFIG_MEM_FLASH_BASE，dual_core sample 必须显式声明")
endif()

include(ExternalProject)

ExternalProject_Add(
    remote_core
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/remote
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/remote
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DBOARD=${BOARD}
        -DEXPECTED_CP_FLASH_BASE=${_CP_FLASH_BASE}
        $<$<BOOL:${BOARD_SEARCH_PATH}>:-DBOARD_SEARCH_PATH=${BOARD_SEARCH_PATH}>
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
)

add_dependencies(${PROJECT_NAME} remote_core)
