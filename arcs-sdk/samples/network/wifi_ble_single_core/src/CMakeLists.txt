listenai_library_named(app)
listenai_library_sources(
    main.c
    listen_wifi.c
    arcs_flash_if.c
    listen_ble.c
    app_net_cfg.c
    rom_uart.c
    bt_if/bt_app_if.c
    bt_if/bt_ble_if.c
    # bt_if/bt_classic_if.c
    bt_if/bt_ip_if.c
    bt_if/bt_stack_if.c
    task/os_task_init.c
)

listenai_include_directories(
    ./
    bt_if
    task
)
