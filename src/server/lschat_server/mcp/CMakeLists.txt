listenai_library_named(mcp)

listenai_library_sources(
    mcp.c
)

listenai_include_directories(
    ./
)

listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/mcp_tool.ld
    SLOT COMPONENTS
    SORT_KEY "50-app"
)
