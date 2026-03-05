listenai_library_named(app-services)
listenai_library_sources(
    service_led.c
    service_brightness.c
    service_volume.c
    service_button.c
    service_camera.c
    service_alarm.c
    service_image.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
