# common_widget/CMakeLists.txt
set(LISAUI_COMMON_TARGET_NAME common_widget)

set(${LISAUI_COMMON_TARGET_NAME}_UI_RESOURCES
    # assets/fonts/lv_font_chinese_18.c
    # assets/fonts/lv_font_notosans_cs_medium_18.c
    base/ls_radio_group.c
    base/ls_lv_list.c
    base/ls_llm_dialog.c
    assets/icon/icon_img_app_default_png.c
)



if (CONFIG_LISAUI_ENV_ARCS_SDK)
    set(${LISAUI_COMMON_TARGET_NAME}_UI_FONTS_RESOURCES
        ${${LISAUI_COMMON_TARGET_NAME}_UI_FONTS_RESOURCES}
        assets/fonts/lv_font_chinese_18_bin.c
    )
else()
    set(${LISAUI_COMMON_TARGET_NAME}_UI_FONTS_RESOURCES
        assets/fonts/lv_font_chinese_18.c
        assets/fonts/lv_font_notosans_cs_medium_18.c
        assets/fonts/lv_font_notosans_cs_medium_14.c
        assets/fonts/lv_font_rubik_bold_64.c
        assets/fonts/lv_font_rubik_bold_64_ascii.c
    )
endif()

add_library(${LISAUI_COMMON_TARGET_NAME} STATIC
    ${${LISAUI_COMMON_TARGET_NAME}_UI_RESOURCES}
    ${${LISAUI_COMMON_TARGET_NAME}_UI_FONTS_RESOURCES}
)

target_include_directories(${LISAUI_COMMON_TARGET_NAME}
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/base/
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
)


if (CONFIG_LISAUI_ENV_ARCS_SDK)
    target_link_libraries(${LISAUI_COMMON_TARGET_NAME}
    PRIVATE
        lisaui_utils
    PUBLIC
        listenai_interface)
    listenai_link_libraries(${LISAUI_COMMON_TARGET_NAME})
else()
    target_link_libraries(${LISAUI_COMMON_TARGET_NAME}
    PRIVATE
        lvgl
        utils
        lisaui_utils
        app_common
        )
endif()