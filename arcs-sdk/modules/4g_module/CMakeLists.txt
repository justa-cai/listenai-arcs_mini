if (CONFIG_4G_MODULE)
listenai_library_named(4g_modem)

# Collect all source files from modem/ml307, excluding example file
file(GLOB ML307_SOURCES modem/ml307/*.c)
# list(FILTER ML307_SOURCES EXCLUDE REGEX ".*ml307_example\\.c$")

listenai_library_sources(
    at_uart.c
    lisa_4g_module.c
    ${ML307_SOURCES}
    ../../components/ringbuf/ring_buffer.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modem/ml307
    ../../components/ringbuf
    ../../drivers/lisa_device
    ../../drivers/lisa_uart
)
endif()