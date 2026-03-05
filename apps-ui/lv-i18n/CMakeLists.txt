target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lv_i18n.c
)

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ./
)
