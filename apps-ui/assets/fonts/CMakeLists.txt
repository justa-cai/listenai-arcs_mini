target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lv_font_chinese_18_bin.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lv_font_notosans_cs_medium_14.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lv_font_notosans_cs_medium_18.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lv_font_rubik_bold_64_ascii.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lv_font_rubik_bold_64.c
)

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
