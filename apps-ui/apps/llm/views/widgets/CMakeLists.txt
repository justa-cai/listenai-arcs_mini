target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    lisa_ui_anim_ext.c
    lisa_ui_anim.c
    lisa_ui_toast.c
)

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ./
)
