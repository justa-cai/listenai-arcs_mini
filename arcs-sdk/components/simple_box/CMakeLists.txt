if (DEFINED CONFIG_SIMPLE_BOX)
    listenai_library_named(simple_box)

    listenai_library_sources(
        ls.c
        cd.c
        touch.c
        mkdir.c
        rm.c
        cp.c
        pwd.c
        util/utils.c
    )
    target_include_directories(simple_box PRIVATE ./)
endif()
