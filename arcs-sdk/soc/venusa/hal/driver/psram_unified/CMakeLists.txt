listenai_library_named(venusa_ps_ram_unified)

listenai_library_sources(
    PSRAM_Unified.c
    PSRAM_Unified_Common.c
    die/xccela/xccela_config.c
    die/apm/apm_config.c
)

target_include_directories(venusa_ps_ram_unified PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
