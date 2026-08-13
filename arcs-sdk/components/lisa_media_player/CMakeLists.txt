# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_MEDIA_PLAYER)

set(LISA_MEDIA_PLAYER_SRCS "")
set(LISA_MEDIA_PLAYER_INCS "")

add_subdirectory(av_render)
add_subdirectory(avi_player)

listenai_library_named(lisa_media_player)

listenai_library_sources(
    ${LISA_MEDIA_PLAYER_SRCS}
)

listenai_include_directories(
    ${LISA_MEDIA_PLAYER_INCS}
)

endif() # CONFIG_LISA_MEDIA_PLAYER
