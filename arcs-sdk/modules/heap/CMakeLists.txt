if(CONFIG_MODULE_HEAP)
    set(TOP_PATH ${CMAKE_HOME_DIRECTORY})
    set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})

    listenai_library_named(heap)
    
    set(LIB_SRCS "heap_caps_base.c"
        "heap_caps.c"
        "heap_caps_init.c"
        "multi_heap.c"
        "tlsf/tlsf.c"
        "port/arcs.c"
    )

    if(CONFIG_HEAP_TASK_TRACKING)
       listenai_library_sources("heap_task_info.c")
    endif()
    if(NOT CONFIG_HEAP_POISONING_DISABLED)
        listenai_library_sources("multi_heap_poisoning.c")
    endif()

    if(CONFIG_HEAP_TRACING_STANDALONE)
        listenai_library_sources("heap_trace_standalone.c")
        
        set_source_files_properties(heap_trace_standalone.c 
             PROPERTIES COMPILE_FLAGS -Wno-frame-address)
    endif()

    listenai_library_sources(${LIB_SRCS})
    listenai_include_directories(
        ${LIB_PATH}/include
        ${LIB_PATH}/include/idf
        ${LIB_PATH}/include/soc
        ${LIB_PATH}
        ${LIB_PATH}/tlsf
        ${LIB_PATH}/tlsf/include
        ${TOP_PATH}/include
        ${TOP_PATH}/shell
        ${TOP_PATH}/rtos/include
        ${TOP_PATH}/modules/arcs-hal/chip/arcs/include
        ${TOP_PATH}/modules/arcs-hal/chip/arcs/include/register
        # ${TOP_PATH}/modules/arcs-hal/chip/arcs/bsp
        # ${TOP_PATH}/modules/arcs-hal/include/bsp
        ${TOP_PATH}/modules/arcs-hal/include/NMSIS/Core/Include
    )
endif()

