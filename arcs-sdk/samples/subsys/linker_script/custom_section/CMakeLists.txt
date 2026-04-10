cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)

# --- 注入自定义段到 ROM slot ---
listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/linker/app-config-sections.ld
    SLOT ROM
    SORT_KEY "50-app"
)

# --- 注册散加载表条目 ---
# 启动代码会将 .app_config 段内容从 ROM 复制到 RAM
listenai_add_linker_scatter(SCATLOAD .app_config)
