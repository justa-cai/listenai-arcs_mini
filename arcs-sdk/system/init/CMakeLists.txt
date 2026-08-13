listenai_library_sources(
    sys_init.c
)

listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/sys-init-sections.ld
    SLOT ROM SORT_KEY "10-system"
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
