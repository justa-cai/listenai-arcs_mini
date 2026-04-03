listenai_library_named(app)

listenai_include_directories(
    ./
)

listenai_library_sources(
    main.c
    custom_service.c
    lcd_logo.c

    arcs_flash_if.c
)




