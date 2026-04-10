if(CONFIG_ARCS_HAL_BSP)
    enable_language(ASM)

    listenai_library_named(arcs_hal_bsp)

    listenai_library_sources(
        wakeup_entry.S
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

endif()
