if (CONFIG_ZLIB)

    listenai_library_named(zlib)

    set(ZLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib)

    list(APPEND ZLIB_CORE_SRCS
        ${ZLIB_DIR}/adler32.c
        ${ZLIB_DIR}/compress.c
        ${ZLIB_DIR}/crc32.c
        ${ZLIB_DIR}/deflate.c
        ${ZLIB_DIR}/inflate.c
        ${ZLIB_DIR}/infback.c
        ${ZLIB_DIR}/inftrees.c
        ${ZLIB_DIR}/inffast.c
        ${ZLIB_DIR}/trees.c
        ${ZLIB_DIR}/uncompr.c
        ${ZLIB_DIR}/zutil.c
    )

    list(APPEND ZLIB_GZIP_SRCS
        ${ZLIB_DIR}/gzclose.c
        ${ZLIB_DIR}/gzlib.c
        ${ZLIB_DIR}/gzread.c
        ${ZLIB_DIR}/gzwrite.c
    )

    list(APPEND ZLIB_GZIP_SRCS
        ${ZLIB_DIR}/contrib/minizip/ioapi.c
        ${ZLIB_DIR}/contrib/minizip/unzip.c
        ${ZLIB_DIR}/contrib/minizip/zip.c
    )

    if (CONFIG_ZLIB_SOLO)
        listenai_library_sources(${ZLIB_CORE_SRCS})
        # zephyr_library_compile_definitions(Z_SOLO)
        listenai_compile_definitions(Z_SOLO)
    else()
        listenai_library_sources(${ZLIB_CORE_SRCS} ${ZLIB_GZIP_SRCS})
    endif()

    listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include  ${ZLIB_DIR}/contrib/minizip ${ZLIB_DIR})

    # taoyun
    # zephyr_library_compile_definitions(PPM_SUPPORTED)
    # zephyr_library_compile_definitions(HAVE_MATH_H)
    # zephyr_library_compile_definitions(HAVE_CTYPE_H)
    # # zephyr_library_compile_definitions(HAVE_UNISTD_H)
    # zephyr_library_compile_definitions(HAVE_ERRNO_H)

    # zlib
    # zephyr_library_compile_definitions(HAVE_SYS_TYPES_H)
    # zephyr_library_compile_definitions(HAVE_STDINT_H)
    # zephyr_library_compile_definitions(HAVE_STDDEF_H)
    # zephyr_library_compile_definitions(_LARGEFILE64_SOURCE=1)
    # zephyr_library_compile_definitions(Z_HAVE_UNISTD_H)
    
    
    # taoyun
    listenai_library_compile_options(PRIVATE -DMINIZIP_FOPEN_NO_64)
    # listenai_library_compile_options(PRIVATE -DPPM_SUPPORTED)
    # listenai_library_compile_options(PRIVATE -DHAVE_MATH_H)
    # listenai_library_compile_options(PRIVATE -DHAVE_CTYPE_H)
    # listenai_library_compile_options(PRIVATE -DHAVE_UNISTD_H)
    # listenai_library_compile_options(PRIVATE -DHAVE_ERRNO_H)
    listenai_library_compile_options(PRIVATE -Wno-unused-function)
    listenai_library_compile_options(PRIVATE -Wno-implicit-int)
    listenai_library_compile_options(PRIVATE -Wno-return-type)
    listenai_library_compile_options(PRIVATE -Wno-format-extra-args)
    listenai_library_compile_options(PRIVATE -Wno-unused-parameter)
    listenai_library_compile_options(PRIVATE -Wno-unused-const-variable)


    #target_link_libraries(zlib PUBLIC embedded_file_io elmfat)

endif()