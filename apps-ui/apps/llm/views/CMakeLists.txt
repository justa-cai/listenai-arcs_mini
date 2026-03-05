add_subdirectory(widgets)
add_subdirectory(setting)

target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    info_view.c
    setting_view.c
    lisa_ui_llm_base.c
    lisa_ui_llm_primary.c
    lisa_ui_keyboard_input.c
    alarm_success_view.c
    alarm_ring_view.c
    quota_qrcode_view.c
)

if(CONFIG_OTA)
    target_sources(${LISA_UI_LIB_NAME} PRIVATE ota_view.c)
endif()

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ./
)
