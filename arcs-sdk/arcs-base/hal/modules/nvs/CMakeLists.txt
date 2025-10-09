
listenai_library_sources(
    crc8_sw.c
    crc32_sw.c
)

if(CONFIG_ARCS_HAL_NVS)
    listenai_library_named(arcs_hal_nvs)

    listenai_compile_definitions(
        CFG_NVS=1
    )

    listenai_library_sources(
        nvs.c
    )

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

    listenai_include_directories(
        ${ARCS_HAL_LIB_PATH}/modules/nvs/include
    )

endif()
