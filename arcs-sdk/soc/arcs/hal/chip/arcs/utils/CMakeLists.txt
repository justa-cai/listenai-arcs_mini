if(CONFIG_ARCS_HAL_UTILS)
    listenai_library_named(arcs_hal_utils)

    listenai_library_sources(
        ls_utils.c
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

endif()

