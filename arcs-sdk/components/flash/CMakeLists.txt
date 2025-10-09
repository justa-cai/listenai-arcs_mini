if (DEFINED CONFIG_LISA_FLASH_MSG)
    listenai_library_named(lisa_flash_msg)
    
    listenai_library_sources(
        flash_msg.c
        arcs_flash.c
    )

    listenai_include_directories(.)
endif()
