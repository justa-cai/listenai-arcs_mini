if (DEFINED CONFIG_RINGBUF)

FILE(GLOB SRCS ./*.c)
listenai_library_sources(${SRCS})
listenai_include_directories(./)

endif()
