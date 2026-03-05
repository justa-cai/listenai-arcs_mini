# LISA Hardware Timer Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_HWTIMER_DEVICE)

listenai_library_named(lisa_hwtimer_device)

listenai_library_sources_ifdef(CONFIG_LISA_HWTIMER_ARCS_GPT_TIMER arcs_gpt_timer.c)
listenai_library_sources_ifdef(CONFIG_LISA_HWTIMER_ARCS_AON_TIMER arcs_aon_timer.c)
listenai_library_sources_ifdef(CONFIG_LISA_HWTIMER_ARCS_DUAL_TIMER arcs_dual_timer.c)

listenai_include_directories(.)

endif() # CONFIG_LISA_HWTIMER_DEVICE
