# LISA GPIO Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_GPIO_DEVICE)

listenai_library_named(lisa_gpio_device)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_gpio_arcs.c)
if(CONFIG_SOC_ARCS AND CONFIG_LISA_CH32V003_GPIO)
listenai_library_sources(lisa_gpio_ch32.c)
endif()

listenai_include_directories(.)

endif() # CONFIG_LISA_GPIO_DEVICE
