# LISA SDMMC Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_SDMMC_DEVICE)

listenai_library_named(lisa_sdmmc_device)

listenai_library_sources(
    lisa_sdmmc_arcs.c           # ARCS 平台适配层
    port/arcs/sdmmc_init.c     # SDMMC 硬件初始化
)

listenai_include_directories(.)
listenai_include_directories(port/arcs)

endif() # CONFIG_LISA_SDMMC_DEVICE
