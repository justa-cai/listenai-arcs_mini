#include "lv_port_mem.h"
#if CONFIG_LV_DEMO_SYS_HEAP
#include "sysheap.h"
#else
#include "xutils.h"
#endif

#include "esp_heap_caps.h"
#include "multi_heap.h"
#include "lisa_log.h"

#define MEM_BASE_CAPS (MALLOC_CAP_32BIT | MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_EXEC)

#ifndef CONFIG_LVGL_HEAP_SIZE
#define CONFIG_LVGL_HEAP_SIZE (1 * 1024 * 1024)  // Default 1MB if not defined
#endif

static uint8_t lvgl_port_mem[CONFIG_LVGL_HEAP_SIZE] __attribute__((section(".psram.bss")));

int lvgl_port_mem_init(void)
{
    const uint32_t psram_caps[] = {
        MEM_BASE_CAPS | MALLOC_CAP_PID2,
        MEM_BASE_CAPS | MALLOC_CAP_PID2,
        0,
    };
    heap_caps_add_region_with_caps(psram_caps, (intptr_t)lvgl_port_mem,
                                   (intptr_t)lvgl_port_mem + sizeof(lvgl_port_mem));
    return 0;
}

void *lvgl_port_malloc(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_PID2);

    return ptr;
}

void *lvgl_port_realloc(void *ptr, size_t size)
{
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_PID2);
    return new_ptr;
}

void lvgl_port_free(void *ptr)
{
    heap_caps_free(ptr);
}
