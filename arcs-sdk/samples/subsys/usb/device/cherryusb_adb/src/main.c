#include "log_print.h"
#include "soc/chip.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#include "esp_heap_caps_init.h"
#include "esp_heap_caps.h"
#include "heap_memory_layout.h"
#include "heap_private.h"
#include "lisa_log.h"

extern int cherryusb_adb_start(void);

static void heap_travel_cb1(void *start, void *end, multi_heap_info_t *info)
{
    LOGI("%p %p %12d %12d %12d %12d %12d %12d %12d\r\n", start, end, info->allocated_blocks, info->free_blocks,
           info->total_blocks, info->largest_free_block, info->total_allocated_bytes, info->total_free_bytes,
           info->minimum_free_bytes);
}

static void usb_task(void *arg)
{
    (void)arg;
    cherryusb_adb_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(int argc, char **argv)
{
    LOGI("CherryUSB ADB example starting...\n");
    extern int test_lsfs_init(void);

    test_lsfs_init();

    xTaskCreate(usb_task, "usb_task", 1024 * 2, NULL, 5, NULL);
    while (1) {
        LOGI("CherryUSB ADB example running...\n");
        void heap_caps_travel(void (*callback)(void *, void *, multi_heap_info_t *));
        LOGI("%10s %10s %12s %12s %12s %12s %12s %12s %12s\r\n", "[Start]", "[End]", "[Alloc/BK]", "[Free/BK]",
             "[Total/BK]", "[MaxFree/BK]", "[Alloc/B]", "[Free/B]", "[MinFree/B]");
        heap_caps_travel(heap_travel_cb1);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
