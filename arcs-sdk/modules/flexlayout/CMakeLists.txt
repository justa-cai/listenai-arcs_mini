if (DEFINED CONFIG_SDK_MODULE_FLEXLAYOUT)
    listenai_library_named(flexlayout)
    
    listenai_library_sources(src/FlexLayout.c)
    listenai_include_directories(src/)

    # 重定向内存函数
    #target_compile_definitions(flexlayout PRIVATE 
    #    -Dmalloc=pvPortMalloc 
    #    -Dfree=vPortFree 
    #    -Drealloc=pvPortRealloc
    #    -Dcalloc=pvPortCalloc
    #)
endif()
