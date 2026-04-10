# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

listenai_link_libraries(m)
# lisa_player.a路径在modules/lisa_player下
# listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/liblisa_player.a)
# listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/libplatform_track.a)
listenai_include_directories(./)
listenai_library_sources(lisa_player_adapter.c)