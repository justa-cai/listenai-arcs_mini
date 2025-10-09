if (DEFINED CONFIG_LISA_KV)
    listenai_library_named(lisa_kv)

    listenai_library_sources_ifdef(
        CONFIG_LISA_KV_TYPE_LSFS
        fs/lisa_kv.c
        fs/pri/base64.c
    )

    listenai_library_sources_ifdef(
        CONFIG_LISA_KV_TYPE_EF
        ef/lisa_kv.c
    )

    if (DEFINED CONFIG_LISA_KV_TYPE_LSFS)
        target_include_directories(lisa_kv PRIVATE fs/pri)
    endif()

    target_include_directories(lisa_kv PRIVATE priv)

    listenai_include_directories(.)

endif()
