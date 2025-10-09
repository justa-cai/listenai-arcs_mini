if (DEFINED CONFIG_RTOS_AL)
    listenai_library_named(rtos_al)
    
    listenai_library_sources(
        ${CMAKE_CURRENT_SOURCE_DIR}/rtos_al.c
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    listenai_link_libraries(freertos)

endif()
