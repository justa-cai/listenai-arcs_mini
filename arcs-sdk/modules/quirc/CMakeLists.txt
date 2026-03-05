if (DEFINED CONFIG_SDK_MODULE_QUIRC)
    listenai_library_named(quirc)
    
    listenai_library_sources(
        lib/quirc.c
        lib/decode.c
        lib/identify.c
        lib/version_db.c
    )
    
    listenai_include_directories(lib)
    
    # Link math library for sqrt, rint functions
    listenai_link_libraries(m)

endif()
