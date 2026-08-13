listenai_library_named(app)

listenai_library_sources(
    main.c
    usb_descriptors.c
    app_context.c
    fs_utils.c
    wifi_utils.c
    playback_mgr.c
    ui_main_menu.c
    ui_usb_msc.c
    ui_local_list.c
    ui_online_list.c
    ui_playback.c
)

listenai_include_directories(
    .
)
