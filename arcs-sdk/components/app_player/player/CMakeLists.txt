# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

# add_subdirectory(lisa_player)
listenai_link_libraries(m)
listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/liblisa_player.a)
listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/libplatform_track.a)
listenai_include_directories(./)
listenai_library_sources(lisa_player_adapter.c)