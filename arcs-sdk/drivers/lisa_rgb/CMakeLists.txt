# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_RGB_DEVICE)

listenai_library_named(lisa_rgb_device)

listenai_library_sources(
    lisa_rgb_arcs.c
)
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

endif() # CONFIG_LISA_RGB_DEVICE
