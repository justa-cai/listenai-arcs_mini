if (CONFIG_LWIP_HOOK_NETCONN_EXTERNAL_RESOLVE)

listenai_library_named(custom_dns)
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

listenai_library_sources(
    dns_reslove.c
)

if(CONFIG_HTTPDNS)
    listenai_library_sources(
    dns_reslove.c
    httpdns.c
    httpdns_cache.c
)
endif()

if(CONFIG_HTTPDNS_USE_ALIYUN)
    listenai_library_sources(httpdns_aliyun.c)
endif()
endif()