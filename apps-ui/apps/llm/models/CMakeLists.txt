target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    model_voice.c
    model_common.c
    model_qrcode.c
    model_wifi.c
    model_battery.c
    model_camera.c
    model_alarm.c
)

if(CONFIG_OTA)
    target_sources(${LISA_UI_LIB_NAME} PRIVATE model_ota.c)
endif()

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ./
)
