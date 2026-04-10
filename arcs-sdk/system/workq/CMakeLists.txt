# ARCS Work Queue
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_WORK_QUEUE)

listenai_include_directories(.)

listenai_library_sources(
    workqueue.c
)
endif() # CONFIG_WORK_QUEUE
