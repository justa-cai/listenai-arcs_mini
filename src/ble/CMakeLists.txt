listenai_library_named(app-ble)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    bt_if
    task
)
listenai_library_sources(
    # main.c
    # listen_wifi.c
    # arcs_flash_if.c
    listen_ble.c
    app_net_cfg.c
    # rom_uart.c
    bt_if/bt_app_if.c
    bt_if/bt_ble_if.c
    # bt_if/bt_classic_if.c
    bt_if/bt_ip_if.c
    bt_if/bt_stack_if.c
    task/os_task_init.c
)