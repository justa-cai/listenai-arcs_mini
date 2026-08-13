# BT Audio Framework CMakeLists.txt

if(CONFIG_LISA_BT_AUDIO_FRAMEWORK)

    listenai_library_named(lisa_bt_audio_framework)
    listenai_include_directories(
        ./
        ./core/
        ./codecs/
        ./interfaces/
    )

    listenai_library_sources(
        core/bt_audio_framework.c
        core/bt_audio_codec_mgr.c
        core/bt_audio_session_mgr.c
        core/bt_audio_stream_lifecycle.c
    )

    # Codec适配器源文件
    if(CONFIG_AUDIO_CODEC_SBC)
        listenai_library_sources(
            codecs/bt_audio_codec_sbc.c
        )
    endif()

    if(CONFIG_AUDIO_CODEC_LC3)
        listenai_library_sources(
            codecs/bt_audio_codec_lc3.c
        )
    endif()

    if(CONFIG_AUDIO_CODEC_MSBC)
        listenai_library_sources(
            codecs/bt_audio_codec_msbc.c
        )
    endif()

    # === 硬件接口实现 ===
    listenai_library_sources(
        interfaces/bt_audio_interface_mgr.c
    )
    
    if(CONFIG_LISA_BT_AUDIO_INTERFACE_LOCAL)
        listenai_library_sources(
            interfaces/lisa_audio_interface.c
            interfaces/lisa_audio_resampler.c
            interfaces/lisa_audio_playback_resample.c
        )
    endif()
    
    if(CONFIG_LISA_BT_AUDIO_INTERFACE_NULL)
        listenai_library_sources(
            interfaces/null_audio_interface.c
        )
    endif()
    
    # === Sink/Source 场景适配器（需要蓝牙协议栈）===
    listenai_add_subdirectory_ifdef(CONFIG_BT_CLASSIC_ROLE_SINK bt_sink)
    listenai_add_subdirectory_ifdef(CONFIG_BT_CLASSIC_ROLE_SOURCE bt_source)

    # 蓝牙音频适配层（桥接蓝牙协议栈）
    if(CONFIG_LISA_BT_AUDIO_ADAPTER)
        listenai_library_sources(
            bt_audio_adapter.c
        )
    endif()

endif()
