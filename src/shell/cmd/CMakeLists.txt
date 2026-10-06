listenai_library_named(shell_cmds)

listenai_library_sources(
    # app.c
    ble.c
    # fs.c
    heap.c
    id_cmd.c
    interact.c
    kv.c
    # listen_cmd.c
    # log.c
    mic_bias.c
    # player.c
    sys.c
    version.c
    wifi.c
)

listenai_library_sources_ifdef(CONFIG_CHERRYUSB_HOST_CDC_ACM usb_serial.c)

if(CONFIG_LISA_SDMMC_DEVICE)
    listenai_library_sources(sd.c)
endif()
listenai_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../middleware/usb)
