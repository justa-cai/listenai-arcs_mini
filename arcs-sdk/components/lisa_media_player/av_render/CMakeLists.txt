# SPDX-License-Identifier: Apache-2.0

# Collect av_render sources/headers for parent lisa_media_player

set(LISA_MEDIA_PLAYER_SRCS
    ${LISA_MEDIA_PLAYER_SRCS}
    ${CMAKE_CURRENT_LIST_DIR}/src/av_render.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sync/sync.c
    ${CMAKE_CURRENT_LIST_DIR}/src/color_convert/rotate.c
    ${CMAKE_CURRENT_LIST_DIR}/src/render_impl/lisa_audio_render.c
    ${CMAKE_CURRENT_LIST_DIR}/src/render_impl/lisa_video_render.c
    ${CMAKE_CURRENT_LIST_DIR}/src/decoder/audio_decoder_mp3.c
    ${CMAKE_CURRENT_LIST_DIR}/src/decoder/video_decoder_mjpeg.c
    ${CMAKE_CURRENT_LIST_DIR}/src/decoder/video_decoder_lisa_jpeg.c
    PARENT_SCOPE
)

set(LISA_MEDIA_PLAYER_INCS
    ${LISA_MEDIA_PLAYER_INCS}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    PARENT_SCOPE
)
