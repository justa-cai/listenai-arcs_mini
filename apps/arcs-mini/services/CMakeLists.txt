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

# SD 卡音乐服务依赖 SDMMC 硬件，无卡座板型禁用
if(CONFIG_DISK_DRIVER_SDMMC)
    listenai_library_sources(service_sd_music.c)
endif()

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
