if (CONFIG_ARCS_HAL_PM)
    set(TARGET_NAME arcs_hal_pm_impl)
    listenai_library_named(${TARGET_NAME})

    listenai_compile_definitions(CONFIG_PM=1)

    listenai_library_sources(pm_impl.c)

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )
else()
    listenai_library_named(arcs_hal_pm_stub)
    listenai_library_sources(pm_stub.c)

endif()
