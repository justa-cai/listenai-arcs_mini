if (DEFINED CONFIG_LISA_TOUCH)
    listenai_library_named(lisa_touch)
    
    listenai_library_sources(lisa_touch.c)
    listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_AXS15231B touch_axs15231b.c)
    listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_ST77921 touch_st77921.c)
    listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_CST816D touch_cst816d.c)
    listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_FT5336 touch_ft5336.c)
    listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_BL6133 touch_bl6133.c)
    listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_CST328 touch_cst328.c)

    listenai_include_directories(.)
endif()
