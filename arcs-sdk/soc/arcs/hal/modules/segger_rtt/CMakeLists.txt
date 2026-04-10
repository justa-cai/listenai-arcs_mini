
if (CONFIG_SUPPORT_SEGGER_RTT)

listenai_library_sources(
    SEGGER_RTT.c
    SEGGER_RTT_printf.c
    SEGGER_RTT_Syscalls_GCC.c
)


listenai_include_directories(
    ./include
)

listenai_compile_definitions(
    -DBUFFER_SIZE_UP=${CONFIG_SEGGER_RTT_BUFFER_SIZE_UP}
    -DBUFFER_SIZE_DOWN=${CONFIG_SEGGER_RTT_BUFFER_SIZE_DOWN}
    -DSEGGER_RTT_MODE_DEFAULT=${CONFIG_SEGGER_RTT_MODE_DEFAULT}
)
endif()
