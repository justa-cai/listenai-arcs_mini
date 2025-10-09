
set(SOURCES
    "src/array.c"
    "src/common.c"
    "src/deque.c"
    "src/hashset.c"
    "src/hashtable.c"
    "src/pqueue.c"
    "src/queue.c"
    "src/ring_buffer.c"
    "src/slist.c"
    "src/stack.c"
    "src/treeset.c"
    "src/treetable.c"
)

if(CONFIG_SDK_MODULE_COLLECTIONS_C_LIST_COMPILE_EN)
    list(APPEND SOURCES "src/list.c")
endif()

listenai_library_named(cc)
listenai_library_sources(${SOURCES})
listenai_include_directories(src/)

if(DEFINED CONFIG_SDK_MODULE_COLLECTIONS_C_WITHOUT_PREFIX)
    listenai_include_directories(src/cc)
endif()

if(DEFINED CONFIG_SDK_MODULE_COLLECTIONS_C_CUSTOM_INCLUDE)
    target_compile_definitions(cc PUBLIC -DCC_CUSTOM_INCLUDE="${CONFIG_SDK_MODULE_COLLECTIONS_C_CUSTOM_INCLUDE_PATH}")
endif()
