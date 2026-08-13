# SPDX-License-Identifier: Apache-2.0

# Collect avi_player sources/headers for parent lisa_media_player

set(LISA_MEDIA_PLAYER_SRCS
    ${LISA_MEDIA_PLAYER_SRCS}
    ${CMAKE_CURRENT_LIST_DIR}/src/avi_player.c
    ${CMAKE_CURRENT_LIST_DIR}/src/avi_io_lsfs.c
    ${CMAKE_CURRENT_LIST_DIR}/src/avi_io_http_range.c
    ${CMAKE_CURRENT_LIST_DIR}/src/avifile.c
    PARENT_SCOPE
)

set(LISA_MEDIA_PLAYER_INCS
    ${LISA_MEDIA_PLAYER_INCS}
    ${CMAKE_CURRENT_LIST_DIR}/include
    PARENT_SCOPE
)
