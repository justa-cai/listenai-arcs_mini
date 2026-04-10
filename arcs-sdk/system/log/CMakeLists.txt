listenai_library_sources(
    lisa_log.c
)

if (DEFINED CONFIG_SDK_MODULE_EASYLOGGER)
    listenai_library_sources(lisa_log_frontend_easylog.c)
endif()

if (DEFINED CONFIG_LOG_BACKEND_CONSOLE)
    listenai_library_sources(lisa_log_backend_console.c)
endif()

if (DEFINED CONFIG_LOG_BACKEND_SEGGER_RTT)
    listenai_library_sources(lisa_log_backend_segger_rtt.c)
endif()

listenai_include_directories(./)
