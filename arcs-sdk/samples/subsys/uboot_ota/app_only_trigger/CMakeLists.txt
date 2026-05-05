cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(uboot_ota_app_only_trigger)

listenai_add_executable(${PROJECT_NAME})
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_sources(${PROJECT_NAME} PRIVATE
    src/main.c
)
