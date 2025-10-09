if (DEFINED CONFIG_SDK_MODULE_LTLSF)
    listenai_library_named(ltlsf)
    
    listenai_library_sources(ltlsf.c)
    listenai_include_directories(.)

endif()
