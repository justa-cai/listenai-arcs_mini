#define LOG_TAG "adb_bench"
#include <lisa_log.h>

#include "adb.h"
#include "adb_device.h"
#include "adb_shell.h"
#include "adb_sync.h"
#include "arcs_ap.h"
#include "tusb.h"

#include "FreeRTOS.h"
#include "task.h"

extern int adb_benchmark_fs_init(void);

static void usb_task(void *arg)
{
    (void)arg;

    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    adb_init();

#if CONFIG_ADB_SHELL
    adb_shell_init();
#endif

#if CONFIG_ADB_SYNC
    adb_sync_init();
#endif

    tud_disconnect();
    tusb_init();
    tud_connect();

    while (1) {
        tud_task();
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LOGI("adb benchmark sample starting");
    if (adb_benchmark_fs_init() != 0) {
        LOGE("adb benchmark: storage preparation incomplete");
    }

    xTaskCreate(usb_task, "usb_task", 1024, NULL, 5, NULL);

    while (1) {
        LOGI("adb benchmark sample running");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    return 0;
}
