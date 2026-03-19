listenai_include_directories(./)

listenai_library_named(lschat_server)

listenai_library_sources(
    mcp_tool_audio_url.c
    mcp_tool_exit.c
    mcp_tool_kuwo_music.c
    mcp_tool_play_ctrl.c
    voice_cloud.c
    voice_img_upload.c
)

add_subdirectory(wakeup)
add_subdirectory(mcp)
