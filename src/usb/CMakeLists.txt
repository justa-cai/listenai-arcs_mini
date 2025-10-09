listenai_library_named(app-usb)

listenai_library_sources(
    app_usb.c
    app_usb_descriptors.c
    app_usb_msc.c
    app_usb_audio.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
