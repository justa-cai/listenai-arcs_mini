# if (CONFIG_LOW_PORT)    
    listenai_library_named(low_port)
    listenai_library_sources(
        src/plat_os.c
        src/low_wifi.c
        src/low_wifi_http.c
    )
    listenai_link_libraries(m)
    listenai_include_directories(./inc)
# endif()
