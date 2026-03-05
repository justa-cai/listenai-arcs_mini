cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

# 定义宏,使用 MQTT 默认配置而不是自定义配置
add_definitions(-DMQTT_DO_NOT_USE_CUSTOM_CONFIG)
add_definitions(-DMQTT_AGENT_DO_NOT_USE_CUSTOM_CONFIG)

listenai_include_directories(
    ./
)

add_subdirectory(src)

listenai_add_executable(${PROJECT_NAME})
