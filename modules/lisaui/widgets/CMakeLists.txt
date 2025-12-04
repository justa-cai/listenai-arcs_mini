# 定义该目录下的源文件
FILE(GLOB SOURCES "*.c")
FILE(GLOB VOICE_SOURCES "voice/*.c")
FILE(GLOB LLM_SOURCES "llm/*.c")
FILE(GLOB_RECURSE ASSETS_SOURCES "assets/c/*.c")

set(WIDGETS_SOURCES
    base/ls_radio_group.c
    base/ls_lv_list.c
    base/ls_llm_dialog.c
    common/lisaui_asset_manager.c
    ${SOURCES}
)

if (CONFIG_LISAUI_WIDGETS_ASSETS)
    list(APPEND WIDGETS_SOURCES
        assets/lisa_ui_assets.c
        ${ASSETS_SOURCES}
    )
endif()
if (CONFIG_LISAUI_WIDGETS_VOICE)
    list(APPEND WIDGETS_SOURCES
        ${VOICE_SOURCES}
    )
endif()
if (CONFIG_LISAUI_WIDGETS_LLM)
    list(APPEND WIDGETS_SOURCES
        ${LLM_SOURCES}
    )
endif()

# 字体资源文件
if (CONFIG_LISAUI_ENV_ARCS_SDK AND CONFIG_LISAUI_WIDGETS_ASSETS)
    set(WIDGETS_FONTS_SOURCES
        assets/fonts/lv_font_chinese_18.c
        assets/png/qr/ble_qr.c
    )
elseif(CONFIG_LISAUI_ENV_SIMULATOR AND CONFIG_LISAUI_WIDGETS_ASSETS)
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

set(widgets_inc)
if (CONFIG_LISAUI_WIDGETS_ASSETS)
    list(APPEND widgets_inc ${CMAKE_CURRENT_SOURCE_DIR}/assets)
endif()
if (CONFIG_LISAUI_WIDGETS_VOICE)
    list(APPEND widgets_inc ${CMAKE_CURRENT_SOURCE_DIR}/voice)
endif()
if (CONFIG_LISAUI_WIDGETS_LLM)
    list(APPEND widgets_inc ${CMAKE_CURRENT_SOURCE_DIR}/llm)
endif()

# 添加该目录到主库的包含路径
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/base
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${widgets_inc}
)
