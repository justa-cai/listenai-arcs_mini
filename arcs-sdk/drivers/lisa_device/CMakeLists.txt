# LISA Device Framework
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_DEVICE)

listenai_library_named(lisa_device)

listenai_include_directories(.)

listenai_library_sources(
    lisa_device.c
)

listenai_library_sources_ifdef(CONFIG_LISA_DEVICE_DEBUG lisa_device_debug.c)

endif() # CONFIG_LISA_DEVICE
