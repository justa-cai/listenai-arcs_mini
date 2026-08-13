# LISA RTC Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_RTC)

listenai_library_named(lisa_rtc)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS    lisa_rtc_arcs.c)
listenai_library_sources_ifdef(CONFIG_SOC_VENUSA  lisa_rtc_venusa.c)

listenai_include_directories(.)

endif() # CONFIG_LISA_RTC
