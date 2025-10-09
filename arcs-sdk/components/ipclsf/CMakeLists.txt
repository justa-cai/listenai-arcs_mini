if (DEFINED CONFIG_IPC_LSF)

listenai_library_named(ipclsf)

listenai_library_sources(
    ic_message/ic_message.c
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

endif()
