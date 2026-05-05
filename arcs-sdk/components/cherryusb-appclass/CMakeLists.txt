if (DEFINED CONFIG_CHERRYUSB_APP_CLASS)
    listenai_add_subdirectory_ifdef(CONFIG_ADB adb)
endif()
