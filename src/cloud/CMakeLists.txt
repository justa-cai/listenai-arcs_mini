listenai_library_named(cloud)

listenai_library_sources(
    # Main cloud files
    app_cloud.c
    haoxueduo.c
    
    # Core components
    core/lisa_aiui.c
    core/lisa_aiui_rid_man.c
    
    # MCP components
    mcp/aiui_mcp.c
    mcp/mcp_integration.c
    mcp/vision_config.c
    
    # MCP tools
    mcp/tools/audio_url.c
    mcp/tools/brightness_control.c
    mcp/tools/emotion_control.c
    mcp/tools/exit_skill.c
    mcp/tools/kuwo_music.c
    mcp/tools/led_control.c
    mcp/tools/model_config_control.c
    mcp/tools/photo_recognition.c
    mcp/tools/play_control.c
    mcp/tools/show_image.c
    mcp/tools/show_qrcode.c
    mcp/tools/switch_full_duplex.c
    mcp/tools/version_info.c
    mcp/tools/volume_control.c
    
    # Process manager
    proc/proc_mgr.c
    
    # Recognizer
    recognizer/recognizer.c
    
    # Stream text
    stream_text/lsc_stream_text.c
    stream_text/lsc_stream_text_thread.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/proc
    ${CMAKE_CURRENT_SOURCE_DIR}/recognizer
    ${CMAKE_CURRENT_SOURCE_DIR}/stream_text
    # ${CMAKE_CURRENT_SOURCE_DIR}/../arcs-sdk/modules/ijg
)
