listenai_library_named(app-usb)

listenai_library_sources(
    app_usb.c
    app_usb_descriptors.c
    app_usb_msc.c
)

listenai_library_sources_ifdef(CONFIG_APP_USB_CDC_ENABLE app_usb_cdc.c cdc_protocol.c)
listenai_library_sources_ifdef(CONFIG_APP_USB_AUDIO_ENABLE app_usb_audio.c)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
