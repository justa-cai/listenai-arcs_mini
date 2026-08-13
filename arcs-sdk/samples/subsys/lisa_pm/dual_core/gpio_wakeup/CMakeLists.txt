cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

function(read_hex_config config_file config_name out_var)
    file(READ ${config_file} config_text)
    string(REGEX MATCH "(^|\n)${config_name}=(0x[0-9A-Fa-f]+)" match "${config_text}")
    if(NOT CMAKE_MATCH_2)
        message(FATAL_ERROR "${config_file} must define ${config_name}=0x...")
    endif()
    set(${out_var} ${CMAKE_MATCH_2} PARENT_SCOPE)
endfunction()

set(CP_PRJ_CONF ${CMAKE_CURRENT_SOURCE_DIR}/prj.conf)
set(AP_PRJ_CONF ${CMAKE_CURRENT_SOURCE_DIR}/remote/prj.conf)

read_hex_config(${CP_PRJ_CONF} CONFIG_MEM_FLASH_BASE CP_FLASH_BASE)
read_hex_config(${CP_PRJ_CONF} CONFIG_MEM_PSRAM_BASE CP_PSRAM_BASE)
read_hex_config(${CP_PRJ_CONF} CONFIG_MEM_PSRAM_SIZE CP_PSRAM_SIZE)
read_hex_config(${AP_PRJ_CONF} CONFIG_MEM_PSRAM_BASE AP_PSRAM_BASE)
read_hex_config(${AP_PRJ_CONF} CONFIG_MEM_PSRAM_SIZE AP_PSRAM_SIZE)

math(EXPR AP_PSRAM_END "${AP_PSRAM_BASE} + ${AP_PSRAM_SIZE}")
math(EXPR CP_PSRAM_END "${CP_PSRAM_BASE} + ${CP_PSRAM_SIZE}")

if(NOT AP_PSRAM_BASE STREQUAL "0x28000000")
    message(FATAL_ERROR "AP PSRAM must start at 0x28000000")
endif()
if(NOT CP_PSRAM_BASE STREQUAL "0x28400000")
    message(FATAL_ERROR "CP PSRAM must start at 0x28400000")
endif()
if(NOT AP_PSRAM_END EQUAL 0x28400000)
    message(FATAL_ERROR "AP PSRAM must end at 0x28400000")
endif()
if(NOT CP_PSRAM_END EQUAL 0x28800000)
    message(FATAL_ERROR "CP PSRAM must end at 0x28800000")
endif()

listenai_include_directories(./)

listenai_compile_definitions(
    EXPECTED_CP_FLASH_BASE=${CP_FLASH_BASE}
    EXPECTED_CP_PSRAM_BASE=${CP_PSRAM_BASE}
    EXPECTED_CP_PSRAM_SIZE=${CP_PSRAM_SIZE}
)

add_subdirectory(src)

listenai_add_executable(${PROJECT_NAME})

# 独立 AP 固件子工程，与 CP 镜像一起构建。
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
        -DEXPECTED_CP_FLASH_BASE=${CP_FLASH_BASE}
        -DEXPECTED_AP_PSRAM_BASE=${AP_PSRAM_BASE}
        -DEXPECTED_AP_PSRAM_SIZE=${AP_PSRAM_SIZE}
        $<$<BOOL:${BOARD_SEARCH_PATH}>:-DBOARD_SEARCH_PATH=${BOARD_SEARCH_PATH}>
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
)

add_dependencies(${PROJECT_NAME} remote_core)
