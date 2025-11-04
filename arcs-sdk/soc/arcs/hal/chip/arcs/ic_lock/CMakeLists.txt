if(CONFIG_ARCS_HAL_IC_LOCK)
    listenai_library_named(arcs_hal_ic_lock)

    listenai_compile_definitions(
        CFG_CRYPTO_IC_MUTEX=1
    )

    listenai_library_sources(
        ic_lock.c
        ic_mutex.c
        ic_platform.c
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARCS_HAL_LIB_PATH}/chip/arcs/include
    )

endif()
    
