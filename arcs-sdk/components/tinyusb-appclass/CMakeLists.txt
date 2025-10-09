if (DEFINED CONFIG_TUSB_APP_CLASS)
    listenai_library_named(tinyusb-appclass)

    listenai_library_sources(
        tusb_appclass.c
    )
    listenai_add_subdirectory_ifdef(CONFIG_ADB adb)
    listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../modules/tinyusb/src/device)

endif()
