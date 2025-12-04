FILE(GLOB SRCS
    acomp_stream.c
)
listenai_library_sources(${SRCS})

target_include_directories(${TARGET_NAME} PUBLIC ./stream)
