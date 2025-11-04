if(CONFIG_ARCS_HAL_FLASH_IF)
    listenai_library_named(arcs_hal_flash_if)

    listenai_compile_definitions(
        CFG_FLASH_IF=1
    )

    listenai_library_sources(
        flash_if.c
    )

endif()
