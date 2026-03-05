if(CONFIG_LISA_FLASH)

listenai_library_named(lisa_flash)

listenai_include_directories(.)

listenai_library_sources(
    lisa_flash_arcs.c
)

endif() # CONFIG_LISA_FLASH
