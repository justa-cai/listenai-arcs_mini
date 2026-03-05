target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    home_presenter.c
    info_presenter.c
    setting_presenter.c
    setting_common_presenter.c
    setting_wifi_presenter.c
    setting_wakeup_presenter.c
    setting_clock_presenter.c
    emoji_anim.c
    alarm_success_presenter.c
    alarm_ring_presenter.c
    quota_qrcode_presenter.c
)

if(CONFIG_OTA)
    target_sources(${LISA_UI_LIB_NAME} PRIVATE ota_presenter.c)
endif()

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ./
    ../views/setting
    ../models
)
