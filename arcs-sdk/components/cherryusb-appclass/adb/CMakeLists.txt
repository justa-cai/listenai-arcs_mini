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
        adb_sync_stage.c
    )

    listenai_library_sources_ifdef(
        CONFIG_ADB_REBOOT
        adb_reboot.c
    )

    listenai_include_directories(
        ./
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/chryusb
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/chryusb/common
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/chryusb/core
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/chryusb/class/adb
    )

    target_link_libraries(adb PUBLIC chryusb)
endif()
