if (DEFINED CONFIG_SDK_MODULE_GIFLIB)
    listenai_library_named(giflib)

    listenai_library_sources(
        dgif_lib.c
        egif_lib.c
        gifalloc.c
        gif_err.c
        gif_font.c
        gif_hash.c
        getarg.c
        openbsd-reallocarray.c
        qprintf.c
        quantize.c
    )

    listenai_include_directories(.)

    # 重定向内存函数
    #target_compile_definitions(giflib PRIVATE 
    #    -Dmalloc=pvPortMalloc 
    #    -Dfree=vPortFree 
    #    -Drealloc=pvPortRealloc
    #    -Dcalloc=pvPortCalloc
    #)
endif()
