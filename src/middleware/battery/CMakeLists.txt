if(CONFIG_BATTERY_COLLECTION)

listenai_library_named(battery)

listenai_library_sources(
    battery.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
