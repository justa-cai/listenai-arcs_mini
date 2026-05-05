listenai_library_named(app)
listenai_library_sources(
    main.c
    fs/user_fs.c
)

if (CONFIG_SAMPLE_MODEM_BACKEND_UART)
    listenai_library_sources(
        modem_backend_uart.c
    )
endif()

if (CONFIG_SAMPLE_MODEM_BACKEND_USB_AT)
    listenai_library_sources(
        modem_backend_usb_at.c
    )
endif()

listenai_include_directories(
    ./
    ./fs
)
