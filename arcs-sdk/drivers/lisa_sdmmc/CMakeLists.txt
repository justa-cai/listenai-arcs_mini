# LISA SDMMC Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_SDMMC_DEVICE)

listenai_library_named(lisa_sdmmc_device)

if(CONFIG_SOC_ARCS)
    listenai_library_sources(
        lisa_sdmmc_arcs.c
        port/arcs/sdmmc_init.c
    )
    listenai_include_directories(port/arcs)
endif()

if(CONFIG_SOC_VENUSA)
    listenai_library_sources(
        lisa_sdmmc_venusa.c
        port/venusa/sdmmc_init.c
    )
    listenai_include_directories(port/venusa)
endif()

listenai_include_directories(.)

endif() # CONFIG_LISA_SDMMC_DEVICE
