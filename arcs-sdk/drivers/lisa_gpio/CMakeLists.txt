# LISA GPIO Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_GPIO_DEVICE)

listenai_library_named(lisa_gpio_device)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_gpio_arcs.c)

listenai_include_directories(.)

endif() # CONFIG_LISA_GPIO_DEVICE
