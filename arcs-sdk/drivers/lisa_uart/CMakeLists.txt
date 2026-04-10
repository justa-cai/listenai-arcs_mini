# LISA UART Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_UART_DEVICE)

listenai_library_named(lisa_uart_device)

listenai_library_sources_ifdef(CONFIG_SOC_ARCS  lisa_uart_arcs.c)

listenai_include_directories(.)

endif() # CONFIG_LISA_UART_DEVICE
