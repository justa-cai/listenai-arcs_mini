if(CONFIG_ARCS_HAL_PM)
    set(TARGET_NAME arcs_hal_pm_impl)
    listenai_library_named(${TARGET_NAME})

    listenai_compile_definitions(CONFIG_PM=1)
    listenai_compile_definitions_ifdef(CONFIG_ARCS_HAL_PM_UART_WAKEUP CONFIG_PM_UART_WAKEUP=1)
    listenai_compile_definitions_ifdef(CONFIG_ARCS_HAL_PM_CLOSE_AP CONFIG_PM_CLOSE_AP=1)
    listenai_compile_definitions_ifdef(CONFIG_ARCS_HAL_PM_KEEP_ALIVE CONFIG_PM_KEEP_ALIVE=1)
    listenai_compile_definitions_ifdef(CONFIG_ARCS_HAL_PM_DEBUG CONFIG_PM_DEBUG=1)

    listenai_library_sources(
        pm_impl.c
        pm.c
    )

    listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )
else()
    listenai_library_named(arcs_hal_pm_stub)
    listenai_library_sources(pm_stub.c)
    listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

endif()
