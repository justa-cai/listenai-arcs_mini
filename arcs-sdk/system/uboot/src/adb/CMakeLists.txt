listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ARCS_SDK_BASE}/components/tinyusb-appclass/adb
)

listenai_library_sources(
    adb.c
    adb_services.c
    usbdesc.c
)

listenai_library_sources_ifdef(CONFIG_BOOT_ADB_SHELL
    adb_shell.c
)

listenai_library_sources_ifdef(CONFIG_BOOT_ADB_SYNC
    adb_sync_ext_disk.c
    ${ARCS_SDK_BASE}/components/tinyusb-appclass/adb/adb_sync_stage.c
)
