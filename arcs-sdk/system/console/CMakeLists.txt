if(CONFIG_CONSOLE)

listenai_library_sources(
    console.c
)

listenai_library_sources_ifdef(CONFIG_CONSOLE_UART_BACKEND console_uart.c)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif() # CONFIG_CONSOLE
