listenai_library_named(app-mcp-tools)
listenai_library_sources(
    mcp_tool_brightness.c
    mcp_tool_volume.c
    mcp_tool_device_info.c
    mcp_tool_device_control.c
    mcp_tool_alarm.c
    mcp_tool_led.c
    mcp_tool_camera.c
    mcp_tool_image.c
    mcp_tool_duplex.c
    mcp_tool_sd_music.c
)

if(CONFIG_LOG_UPLOAD)
    listenai_library_sources(
        mcp_tool_upload.c
    )
endif()

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
