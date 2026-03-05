if(CONFIG_POWER_MANAGER)

listenai_library_named(power_mgr)

listenai_library_sources(
    power_manager.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
