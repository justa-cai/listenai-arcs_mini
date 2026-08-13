cmake_minimum_required(VERSION 3.19)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})
project(rust_ble_adv)

set(LISTENAI_RUST_APP ${CMAKE_CURRENT_SOURCE_DIR})

# The LISA Bluetooth driver includes "bt_config.h" (BT stack tunables); every
# BLE sample ships its own copy at its root, so put it on the include path.
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

listenai_add_executable(${PROJECT_NAME})
include($ENV{ARCS_BASE}/modules/rust/rust_sample.cmake)
listenai_maybe_enable_rust_target(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE src/main.c)
