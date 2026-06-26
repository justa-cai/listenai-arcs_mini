cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置默认板型 (如果未在命令行指定)
if(NOT DEFINED BOARD)
    set(BOARD "arcs_mini")
    message(STATUS "Using default BOARD: ${BOARD}")
endif()

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs-mini)

include(version.cmake)

add_definitions(-DLISTEN_CLOUD -DCONFIG_EXT_RAM)

# Add project root directory to include path for app-config-tls-generic.h and memap.h
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
# Get project root directory (two levels up from apps/aopu-yuba)
get_filename_component(PROJECT_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
message (STATUS "PROJECT_TOP_DIR: ${PROJECT_TOP_DIR}")
# Python 3 is discovered by the SDK; reuse the cached Python3_EXECUTABLE.
set(PYTHON3 ${Python3_EXECUTABLE})

add_subdirectory(${PROJECT_TOP_DIR}/modules ${CMAKE_BINARY_DIR}/modules)
add_subdirectory(${PROJECT_TOP_DIR}/src ${CMAKE_BINARY_DIR}/src)

if (DEFINED CONFIG_APPLICATION_UI)
    add_subdirectory(${PROJECT_TOP_DIR}/apps-ui ${CMAKE_BINARY_DIR}/apps-ui)
endif()

add_subdirectory(tone)
add_subdirectory(services)
add_subdirectory(mcp-tools)
add_subdirectory(voice_intent)

listenai_add_executable(${PROJECT_NAME})

# res 目录命名约定：<app-name>[_variant]，BOARD 去掉 "arcs_mini" 前缀后即变体后缀
string(REGEX REPLACE "^arcs_mini" "" _VARIANT_SUFFIX ${BOARD})
set(RES_DIR ${PROJECT_TOP_DIR}/res/arcs-mini${_VARIANT_SUFFIX})
set(COMBINED_HEX ${CMAKE_BINARY_DIR}/${PROJECT_NAME}${_VARIANT_SUFFIX}.combined.hex)
set(OTA_TXZ ${CMAKE_BINARY_DIR}/${PROJECT_NAME}${_VARIANT_SUFFIX}-v${PROJECT_VERSION_STR}.ota.txz)

add_custom_target(
    combined_hex ALL
    COMMAND ${CMAKE_COMMAND} -E echo "-- Generating combined hex: ${COMBINED_HEX}"
    COMMAND ${PYTHON3} ${PROJECT_TOP_DIR}/tools/make_package.py
        --format hex
        --partition-table ${RES_DIR}/partition_table.json
        --var BUILD_DIR=${CMAKE_BINARY_DIR}
        ${COMBINED_HEX}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_dependencies(combined_hex ${PROJECT_NAME})

add_custom_target(
    ota_txz ALL
    COMMAND ${CMAKE_COMMAND} -E echo "-- Generating ota txz: ${OTA_TXZ}"
    COMMAND ${PYTHON3} ${PROJECT_TOP_DIR}/tools/make_package.py
        --format ota
        --partition-table ${RES_DIR}/partition_table.json
        --var BUILD_DIR=${CMAKE_BINARY_DIR}
        --ota-version ${PROJECT_VERSION_STR}
        ${OTA_TXZ}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_dependencies(ota_txz ${PROJECT_NAME})

# add_subdirectory(msgs)
# add_subdirectory(recognize)

target_sources(
    ${PROJECT_NAME} PRIVATE
    main.c
    voice_player.c
    button_camera_preview.c
)

target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--wrap,gettimeofday
    -Wl,--wrap,settimeofday
    -Wl,--wrap,time
    -Wl,--wrap,gmtime_r
)
