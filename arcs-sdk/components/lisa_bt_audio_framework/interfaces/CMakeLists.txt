# BT Audio Hardware Interfaces

# 接口管理器（必需）
listenai_library_sources(
    bt_audio_interface_mgr.c
)

# lisa_audio 接口实现
if(CONFIG_LISA_BT_AUDIO_INTERFACE_LOCAL)
    listenai_library_sources(
        lisa_audio_interface.c
        lisa_audio_resampler.c
        lisa_audio_playback_resample.c
    )
endif()

# 空接口实现（测试用）
if(CONFIG_LISA_BT_AUDIO_INTERFACE_NULL)
    listenai_library_sources(
        null_audio_interface.c
    )
endif()
