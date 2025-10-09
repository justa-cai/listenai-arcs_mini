if (DEFINED CONFIG_CORE_SPINLOCK)
    listenai_library_named(core_spinlock)
    
    listenai_library_sources(core_spinlock.c)

    listenai_include_directories(.)
endif()
