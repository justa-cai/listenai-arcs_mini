if (DEFINED CONFIG_LISA_DISPLAY)
    listenai_library_named(lisa_display)
    
    listenai_library_sources(lisa_display.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_AXS15231B display_axs15231b.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_ST77926 display_st77926.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_ST7789P3 display_st7789p3.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_ST7789V display_st7789v.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_GC9309NA display_gc9309na.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_NV3030B display_nv3030b.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_GC9A01 display_gc9a01.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_UC8253C display_uc8253c.c)
    listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_SSD1683 display_ssd1683.c)

    listenai_include_directories(.)

    if (DEFINED CONFIG_USE_TRANS_CTX)
        add_subdirectory(trans_ctx)
    endif()

    add_subdirectory(common)
endif()
