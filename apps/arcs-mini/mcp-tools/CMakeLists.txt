listenai_library_named(app-mcp-tools)
listenai_library_sources(
    mcp_tool_brightness.c
    mcp_tool_volume.c
    mcp_tool_device_info.c
    mcp_tool_alarm.c
    mcp_tool_led.c
    mcp_tool_camera.c
    mcp_tool_image.c
    mcp_tool_duplex.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
