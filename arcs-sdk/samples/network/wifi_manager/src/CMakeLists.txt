listenai_library_named(app)
listenai_library_sources(
    main.c
    listen_wifi.c
    utils/evs_utils.c
    utils/evs_event.c
)

listenai_include_directories(
    ./
    ./utils
)