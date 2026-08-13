cmake_minimum_required(VERSION 3.19)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})
project(rust_async_tasks)

set(LISTENAI_RUST_APP ${CMAKE_CURRENT_SOURCE_DIR})

listenai_add_executable(${PROJECT_NAME})
include($ENV{ARCS_BASE}/modules/rust/rust_sample.cmake)
listenai_maybe_enable_rust_target(${PROJECT_NAME})

# The arcs-embassy crate needs a small FreeRTOS C shim; compile it as part of
# the app so it sees the full SDK / FreeRTOS include path.
target_sources(${PROJECT_NAME} PRIVATE
    src/main.c
    $ENV{ARCS_BASE}/modules/rust/arcs-embassy/glue/arcs_embassy_glue.c)
target_include_directories(${PROJECT_NAME} PRIVATE
    $ENV{ARCS_BASE}/modules/rust/arcs-embassy/glue)
