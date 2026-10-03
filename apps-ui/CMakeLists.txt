set(LISA_UI_LIB_NAME lisa_ui_app)
set(CMAKE_C_EXTENSIONS ON)
set(CONFIG_LOG_ENABLE 1)
add_definitions(-DCONFIG_LOG_ENABLE=${CONFIG_LOG_ENABLE})

set(CONFIG_LOG_LEVEL 4)
add_definitions(-DCONFIG_LOG_LEVEL=${CONFIG_LOG_LEVEL})

# PC 模拟器(LISA_UI_SIM=ON, 由 sim/CMakeLists.txt 设置)下只编译框架+game 屏,
# 不编译依赖固件 API 的 llm 产品代码与 romfs 字体; 设备构建编完整产品 UI。
if(LISA_UI_SIM)
    set(LISA_UI_IS_SIM 1)
    # 全目录生效: game 屏等代码用它区分 sim/设备分支 (linux_sdl2 子目录里的同名定义只影响其子树)
    add_definitions(-DCONFIG_LVGL_ENV_SIMULATOR=1)
else()
    set(LISA_UI_IS_SIM 0)
endif()

add_library(${LISA_UI_LIB_NAME} STATIC)

target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lisa_ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lisa_ui_nav_scr.c
)

target_include_directories(${LISA_UI_LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(portable)
add_subdirectory(apps)
add_subdirectory(assets)
add_subdirectory(lv-i18n)
