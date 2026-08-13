# ARCS BOARD SUPPORT
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

# 创建板型库
listenai_library_named(module_boards)

# 添加源文件
listenai_library_sources(
    pinmux.c
    board.c
)

# 添加头文件路径
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

