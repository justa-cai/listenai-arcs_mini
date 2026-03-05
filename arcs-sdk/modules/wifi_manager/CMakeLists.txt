if(CONFIG_WIFI_MANAGER)

listenai_library_named(wifi_manager)

target_include_directories(wifi_manager PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(wifi_manager PRIVATE 
    src/wifi_manager.c
    src/wifi_manager_ops.c
    src/wifi_manager_storage.c
    src/wifi_manager_storage_cache.c
    src/wifi_manager_storage_kv.c
    src/wifi_manager_storage_blob_legacy.c
    src/wifi_manager_storage_migrate.c
)
  
if(CONFIG_WIFI_MANAGER_WIFI_DEV_ARCS)
  target_sources(wifi_manager PRIVATE 
      port/wifi_ops/arcs/wifi_arcs.c
  )
  listenai_include_directories(wifi_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/port/wifi_ops/arcs
  )
endif()

if(CONFIG_WIFI_MANAGER_MEM_HEAP_CAPS)
  target_sources(wifi_manager PRIVATE 
      port/mem/heap_caps/wifi_manager_mem_ops_heap_caps.c
  )

  listenai_include_directories(wifi_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/port/mem/heap_caps
  )
endif()

if(CONFIG_WIFI_MANAGER_OS_LISA)
  target_sources(wifi_manager PRIVATE 
      port/os/lisa/wifi_manager_os_ops_lisa.c
  )
  listenai_include_directories(wifi_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/port/os/lisa
  )
endif()

if(CONFIG_WIFI_MANAGER_WIFI_TYPE_STATION)
  target_compile_definitions(wifi_manager PRIVATE CONFIG_WIFI_DEV_STATION)
endif()


target_link_libraries(wifi_manager PUBLIC arcs-hal)

if(DEFINED LISTENAI_CMAKE_PATH)
listenai_link_libraries(wifi_manager)
endif()

endif()
