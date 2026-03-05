# LISA PWM Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_PWM)

listenai_library_named(lisa_pwm)

listenai_library_sources(
    lisa_pwm_arcs.c     # 平台适配层实现
)

listenai_include_directories(.)

endif() # CONFIG_LISA_PWM
