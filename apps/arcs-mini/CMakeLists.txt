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
add_subdirectory(${PROJECT_TOP_DIR}/modules ${CMAKE_BINARY_DIR}/modules)
add_subdirectory(${PROJECT_TOP_DIR}/src ${CMAKE_BINARY_DIR}/src)

if (DEFINED CONFIG_APPLICATION_UI)
    add_subdirectory(${PROJECT_TOP_DIR}/apps-ui ${CMAKE_BINARY_DIR}/apps-ui)
endif()

add_subdirectory(tone)
add_subdirectory(services)
add_subdirectory(mcp-tools)
add_subdirectory(misc)

listenai_add_executable(${PROJECT_NAME})

# add_subdirectory(msgs)
# add_subdirectory(recognize)

target_sources(
    ${PROJECT_NAME} PRIVATE
    main.c
    voice_player.c
)

target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--wrap,gettimeofday
    -Wl,--wrap,settimeofday
    -Wl,--wrap,time
    -Wl,--wrap,gmtime_r
)
