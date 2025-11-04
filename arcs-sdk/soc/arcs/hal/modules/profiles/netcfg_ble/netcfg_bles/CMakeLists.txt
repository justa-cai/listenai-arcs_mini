cmake_minimum_required(VERSION 3.10)

# 静态库目标
listenai_library_named(ble_profile_netcfg_bles)

listenai_library_sources(
    src/netcfg_bles.c
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
