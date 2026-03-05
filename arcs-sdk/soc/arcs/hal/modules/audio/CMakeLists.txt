if (DEFINED CONFIG_BT_AUDIO)
    listenai_library_named(bt_audio)
    listenai_library_sources(
        aud_alg/resample/resample.c
        aud_mgr/aud_mgr_buf.c
        aud_mgr/aud_mgr_lc3.c
        aud_mgr/aud_mgr_sbc.c
        aud_mgr/aud_mgr.c
        aud_mgr/aud_sync.c
        aud_mgr/aud_dac.c
        aud_pro/aud_pro.c
        app_aud.c
        aud_msg.c
    )
    target_include_directories(bt_audio PUBLIC 
        api
    )
    listenai_include_directories(api)
endif()
