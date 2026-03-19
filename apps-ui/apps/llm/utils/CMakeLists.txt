target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    emoji_anim.c
    img_utils.c
    lv_img_utils.c
)

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    .
)
