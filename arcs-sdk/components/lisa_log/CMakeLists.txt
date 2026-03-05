listenai_library_named(lisa_log)

listenai_library_sources(
    lisa_log.c
)

if (DEFINED CONFIG_SDK_MODULE_EASYLOGGER)
    listenai_library_sources(lisa_log_frontend_easylog.c)
endif()

if (DEFINED CONFIG_LOG_BACKEND_UART)
    listenai_library_sources(lisa_log_backend_sys_uart.c)
endif()


listenai_include_directories(./)
