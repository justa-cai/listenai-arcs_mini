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
        adb_sync_stage.c
        adb_sync_metadata.c
        adb_sync_ext_disk.c
    )

    listenai_library_sources_ifdef(
        CONFIG_ADB_REBOOT
        adb_reboot.c
    )

    listenai_include_directories(
        ./
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/tinyusb/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/tinyusb/src/device
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/fs/lsfs
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/fs/disk/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/fs/port/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../system/uboot/src
    )
endif()
