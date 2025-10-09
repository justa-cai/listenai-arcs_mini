listenai_library_named(app)
listenai_library_sources(
    main.c
    listen_wifi.c
    # ls_crypto.c
    arcs_flash_if.c
)

listenai_include_directories(
    ./
)
