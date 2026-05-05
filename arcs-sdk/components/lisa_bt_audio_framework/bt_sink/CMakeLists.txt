# BT Sink - Bluetooth Audio Sink Scenario

# Sink 场景：蓝牙音频接收
# 支持 LOCAL 接口（物理硬件）和 NULL 接口（虚拟/测试）
if(CONFIG_LISA_BT_AUDIO_INTERFACE_LOCAL OR CONFIG_LISA_BT_AUDIO_INTERFACE_NULL)
    listenai_library_sources(
        bt_sink.c
    )

    listenai_include_directories(
        ./
    )
endif()
