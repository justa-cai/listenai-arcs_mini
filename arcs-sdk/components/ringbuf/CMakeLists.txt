if (DEFINED CONFIG_RINGBUF)

listenai_library_named(psram_ringbuf)
FILE(GLOB SRCS ./*.c)
listenai_library_sources(${SRCS})
listenai_include_directories(./)

endif()
