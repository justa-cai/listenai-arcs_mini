# BT Sink - Bluetooth Audio Sink Scenario

# Sink 场景：蓝牙音频接收（需要 local audio 接口）
if(CONFIG_LISA_BT_AUDIO_INTERFACE_LOCAL)
    listenai_library_sources(
        bt_sink.c
    )

    listenai_include_directories(
        ./
    )
endif()
