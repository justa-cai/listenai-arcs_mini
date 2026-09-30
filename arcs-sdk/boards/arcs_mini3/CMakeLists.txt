# ARCS BOARD SUPPORT
#
# Copyright (c) 2026, LISTENAI
# SPDX-License-Identifier: Apache-2.0

listenai_library_named(module_boards)

listenai_library_sources(
    pinmux.c
    board.c
)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
