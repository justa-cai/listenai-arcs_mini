# LISA Display Device Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_DISPLAY_DEVICE)

listenai_library_named(lisa_display_device)

listenai_library_sources(lisa_display_panel.c)
listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_display_arcs.c)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

add_subdirectory(bus)
add_subdirectory(panels)

endif() # CONFIG_LISA_DISPLAY_DEVICE
