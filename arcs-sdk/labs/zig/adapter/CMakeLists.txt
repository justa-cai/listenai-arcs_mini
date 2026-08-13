# ARCS SDK Zig Adapter - CMake integration
# Register the shared `arcs-zig` target and expose target-oriented Zig helpers.
#
# This directory is included centrally from the SDK root. Targets that use Zig
# should declare `LISTENAI_ZIG_APP`, call `listenai_add_executable()`, then call
# `listenai_maybe_enable_zig_target(${PROJECT_NAME})`.
#
# The actual target-side helper implementation lives in `zig_target.cmake`.
# This file stays lightweight so non-Zig targets do not probe the host Zig
# compiler during configure.

cmake_minimum_required(VERSION 3.20)

if(TARGET arcs-zig)
    return()
endif()

set(ZIG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ARCS_ZIG_SDK_ROOT "$ENV{ARCS_BASE}")
if(NOT ARCS_ZIG_SDK_ROOT)
    get_filename_component(ARCS_ZIG_SDK_ROOT "${ZIG_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

# The actual Zig build steps are created lazily so non-Zig targets do not
# require a host Zig compiler during configure.
add_library(arcs-zig INTERFACE)
target_include_directories(arcs-zig INTERFACE
    ${ZIG_SOURCE_DIR}/include
)

set_property(GLOBAL PROPERTY ARCS_ZIG_ADAPTER_DIR ${ZIG_SOURCE_DIR})
set_property(GLOBAL PROPERTY ARCS_ZIG_SDK_ROOT ${ARCS_ZIG_SDK_ROOT})
include(${ZIG_SOURCE_DIR}/zig_target.cmake)
