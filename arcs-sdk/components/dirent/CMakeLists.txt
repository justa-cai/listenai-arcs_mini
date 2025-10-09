if (DEFINED CONFIG_DIRENT)
    listenai_library_named(dirent)
    if (CONFIG_DIRENT_EMPTY_IMPLEMENTATION)
        listenai_library_sources(dirent_empty.c)
    else()
        listenai_library_sources(dirent.c)
    endif()
    target_include_directories(dirent PRIVATE ./)
    target_include_directories(listenai_interface BEFORE INTERFACE
        ./
    )
endif()
