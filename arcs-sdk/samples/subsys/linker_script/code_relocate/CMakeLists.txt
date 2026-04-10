cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)

# --- 按段名重定位: 将 .text.irq_handler 段搬入 SRAM ---
listenai_code_relocate(SECTIONS .text.irq_handler LOCATION SRAM_TEXT)

# --- 按文件重定位 (示例注释，将指定源文件整体搬入 SRAM):
# listenai_code_relocate(FILES my_fast_math.c LOCATION SRAM)

# --- 按库重定位 (示例注释，将整个第三方库搬入 PSRAM):
# listenai_code_relocate(LIBRARY modules_lvgl LOCATION PSRAM)
