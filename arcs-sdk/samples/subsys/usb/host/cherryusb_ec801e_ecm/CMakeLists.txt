cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/ec801e_usb_at.c
    src/ec801e_usb_at_parse.c
    src/main.c
    src/net_probe.c
    src/net_probe_checks.c
)

listenai_include_directories(
    src
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Os
)
listenai_code_relocate(LIBRARY chryusb LOCATION PSRAM)