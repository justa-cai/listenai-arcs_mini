/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_port_mem.h"
#if CONFIG_LV_DEMO_SYS_HEAP
#include "sysheap.h"
#else
#include "xutils.h"
#endif


void *lvgl_port_malloc(size_t size)
{
    return exram_malloc(32, size);
}

void *lvgl_port_realloc(void *ptr, size_t size)
{
    return exram_realloc(ptr, size);
}

void lvgl_port_free(void *ptr)
{
    exram_free(ptr);
}