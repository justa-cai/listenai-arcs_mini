# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_APP_PLAYER)

listenai_library_named(app_player)

listenai_library_sources(
    app_player.c
    app_player_core.c
    pa_manager.c
)

if (CONFIG_APP_PLAYER_AUDIO_FOCUS)
    listenai_library_sources(app_player_focus.c)
endif()

listenai_include_directories(.)

add_subdirectory(player)
add_subdirectory(tone)

if (CONFIG_APP_PLAYER_AUDIO_FOCUS)
    add_subdirectory(focus_mgr)
endif()

endif() # CONFIG_APP_PLAYER
