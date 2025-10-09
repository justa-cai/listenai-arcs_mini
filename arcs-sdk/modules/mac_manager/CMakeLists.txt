
message(STATUS "CONFIG_MAC_MANAGER: ${CONFIG_MAC_MANAGER}")
if(DEFINED CONFIG_MAC_MANAGER)
listenai_library_named(mac_manager)

listenai_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mac_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mac_manager_ops.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port/mem/sys_heap/mac_manager_mem_ops_sys_heap.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port/mem/sys_heap
)

if (CONFIG_MAC_MANAGER_MAC_IN_LISA_KV)
    listenai_library_sources(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/port/content/lisa_kv/mac_manager_content_ops_lisa_kv.c
    )
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/port/content/lisa_kv
    )
elseif(CONFIG_MAC_MANAGER_MAC_BY_CHIP_ID)
    listenai_library_sources(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/port/content/chip_id/mac_manager_content_ops_chip_id.c
    )
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/port/content/chip_id
    )
elseif(CONFIG_MAC_MANAGER_MAC_GET_FROM_CHIP_ID_SAVE_TO_LISA_KV)
    listenai_library_sources(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/port/content/from_chip_id_save_to_lisa_kv/mac_manager_content_ops_from_chip_id_save_to_lisa_kv.c
    )
    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/port/content/from_chip_id_save_to_lisa_kv
    )
else()
    message(FATAL_ERROR "Only support MAC in lisa kv or chip id now")
endif() # CONFIG_MAC_MANAGER_MAC_IN_LISA_KV

endif()