listenai_library_named(app-services)
listenai_library_sources(
    service_led.c
    service_brightness.c
    service_volume.c
    service_button.c
    service_camera.c
    service_alarm.c
    service_image.c
    service_power_policy.c
)

# SD 卡离线音乐服务仅在启用 SD 中间件时编译 (关闭 SD 后本模块一并移除)
if(CONFIG_MIDDLEWARE_SD_MUSIC)
listenai_library_sources(
    service_sd_music.c
)
endif()

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
