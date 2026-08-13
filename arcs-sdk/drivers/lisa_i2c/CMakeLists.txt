# LISA I2C Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_I2C)

listenai_library_named(lisa_i2c)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_i2c_arcs.c)
listenai_library_sources_ifdef(CONFIG_SOC_VENUSA  lisa_i2c_venusa.c)

listenai_include_directories(.)

endif() # CONFIG_LISA_I2C
