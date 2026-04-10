# BT Source Audio Interface

# Source模式：虚拟音频接口
if(CONFIG_LISA_BT_AUDIO_INTERFACE_VIRTUAL)
    listenai_library_sources(
        bt_audio_interface_virtual.c
    )

    listenai_include_directories(
        ./
    )
endif()
