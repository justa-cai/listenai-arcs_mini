cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

if(NOT DEFINED CONFIG_DEFAULT)
    set(CONFIG_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf")
endif()

get_filename_component(BOOT_CONFIG_NAME "${CONFIG_DEFAULT}" NAME)

set(FW_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fw/prj.conf")

if(BOOT_CONFIG_NAME STREQUAL "prj.conf")
    set(FW_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fw/prj.conf")
elseif(BOOT_CONFIG_NAME STREQUAL "prj_boot_a.conf")
    set(FW_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fw/prj_fw_a.conf")
elseif(BOOT_CONFIG_NAME STREQUAL "prj_boot_b.conf")
    set(FW_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fw/prj_fw_b.conf")
elseif(BOOT_CONFIG_NAME STREQUAL "prj_boot_c.conf")
    set(FW_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fw/prj_fw_c.conf")
elseif(BOOT_CONFIG_NAME STREQUAL "prj_boot_d.conf")
    set(FW_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fw/prj_fw_d.conf")
elseif(BOOT_CONFIG_NAME STREQUAL "prj_boot_cp_a.conf")
    set(FW_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fw/prj_fw_cp_a.conf")
else()
    message(FATAL_ERROR "Unsupported vaddr_remap boot config: ${BOOT_CONFIG_NAME}")
endif()

project(vaddr_remap_test_boot)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE test_main.c)

include(ExternalProject)

ExternalProject_Add(
    vaddr_remap_fw
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fw
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/fw
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCONFIG_DEFAULT=${FW_CONFIG_PATH}
        -DBOARD=${BOARD}
        $<$<BOOL:${BOARD_SEARCH_PATH}>:-DBOARD_SEARCH_PATH=${BOARD_SEARCH_PATH}>
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
)

add_dependencies(${PROJECT_NAME} vaddr_remap_fw)
