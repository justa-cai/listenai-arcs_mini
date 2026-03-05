# LISA QSPI LCD Driver
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_QSPILCD_DEVICE)

listenai_library_named(lisa_qspilcd_device)

listenai_library_sources(
    lisa_qspilcd_arcs.c
)

listenai_include_directories(.)

endif() # CONFIG_LISA_QSPILCD_DEVICE
