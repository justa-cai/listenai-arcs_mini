if (DEFINED CONFIG_ADB)
    listenai_library_named(adb)

    listenai_library_sources(
        adb_device.c
        adb.c
        adb_services.c
    )

    listenai_library_sources_ifdef(
        CONFIG_ADB_SHELL
        adb_shell.c
    )

    listenai_library_sources_ifdef(
        CONFIG_ADB_SYNC
        adb_sync.c
        adb_sync_ext_disk.c
    )

    listenai_include_directories(./)
endif()
