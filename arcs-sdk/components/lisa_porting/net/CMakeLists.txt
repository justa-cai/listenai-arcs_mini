if (DEFINED CONFIG_LISA_NETWORK)
    listenai_library_named(lisa_net)

    listenai_library_sources(
        lisa_http.c
        lisa_websocket.c
    )

    listenai_include_directories(.)

endif()