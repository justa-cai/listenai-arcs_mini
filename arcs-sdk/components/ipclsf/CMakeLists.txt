if (CONFIG_IPC_LSF OR CONFIG_IPC_CIDU)

listenai_library_named(ipclsf)

listenai_library_sources_ifdef(
    CONFIG_IPC_LSF
    ic_message/ic_message.c
)

listenai_library_sources_ifdef(
    CONFIG_IPC_CIDU
    ic_message/ic_message_cidu.c
)

listenai_library_sources_ifdef(
    CONFIG_IC_MESSAGE_AUTO_INIT
    ic_message/ic_message_auto_init.c
)

listenai_library_sources_ifdef(
    CONFIG_DISK_MEM_CLIENT
    diskmem/client/disk_mem_urpc.c
)

listenai_library_sources_ifdef(
    CONFIG_DISK_MEM_SERVER
    diskmem/server/disk_mem_urpc.c
)

if (CONFIG_DISK_MEM_SERVER)
listenai_include_directories(diskmem/server)
endif()

if (CONFIG_DISK_MEM_CLIENT)
listenai_include_directories(diskmem/client)
endif()

target_include_directories(ipclsf PRIVATE port)

listenai_include_directories(ic_message)

if (CONFIG_IPC_CIDU)
listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/linker/ipc-cidu-check.ld
    SLOT SECTIONS_END
    SORT_KEY "30-component"
)
endif()

endif()
