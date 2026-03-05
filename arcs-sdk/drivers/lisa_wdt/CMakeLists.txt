# LISA WDT Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_WDT)

listenai_library_named(lisa_wdt)

listenai_library_sources(
    lisa_wdt_arcs.c
)

listenai_include_directories(.)

endif() # CONFIG_LISA_WDT
