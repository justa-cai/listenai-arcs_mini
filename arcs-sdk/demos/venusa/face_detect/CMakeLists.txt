cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置默认板型
if(NOT DEFINED BOARD)
    set(BOARD "csk7s_yt_evb")
    message(STATUS "Using default BOARD: ${BOARD}")

    set(BOARD_SEARCH_PATH "${CMAKE_CURRENT_LIST_DIR}/boards" CACHE PATH "Board search path" FORCE)
endif()


find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(venusa_face_detect)

set(FACE_DETECT_VERSION "0.1.3" CACHE STRING "VenusA face-detect demo version")
string(TIMESTAMP FACE_DETECT_BUILD_TIME "%Y-%m-%d %H:%M:%S")
set(FACE_DETECT_VERSION_HEADER_DIR "${CMAKE_BINARY_DIR}/generated/face_detect")
file(MAKE_DIRECTORY "${FACE_DETECT_VERSION_HEADER_DIR}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${FACE_DETECT_VERSION_HEADER_DIR}/version.h
    @ONLY
)

add_subdirectory(src)

listenai_add_executable(${PROJECT_NAME})

if(CONFIG_FACE_DETECT_UVC_ENABLE)
    listenai_code_relocate(LIBRARY chryusb LOCATION PSRAM)
endif()
