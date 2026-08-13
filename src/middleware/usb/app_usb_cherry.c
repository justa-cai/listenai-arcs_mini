/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "app-usb"

#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "adb.h"
#include "adb_reboot.h"
#include "adb_shell.h"
#include "adb_sync.h"
#include "app_usb_cherry.h"
#include "lisa_kv.h"
#include "lisa_log.h"
#include "soc/chip.h"
#include "usbd_core.h"
#include "usbd_msc.h"

#define KV_USB_MSC "sys.usb.msc"
#define USB_REENUMERATION_DELAY_MS 300U

static bool s_usb_initialized;

bool app_usb_msc_enabled(void)
{
    bool enabled = false;

    lisa_kv_get_bool(KV_USB_MSC, &enabled);
    return enabled;
}

static void app_usb_port_init(void)
{
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
}

static void app_usb_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;

    switch (event) {
    case USBD_EVENT_RESET:
    case USBD_EVENT_DISCONNECTED:
#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
        app_usb_cherry_audio_on_disconnect();
#endif
        break;
    default:
        break;
    }
}

void app_usb_prepare_reboot(void)
{
    if (!s_usb_initialized) {
        return;
    }

    s_usb_initialized = false;
    if (usbd_deinitialize(APP_USB_BUS_ID) != 0) {
        LISA_LOGW(TAG, "CherryUSB deinitialization failed");
    }

    /* Give the host a stable detach interval before boot recovery reconnects. */
    vTaskDelay(pdMS_TO_TICKS(USB_REENUMERATION_DELAY_MS));
}

#if defined(CONFIG_USBDEV_MSC_POLLING)
static void app_usb_msc_poll_task(void *arg)
{
    (void)arg;

    while (1) {
        if (usbd_msc_polling(APP_USB_BUS_ID) == 0U) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
#endif

int user_usb_start(void)
{
    bool msc_mode = app_usb_msc_enabled();
    int ret;

    app_usb_port_init();
    app_usb_cherry_descriptors_register(msc_mode);

    if (!msc_mode) {
        adb_init();
#if CONFIG_ADB_REBOOT
        adb_reboot_init();
#endif
#if CONFIG_ADB_SHELL
        adb_shell_init();
#endif
#if CONFIG_ADB_SYNC
        adb_sync_init();
#endif
#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
        if (app_usb_audio_run() != 0) {
            LISA_LOGE(TAG, "USB audio initialization failed");
            return -1;
        }
#endif
    }

    ret = usbd_initialize(APP_USB_BUS_ID, USBC_BASE, app_usb_event_handler);
    if (ret != 0) {
        LISA_LOGE(TAG, "CherryUSB initialization failed: %d", ret);
        return ret;
    }
    s_usb_initialized = true;

#if defined(CONFIG_USBDEV_MSC_POLLING)
    if (msc_mode &&
        xTaskCreate(app_usb_msc_poll_task, "usb_msc", 1024, NULL,
                    configMAX_PRIORITIES - 2, NULL) != pdPASS) {
        LISA_LOGE(TAG, "failed to create USB MSC polling task");
        return -1;
    }
#endif

    printf("CherryUSB initialized\n");
    return 0;
}
