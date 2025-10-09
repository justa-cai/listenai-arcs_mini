if (DEFINED CONFIG_SDK_MODULE_LIBPNG)

    listenai_library_named(libpng)

    set(SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../)

    set(PNG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libpng16")

    list(APPEND  png_sources
        ${PNG_SOURCE_DIR}/png.c
        ${PNG_SOURCE_DIR}/pngerror.c
        ${PNG_SOURCE_DIR}/pngget.c
        ${PNG_SOURCE_DIR}/pngmem.c
        ${PNG_SOURCE_DIR}/pngpread.c
        ${PNG_SOURCE_DIR}/pngread.c
        ${PNG_SOURCE_DIR}/pngrio.c
        ${PNG_SOURCE_DIR}/pngrtran.c
        ${PNG_SOURCE_DIR}/pngrutil.c
        ${PNG_SOURCE_DIR}/pngset.c
        ${PNG_SOURCE_DIR}/pngtrans.c
        ${PNG_SOURCE_DIR}/pngwio.c
        ${PNG_SOURCE_DIR}/pngwrite.c
        ${PNG_SOURCE_DIR}/pngwtran.c
        ${PNG_SOURCE_DIR}/pngwutil.c
    )

    # 添加源文件
    listenai_library_sources(${png_sources})

    # 添加包含目录，包括 zlib 的 include 目录
    listenai_include_directories("${CMAKE_CURRENT_SOURCE_DIR}"  "${PNG_SOURCE_DIR}")

    # 添加zlib 
    listenai_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../zlib/include" )
    listenai_include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../zlib/zlib" )

endif()