# LISA Misc CH32 Driver
#
# Copyright (c) 2026, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_CH32V003 AND CONFIG_SOC_ARCS)

listenai_library_named(lisa_misc)

listenai_library_sources(lisa_misc_ch32.c)

listenai_include_directories(.)

endif() # CONFIG_LISA_CH32V003 AND CONFIG_SOC_ARCS
