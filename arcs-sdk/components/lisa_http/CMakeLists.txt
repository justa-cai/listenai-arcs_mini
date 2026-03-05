if (DEFINED CONFIG_LISA_HTTP)
    listenai_library_named(lisa_http)

    listenai_library_sources(
        lisa_http.c
    )

    listenai_include_directories(.)

endif()
