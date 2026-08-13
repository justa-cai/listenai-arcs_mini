# LISA Hardware Timer Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_HWTIMER_DEVICE)

listenai_library_named(lisa_hwtimer_device)

if(CONFIG_LISA_HWTIMER_GPT_TIMER)
    listenai_library_sources_ifdef(CONFIG_SOC_ARCS   arcs_gpt_timer.c)
    listenai_library_sources_ifdef(CONFIG_SOC_VENUSA venusa_gpt_timer.c)
endif()

if(CONFIG_LISA_HWTIMER_AON_TIMER)
    listenai_library_sources_ifdef(CONFIG_SOC_ARCS   arcs_aon_timer.c)
    listenai_library_sources_ifdef(CONFIG_SOC_VENUSA venusa_aon_timer.c)
endif()

if(CONFIG_LISA_HWTIMER_DUAL_TIMER)
    listenai_library_sources_ifdef(CONFIG_SOC_ARCS   arcs_dual_timer.c)
    listenai_library_sources_ifdef(CONFIG_SOC_VENUSA venusa_dual_timer.c)
endif()

listenai_include_directories(.)

endif() # CONFIG_LISA_HWTIMER_DEVICE
