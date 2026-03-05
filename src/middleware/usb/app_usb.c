/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "app-usb"
#define TAG "app-usb"

#include "lisa_log.h"

#if CFG_TUSB_OS == OPT_OS_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif
#include "log_print.h"
#include "tusb.h"
#include "arcs_ap.h"
#include "sysheap.h"
#include "assert.h"
#include "adb.h"
#include "adb_sync.h"
#include "adb_shell.h"
#include "lisa_kv.h"

#include <stdio.h>

#define USBD_STACK_SIZE 812

static char *buf = NULL;

#if BOARD_TUD_MAX_SPEED == OPT_MODE_HIGH_SPEED
#define BUF_SIZE 512
#else
#define BUF_SIZE 64
#endif

#if CFG_TUSB_OS == OPT_OS_FREERTOS
static SemaphoreHandle_t sem_cdc_received_data = NULL;
#endif

void cdc_task(void);

#define KV_USB_MSC "sys.usb.msc"
bool app_usb_msc_enabled(void)
{
    bool enabled = false;

    lisa_kv_get_bool(KV_USB_MSC, &enabled);

    return enabled;
}

static void user_usbd_cdc_init(void)
{
    // enable usb clock
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;   // Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; // 16bit mode

    buf = psram_malloc(BUF_SIZE);
    if (buf == NULL) {
        log_i("psram malloc failed\n");
        assert(0);
    }

    tud_disconnect(); // soft-disconnect from host
    tusb_init();
    tud_connect(); // soft-connect to host

    log_i("TinyUSB CDC class ready\n");
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
#if CONFIG_APP_USB_AUDIO_ENABLE
    extern int app_usb_audio_run(void);
    app_usb_audio_run();
#endif

#if CONFIG_APP_USB_CDC_ENABLE
    extern int app_usb_cdc_init(void);
    app_usb_cdc_init();
#endif

    while (1) {
        tud_task();
    }
}

int user_usb_start(void)
{
    printf("TinyUSB initialized");
    xTaskCreate(usb_task, "usb_task", 1024 * 1, NULL, configMAX_PRIORITIES -2, NULL);

    return 0;
}
