# 定义该目录下的源文件
FILE(GLOB SOURCES "*.c")
FILE(GLOB LLM_SOURCES "llm/*.c")
FILE(GLOB_RECURSE ASSETS_SOURCES "assets/c/*.c")

set(WIDGETS_SOURCES
    assets/lisa_ui_assets.c
    ${SOURCES}
    ${LLM_SOURCES}
    ${ASSETS_SOURCES}
)

# 字体资源文件
if (CONFIG_LISAUI_ENV_ARCS_SDK)
    set(WIDGETS_FONTS_SOURCES
        assets/fonts/lv_font_chinese_18.c
        assets/png/qr/ble_qr.c
    )
else()
    set(WIDGETS_FONTS_SOURCES
        assets/fonts/lv_font_chinese_18.c
        assets/fonts/lv_font_notosans_cs_medium_18.c
        assets/fonts/lv_font_notosans_cs_medium_14.c
        assets/fonts/lv_font_rubik_bold_64.c
        assets/fonts/lv_font_rubik_bold_64_ascii.c
    )
endif()

# 添加所有源文件到主库
listenai_library_sources(
    ${WIDGETS_SOURCES}
    ${WIDGETS_FONTS_SOURCES}
)

# 添加该目录到主库的包含路径
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/llm
)
