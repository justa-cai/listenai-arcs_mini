if(DEFINED CONFIG_SDK_MODULE_EASYLOGGER)
    listenai_library_named(easylogger)

    file(GLOB core_sources "${CMAKE_CURRENT_SOURCE_DIR}/easylogger/src/*.c")

    listenai_library_sources(
        "${CMAKE_CURRENT_SOURCE_DIR}/port_arcs/elog_port.c"
        ${core_sources}
    )
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/port_arcs
        ${CMAKE_CURRENT_SOURCE_DIR}/easylogger/inc
     )

    if(DEFINED CONFIG_LOG_ASYNC)
        listenai_link_libraries(hal heap)
    endif()

endif()
