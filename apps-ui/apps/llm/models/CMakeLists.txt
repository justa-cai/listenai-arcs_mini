target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    model_voice.c
    model_common.c
    model_qrcode.c
    model_modem.c
    model_wifi.c
    model_battery.c
    model_camera.c
    model_camera_preview.c
    model_alarm.c
    model_sd_music_sync.c
)

if(CONFIG_OTA)
    target_sources(${LISA_UI_LIB_NAME} PRIVATE model_ota.c)
endif()

if(CONFIG_LOG_UPLOAD)
    target_sources(${LISA_UI_LIB_NAME} PRIVATE model_log_upload.c)
endif()

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ./
)
