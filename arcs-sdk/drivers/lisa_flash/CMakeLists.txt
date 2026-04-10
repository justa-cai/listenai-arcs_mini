if(CONFIG_LISA_FLASH)

listenai_library_named(lisa_flash)

listenai_include_directories(.)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_flash_arcs.c)

endif() # CONFIG_LISA_FLASH
