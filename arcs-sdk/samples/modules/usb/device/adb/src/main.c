#include "log_print.h"
#include "tusb.h"
#include "arcs_ap.h"

#include "adb.h"
#include "tusb.h"
#include "adb_device.h"
#include "adb_shell.h"
#include "adb_sync.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#include "esp_heap_caps_init.h"
#include "esp_heap_caps.h"
#include "heap_memory_layout.h"
#include "heap_private.h"
#include "lisa_log.h"

static void heap_travel_cb1(void *start, void *end, multi_heap_info_t *info)
{
    LOGI("%p %p %12d %12d %12d %12d %12d %12d %12d\r\n", start, end, info->allocated_blocks, info->free_blocks,
           info->total_blocks, info->largest_free_block, info->total_allocated_bytes, info->total_free_bytes,
           info->minimum_free_bytes);
}

void usb_task(void *arg)
{
    // enable usb clock
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;   // Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; // 16bit mode

    adb_init();

#if CONFIG_ADB_SHELL
    adb_shell_init();
#endif

#if CONFIG_ADB_SYNC
    adb_sync_init();
#endif

    tud_disconnect(); // soft-disconnect from host
    // Initialize TinyUSB
    tusb_init();
    tud_connect(); // soft-connect to host

    while (1) {
        tud_task();
    }
}

int main(int argc, char **argv)
{
    LOGI("USB CDC Echo example starting...\n");
    extern int test_lsfs_init(void);

    test_lsfs_init();

    LOGI("TinyUSB initialized");
    xTaskCreate(usb_task, "usb_task", 1024 * 1, NULL, 5, NULL);
    while (1) {
        LOGI("USB ADB example running...\n");
        void heap_caps_travel(void (*callback)(void *, void *, multi_heap_info_t *));
        LOGI("%10s %10s %12s %12s %12s %12s %12s %12s %12s\r\n", "[Start]", "[End]", "[Alloc/BK]", "[Free/BK]",
             "[Total/BK]", "[MaxFree/BK]", "[Alloc/B]", "[Free/B]", "[MinFree/B]");
        heap_caps_travel(heap_travel_cb1);

        vTaskDelay(1000);
    }

    return 0;
}
