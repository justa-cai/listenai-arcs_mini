if(DEFINED CONFIG_VENDOR_STORAGE_ENABLE)

listenai_library_named(vendor_storage)

listenai_library_sources(
    vendor_ops.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
endif()
