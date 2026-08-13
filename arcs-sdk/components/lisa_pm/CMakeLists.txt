if(CONFIG_LISA_PM)

listenai_library_named(lisa_pm)

if(CONFIG_LISA_PM_CORE_PRIMARY)
    listenai_compile_definitions(
        PM_CORE_PRIMARY=1
        CFG_VRTC=1
    )
else()
    if(NOT CONFIG_ARCS_HAL_IPC)
        message(FATAL_ERROR "lisa_pm: non-primary PM core requires CONFIG_ARCS_HAL_IPC")
    endif()

    listenai_compile_definitions(
        PM_CORE_PRIMARY=0
        CFG_VRTC_PROXY=1
    )
endif()

listenai_library_sources(
    src/lisa_pm_core.c
    src/lisa_pm_device.c
    src/lisa_pm_porting.c
)

set(lisa_pm_porting_sources)

if(CONFIG_SOC_ARCS)
    list(APPEND lisa_pm_porting_sources
        src/porting/arcs/lisa_pm_porting_arcs.c
    )
    if(CONFIG_LISA_PM_REMOTE_LOCK_CLIENT)
        list(APPEND lisa_pm_porting_sources
            src/porting/arcs/lisa_pm_remote_lock_client_arcs.c
        )
    endif()
    if(CONFIG_LISA_PM_REMOTE_LOCK_SERVER)
        list(APPEND lisa_pm_porting_sources
            src/porting/arcs/lisa_pm_remote_lock_server_arcs.c
        )
    endif()
endif()

if(lisa_pm_porting_sources)
    listenai_library_sources(${lisa_pm_porting_sources})
else()
    message(FATAL_ERROR "lisa_pm: unsupported SoC porting implementation")
endif()

listenai_library_sources_ifdef(CONFIG_LISA_PM_STATS
    src/lisa_pm_stats.c
)

listenai_library_sources_ifdef(CONFIG_LISA_PM_WIFI
    src/lisa_pm_wifi.c
)

if(CONFIG_LISA_PM_REMOTE_LOCK_CLIENT)
    listenai_library_sources(src/lisa_pm_remote_lock.c)
endif()

listenai_include_directories(include)

endif()
