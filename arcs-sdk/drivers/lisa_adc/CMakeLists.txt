# LISA ADC Driver Example
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_ADC)

listenai_library_named(lisa_adc)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_adc_arcs.c)
if(CONFIG_SOC_ARCS AND CONFIG_LISA_CH32V003_ADC)
listenai_library_sources(lisa_adc_ch32.c)
endif()

listenai_include_directories(.)

endif() # CONFIG_LISA_ADC
