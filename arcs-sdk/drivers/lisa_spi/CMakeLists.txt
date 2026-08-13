# LISA SPI Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_SPI_DEVICE)

listenai_library_named(lisa_spi)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_spi_arcs.c)
listenai_library_sources_ifdef(CONFIG_SOC_VENUSA  lisa_spi_venusa.c)

listenai_include_directories(.)

endif() # CONFIG_LISA_SPI
