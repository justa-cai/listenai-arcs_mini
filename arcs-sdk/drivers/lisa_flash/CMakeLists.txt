if(CONFIG_LISA_FLASH)

listenai_library_named(lisa_flash)

listenai_include_directories(.)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_flash_arcs.c)
listenai_library_sources_ifdef(CONFIG_SOC_VENUSA  lisa_flash_venusa.c)

if(CONFIG_LISA_FLASH_HALT_REMOTE_CORE)
    listenai_library_sources(lisa_flash_venusa_hooks.c)
endif()

if(CONFIG_LISA_FLASH_VENUSA)
    # VENUSA cannot fetch from XIP flash while the same flash is being
    # programmed/erased. This driver is initialized early, so keep it in SRAM
    # instead of PSRAM to avoid depending on PSRAM execution during boot.
    listenai_code_relocate(LIBRARY lisa_flash LOCATION SRAM)
endif()

endif() # CONFIG_LISA_FLASH
