if(CONFIG_LISA_DVP_DEVICE)

listenai_library_named(lisa_dvp)

listenai_library_sources(
    lisa_dvp_arcs.c
)

listenai_include_directories(.)

endif() # CONFIG_LISA_DVP_DEVICE
