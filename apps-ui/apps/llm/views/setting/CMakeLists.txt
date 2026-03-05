target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    setting_common_view.c
    setting_wifi_view.c
    setting_wakeup_view.c
    setting_clock_view.c
)

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ./
)
