listenai_library_named(app-services)
listenai_library_sources(
    service_led.c
    service_brightness.c
    service_volume.c
    service_button.c
    service_alarm.c
    service_image.c
    service_power_policy.c
)

# 摄像头服务（DVP 帧缓冲 ~1.2MB PSRAM）；随 CONFIG_APP_CAMERA_SERVICE 裁剪
listenai_library_sources_ifdef(CONFIG_APP_CAMERA_SERVICE service_camera.c)

# SD 音乐服务依赖 SDMMC（MIDDLEWARE_SD_MUSIC 随 DISK_DRIVER_SDMMC 联动裁剪）
listenai_library_sources_ifdef(CONFIG_MIDDLEWARE_SD_MUSIC service_sd_music.c)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
