if (CONFIG_ARCS_HAL_VRTC)
    listenai_library_named(arcs_hal_vrtc)

    listenai_library_sources(
        vrtc.c
    )

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/chip/arcs/include/wifi
    )

endif()
