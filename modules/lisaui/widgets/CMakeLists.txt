# 定义该目录下的源文件
FILE(GLOB SOURCES "*.c")
FILE(GLOB VOICE_SOURCES "voice/*.c")
FILE(GLOB VOICE_SOURCES "llm/*.c")
FILE(GLOB_RECURSE ASSETS_SOURCES "assets/c/*.c")

set(WIDGETS_SOURCES
    base/ls_radio_group.c
    base/ls_lv_list.c
    base/ls_llm_dialog.c
    # assets/icon/icon_img_app_default_png.c
    ${SOURCES}
    ${VOICE_SOURCES}
    ${ASSETS_SOURCES}
    common/lisaui_asset_manager.c
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
target_sources(${TARGET_NAME} PRIVATE
    ${WIDGETS_SOURCES}
    ${WIDGETS_FONTS_SOURCES}
)

# 添加该目录到主库的包含路径
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/base
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/voice
    ${CMAKE_CURRENT_SOURCE_DIR}/llm
)
