if (CONFIG_MODULE_SHELL)
    set(TOP_PATH ${CMAKE_HOME_DIRECTORY})
    set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    file(GLOB_RECURSE LIB_SRCS "*.c")

    listenai_library_named(shell)
    listenai_library_sources(${LIB_SRCS})
    listenai_include_directories(
        ${LIB_PATH}
    )

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )
endif()