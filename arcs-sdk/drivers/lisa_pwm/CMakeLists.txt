# LISA PWM Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_PWM)

listenai_library_named(lisa_pwm)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_pwm_arcs.c)
if(CONFIG_SOC_ARCS AND CONFIG_LISA_CH32V003_PWM)
listenai_library_sources(lisa_pwm_ch32.c)
endif()

listenai_include_directories(.)

endif() # CONFIG_LISA_PWM
