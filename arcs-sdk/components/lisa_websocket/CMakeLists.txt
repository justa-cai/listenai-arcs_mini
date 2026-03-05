if (DEFINED CONFIG_LISA_WEBSOCKET)

listenai_library_named(lisa_websocket)

listenai_library_sources(
    lisa_websocket.c
)

listenai_include_directories(.)

endif()
