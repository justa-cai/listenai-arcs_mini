if (CONFIG_MODULE_EASYFLASH)
    listenai_library_named(easyflash)

    listenai_library_sources(
        ${CMAKE_CURRENT_SOURCE_DIR}/easyflash/src/easyflash.c
        ${CMAKE_CURRENT_SOURCE_DIR}/easyflash/src/ef_env.c
        ${CMAKE_CURRENT_SOURCE_DIR}/easyflash/src/ef_utils.c

        ${CMAKE_CURRENT_SOURCE_DIR}/easyflash/port/ef_port_ls_arcs.c
    )
    listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/easyflash/inc)

endif()