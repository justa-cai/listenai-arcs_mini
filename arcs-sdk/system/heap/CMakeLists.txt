# ARCS System Heap
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_HEAP)

listenai_include_directories(.)

listenai_library_sources(
    sysheap.c
)
endif() # CONFIG_HEAP
