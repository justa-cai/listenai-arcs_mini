if(DEFINED CONFIG_SDK_MODULE_EASYLOGGER)
    listenai_library_named(easylogger)

    set(port_file "${CMAKE_CURRENT_SOURCE_DIR}/port_arcs/elog_port.c")

    # 获取其他源文件
    file(GLOB core_sources "${CMAKE_CURRENT_SOURCE_DIR}/easylogger/src/*.c")

    # 合并所有源文件
    set(source_files ${port_file} ${core_sources})

    listenai_library_sources(${source_files})
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/port_arcs
        ${CMAKE_CURRENT_SOURCE_DIR}/easylogger/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/..//arcs-hal/chip/arcs/include/
     )

    if(DEFINED CONFIG_LOG_ASYNC)
        listenai_link_libraries(arcs-hal heap)
    endif()

endif()
