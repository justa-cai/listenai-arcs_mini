if(CONFIG_VIDEO_PLAYER)

listenai_library_named(video_player)

listenai_library_sources(
    video_player.c
    tsdemux.c
)

listenai_include_directories(
    ./
)

endif() # CONFIG_VIDEO_PLAYER
