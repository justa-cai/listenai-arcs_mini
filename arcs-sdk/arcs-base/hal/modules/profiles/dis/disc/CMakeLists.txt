cmake_minimum_required(VERSION 3.10)

# 静态库目标
listenai_library_named(arcs_hal_disc)

listenai_library_sources(
    src/disc.c
)

listenai_library_compile_options_ifdef(
    CONFIG_ARCS_HAL_WARNING_DISABLE
    PRIVATE
    -w
)

# 包含目录配置
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/api/
)
