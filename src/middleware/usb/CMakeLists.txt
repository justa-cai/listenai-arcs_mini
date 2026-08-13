listenai_library_named(app-usb)

if(CONFIG_CHERRYUSB)
    if(CONFIG_TINY_USB)
        message(FATAL_ERROR "TinyUSB and CherryUSB cannot share the USB device controller")
    endif()

    if(CONFIG_APP_USB_CDC_ENABLE)
        message(FATAL_ERROR "CherryUSB CDC is not implemented by app-usb")
    endif()

    listenai_library_sources(
        app_usb_cherry.c
        app_usb_cherry_descriptors.c
        app_usb_cherry_msc.c
    )
    listenai_library_sources_ifdef(CONFIG_APP_USB_AUDIO_ENABLE app_usb_cherry_audio.c)
else()
    listenai_library_sources(
        app_usb.c
        app_usb_descriptors.c
        app_usb_msc.c
    )

    listenai_library_sources_ifdef(CONFIG_APP_USB_CDC_ENABLE app_usb_cdc.c cdc_protocol.c)
    listenai_library_sources_ifdef(CONFIG_APP_USB_AUDIO_ENABLE app_usb_audio.c)
endif()

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
