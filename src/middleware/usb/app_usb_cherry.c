/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "app-usb"

#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "adb.h"
#include "adb_reboot.h"
#include "adb_shell.h"
#include "adb_sync.h"
#include "app_usb_cherry.h"
#include "ClockManager.h"
#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
#include "app_usb_role_detect.h"
#endif
#include "lisa_kv.h"
#include "lisa_log.h"
#include "soc/chip.h"
#include "usbd_core.h"
#include "usbd_msc.h"
#if defined(CONFIG_CHERRYUSB_HOST) && CONFIG_CHERRYUSB_HOST
#include "usbh_core.h"
#endif

#define KV_USB_MSC "sys.usb.msc"
#define USB_REENUMERATION_DELAY_MS 300U
#define USB_DEVICE_DISCONNECT_DELAY_MS 20U
#define USB_ROLE_POLL_INTERVAL_MS  100U
#define USB_ROLE_DEBOUNCE_SAMPLES  3U
#define USB_ROLE_TASK_STACK_SIZE   1024U

#define APP_USB_MUSB_POWER_OFFSET  0x01U
#define APP_USB_MUSB_DEVCTL_OFFSET 0x60U
#define APP_USB_POWER_SOFTCONN     0x40U
#define APP_USB_DEVCTL_SESSION     0x01U

static bool s_usb_device_services_initialized;
static bool s_usb_msc_mode;
static volatile bool s_usb_suspended;
static volatile app_usb_role_t s_active_role = APP_USB_ROLE_UNKNOWN;
static volatile uint8_t s_usb_host_enumerated_devices;

static void app_usb_clear_musb_register_bits(uint32_t offset, uint8_t bits)
{
    *(volatile uint8_t *)(USBC_BASE + offset) &= (uint8_t)~bits;
}

static void app_usb_reset_usbc_for_device(void)
{
    IP_CMN_SYS->REG_SW_RESET_CP2.bit.USBC_RESET = 1U;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
}

#if defined(CONFIG_USBDEV_MSC_POLLING)
static TaskHandle_t s_usb_msc_poll_task;
#endif

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
static SemaphoreHandle_t s_usb_role_mutex;
static TaskHandle_t s_usb_role_task;
static volatile bool s_usb_stopping;
#endif

bool app_usb_msc_enabled(void)
{
    bool enabled = false;

    lisa_kv_get_bool(KV_USB_MSC, &enabled);
    return enabled;
}

app_usb_role_t app_usb_active_role(void)
{
    return s_active_role;
}

bool app_usb_host_device_enumerated(void)
{
    return s_active_role == APP_USB_ROLE_HOST && s_usb_host_enumerated_devices > 0U;
}

static const char *app_usb_cherry_role_name(app_usb_role_t role)
{
    switch (role) {
    case APP_USB_ROLE_DEVICE:
        return "device";
    case APP_USB_ROLE_HOST:
        return "host";
    default:
        return "unknown";
    }
}

static void app_usb_device_port_init(void)
{
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x01;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    /* Keep the tested disconnect window before CherryUSB sets SOFTCONN. */
    app_usb_clear_musb_register_bits(APP_USB_MUSB_POWER_OFFSET,
                                     APP_USB_POWER_SOFTCONN);
    app_usb_clear_musb_register_bits(APP_USB_MUSB_DEVCTL_OFFSET,
                                     APP_USB_DEVCTL_SESSION);
    vTaskDelay(pdMS_TO_TICKS(USB_DEVICE_DISCONNECT_DELAY_MS));
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

static int app_usb_device_services_init(void)
{
    if (s_usb_device_services_initialized) {
        return 0;
    }

    if (!s_usb_msc_mode) {
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

    s_usb_device_services_initialized = true;
    return 0;
}

#if defined(CONFIG_USBDEV_MSC_POLLING)
static void app_usb_msc_poll_task(void *arg)
{
    (void)arg;

    while (1) {
        if (s_active_role != APP_USB_ROLE_DEVICE) {
            vTaskDelay(pdMS_TO_TICKS(USB_ROLE_POLL_INTERVAL_MS));
            continue;
        }

        if (usbd_msc_polling(APP_USB_BUS_ID) == 0U) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
#endif

static int app_usb_device_start(void)
{
    int ret;

    app_usb_device_port_init();
    /* Reset controller callbacks and rebuild the interface table per attach. */
    app_usb_cherry_descriptors_register(s_usb_msc_mode);

    ret = app_usb_device_services_init();
    if (ret != 0) {
        return ret;
    }

    ret = usbd_initialize(APP_USB_BUS_ID, USBC_BASE, app_usb_event_handler);
    if (ret != 0) {
        LISA_LOGE(TAG, "CherryUSB device initialization failed: %d", ret);
        (void)usbd_deinitialize(APP_USB_BUS_ID);
        if (!s_usb_msc_mode) {
            adb_reset();
        }
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100U));
    s_active_role = APP_USB_ROLE_DEVICE;

#if defined(CONFIG_USBDEV_MSC_POLLING)
    if (s_usb_msc_mode && s_usb_msc_poll_task == NULL &&
        xTaskCreate(app_usb_msc_poll_task, "usb_msc", 1024, NULL,
                    configMAX_PRIORITIES - 2, &s_usb_msc_poll_task) != pdPASS) {
        LISA_LOGE(TAG, "failed to create USB MSC polling task");
        s_active_role = APP_USB_ROLE_UNKNOWN;
        (void)usbd_deinitialize(APP_USB_BUS_ID);
        return -1;
    }
#endif

    LISA_LOGI(TAG, "CherryUSB active role=device");
    return 0;
}

static int app_usb_device_stop(void)
{
    int ret;

    s_active_role = APP_USB_ROLE_UNKNOWN;
#if defined(CONFIG_APP_USB_AUDIO_ENABLE) && CONFIG_APP_USB_AUDIO_ENABLE
    app_usb_cherry_audio_on_disconnect();
#endif
    ret = usbd_deinitialize(APP_USB_BUS_ID);
    if (!s_usb_msc_mode) {
        adb_reset();
    }
    if (ret != 0) {
        LISA_LOGE(TAG, "CherryUSB device deinitialization failed: %d", ret);
    }
    return ret;
}

#if defined(CONFIG_CHERRYUSB_HOST) && CONFIG_CHERRYUSB_HOST
static void app_usb_host_port_init(void)
{
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x0;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;
}

static const char *app_usb_host_event_name(uint8_t event)
{
    switch (event) {
    case USBH_EVENT_ERROR:
        return "error";
    case USBH_EVENT_DEVICE_CONNECTED:
        return "connected";
    case USBH_EVENT_DEVICE_DISCONNECTED:
        return "disconnected";
    case USBH_EVENT_DEVICE_CONFIGURED:
        return "configured";
    case USBH_EVENT_INTERFACE_UNSUPPORTED:
        return "unsupported-interface";
    case USBH_EVENT_INTERFACE_START:
        return "interface-start";
    case USBH_EVENT_INTERFACE_STOP:
        return "interface-stop";
    case USBH_EVENT_INIT:
        return "init";
    case USBH_EVENT_DEINIT:
        return "deinit";
    default:
        return NULL;
    }
}

static void app_usb_host_event_handler(uint8_t busid, uint8_t hub_index,
                                       uint8_t hub_port, uint8_t intf,
                                       uint8_t event)
{
    const char *name = app_usb_host_event_name(event);

    if (name == NULL) {
        return;
    }

    if (event == USBH_EVENT_DEVICE_CONFIGURED) {
        if (s_usb_host_enumerated_devices < UINT8_MAX) {
            s_usb_host_enumerated_devices++;
        }
    } else if (event == USBH_EVENT_DEVICE_DISCONNECTED) {
        if (s_usb_host_enumerated_devices > 0U) {
            s_usb_host_enumerated_devices--;
        }
    }

    LISA_LOGI(TAG, "CherryUSB host event=%s bus=%u hub=%u port=%u intf=%u",
              name, busid, hub_index, hub_port, intf);
}

static int app_usb_host_start(void)
{
    int ret;

    s_usb_host_enumerated_devices = 0U;
    app_usb_host_port_init();
    ret = usbh_initialize(APP_USB_BUS_ID, USBC_BASE,
                          app_usb_host_event_handler);
    if (ret != 0) {
        LISA_LOGE(TAG, "CherryUSB host initialization failed: %d", ret);
        return ret;
    }

    s_active_role = APP_USB_ROLE_HOST;
    LISA_LOGI(TAG, "CherryUSB active role=host");
    return 0;
}

static int app_usb_host_stop(void)
{
    int ret;

    s_usb_host_enumerated_devices = 0U;
    s_active_role = APP_USB_ROLE_UNKNOWN;
    ret = usbh_deinitialize(APP_USB_BUS_ID);
    if (ret != 0) {
        LISA_LOGE(TAG, "CherryUSB host deinitialization failed: %d", ret);
    }
    return ret;
}
#endif

static int app_usb_active_role_stop(void)
{
    switch (s_active_role) {
    case APP_USB_ROLE_DEVICE:
        return app_usb_device_stop();
#if defined(CONFIG_CHERRYUSB_HOST) && CONFIG_CHERRYUSB_HOST
    case APP_USB_ROLE_HOST:
        return app_usb_host_stop();
#endif
    default:
        return 0;
    }
}

static int app_usb_role_start(app_usb_role_t role)
{
    switch (role) {
    case APP_USB_ROLE_DEVICE:
        return app_usb_device_start();
#if defined(CONFIG_CHERRYUSB_HOST) && CONFIG_CHERRYUSB_HOST
    case APP_USB_ROLE_HOST:
        return app_usb_host_start();
#endif
    default:
        return -1;
    }
}

static int app_usb_role_switch(app_usb_role_t target)
{
    app_usb_role_t previous = s_active_role;
    int rollback_ret;
    int ret;

    if (target == previous) {
        return 0;
    }

    if (previous != APP_USB_ROLE_UNKNOWN) {
        ret = app_usb_active_role_stop();
        if (ret != 0) {
            return ret;
        }
        if (previous == APP_USB_ROLE_HOST && target == APP_USB_ROLE_DEVICE) {
            app_usb_reset_usbc_for_device();
        }
        vTaskDelay(pdMS_TO_TICKS(USB_REENUMERATION_DELAY_MS));
    }

    LISA_LOGI(TAG, "switching CherryUSB role: %s -> %s",
              app_usb_cherry_role_name(previous), app_usb_cherry_role_name(target));
    ret = app_usb_role_start(target);
    if (ret == 0 || previous == APP_USB_ROLE_UNKNOWN) {
        return ret;
    }

    LISA_LOGE(TAG, "failed to switch CherryUSB role to %s: %d; restoring %s",
              app_usb_cherry_role_name(target), ret,
              app_usb_cherry_role_name(previous));
    vTaskDelay(pdMS_TO_TICKS(USB_REENUMERATION_DELAY_MS));
    rollback_ret = app_usb_role_start(previous);
    if (rollback_ret != 0) {
        LISA_LOGE(TAG, "failed to restore CherryUSB role %s: %d",
                  app_usb_cherry_role_name(previous), rollback_ret);
    } else {
        LISA_LOGW(TAG, "restored CherryUSB role=%s after switch failure",
                  app_usb_cherry_role_name(previous));
    }
    return ret;
}

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
static void app_usb_role_task_entry(void *arg)
{
    app_usb_role_t candidate = APP_USB_ROLE_UNKNOWN;
    uint8_t stable_samples = 0U;
    bool read_failed = false;
    bool disconnected = false;

    (void)arg;
    while (!s_usb_stopping) {
        if (s_usb_suspended) {
            vTaskDelay(pdMS_TO_TICKS(USB_ROLE_POLL_INTERVAL_MS));
            continue;
        }

        app_usb_role_t detected;
        int connected;

        connected = app_usb_role_detect_read(&detected);

        if (connected < 0) {
            if (!read_failed) {
                LISA_LOGE(TAG, "USB role read failed: %d", connected);
                read_failed = true;
            }
            candidate = APP_USB_ROLE_UNKNOWN;
            stable_samples = 0U;
        } else if (connected == 0) {
            if (!disconnected) {
                LISA_LOGI(TAG, "USB role controller disconnected, keeping active role=%s",
                          app_usb_cherry_role_name(s_active_role));
                disconnected = true;
            }
            candidate = APP_USB_ROLE_UNKNOWN;
            stable_samples = 0U;
        } else {
            if (read_failed) {
                LISA_LOGI(TAG, "USB role detection recovered");
                read_failed = false;
            }
            disconnected = false;

            if (detected != candidate) {
                candidate = detected;
                stable_samples = 1U;
            } else if (stable_samples < USB_ROLE_DEBOUNCE_SAMPLES) {
                stable_samples++;
            }

            if (stable_samples >= USB_ROLE_DEBOUNCE_SAMPLES &&
                detected != s_active_role) {
                xSemaphoreTake(s_usb_role_mutex, portMAX_DELAY);
                if (!s_usb_stopping && detected != s_active_role &&
                    app_usb_role_switch(detected) != 0) {
                    stable_samples = 0U;
                }
                xSemaphoreGive(s_usb_role_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(USB_ROLE_POLL_INTERVAL_MS));
    }

    s_usb_role_task = NULL;
    vTaskDelete(NULL);
}
#endif

int app_usb_suspend(void)
{
    int ret;

    if (s_usb_suspended || s_active_role == APP_USB_ROLE_UNKNOWN) {
        return 0;
    }

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    if (s_usb_role_mutex != NULL) {
        xSemaphoreTake(s_usb_role_mutex, portMAX_DELAY);
    }
#endif

    s_usb_suspended = true;
    ret = app_usb_active_role_stop();
    if (ret == 0) {
        __HAL_CRM_USB_CLK_DISABLE();
        LISA_LOGI(TAG, "CherryUSB suspended and clocks disabled");
    } else {
        s_usb_suspended = false;
        LISA_LOGW(TAG, "CherryUSB standby deinitialization failed: %d", ret);
    }

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    if (s_usb_role_mutex != NULL) {
        xSemaphoreGive(s_usb_role_mutex);
    }
#endif
    return ret;
}

int app_usb_resume(void)
{
    app_usb_role_t target = APP_USB_ROLE_DEVICE;
    int ret;

    if (!s_usb_suspended) {
        return 0;
    }

    s_usb_msc_mode = app_usb_msc_enabled();
#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    (void)app_usb_role_detect_read(&target);
    if (s_usb_role_mutex != NULL) {
        xSemaphoreTake(s_usb_role_mutex, portMAX_DELAY);
    }
#endif

    s_usb_suspended = false;
    ret = app_usb_role_switch(target);
    if (ret != 0) {
        s_usb_suspended = true;
        __HAL_CRM_USB_CLK_DISABLE();
        LISA_LOGE(TAG, "CherryUSB standby resume failed: %d", ret);
    } else {
        LISA_LOGI(TAG, "CherryUSB resumed and re-enumerating");
    }

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    if (s_usb_role_mutex != NULL) {
        xSemaphoreGive(s_usb_role_mutex);
    }
#endif
    return ret;
}

void app_usb_prepare_reboot(void)
{
    bool was_active;

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    s_usb_stopping = true;
    if (s_usb_role_mutex != NULL) {
        xSemaphoreTake(s_usb_role_mutex, portMAX_DELAY);
    }
#endif

    was_active = s_active_role != APP_USB_ROLE_UNKNOWN;
    if (app_usb_active_role_stop() != 0) {
        LISA_LOGW(TAG, "CherryUSB shutdown failed");
    }

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    if (s_usb_role_mutex != NULL) {
        xSemaphoreGive(s_usb_role_mutex);
    }
#endif

    if (was_active) {
        /* Give boot recovery a stable detach interval before reconnecting. */
        vTaskDelay(pdMS_TO_TICKS(USB_REENUMERATION_DELAY_MS));
    }
}

int user_usb_start(void)
{
    app_usb_role_t initial_role = APP_USB_ROLE_DEVICE;
    int ret;

    s_usb_msc_mode = app_usb_msc_enabled();
#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    s_usb_stopping = false;
#endif

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    int connected = app_usb_role_detect_read(&initial_role);
    if (connected < 0) {
        initial_role = APP_USB_ROLE_DEVICE;
        LISA_LOGW(TAG, "USB role unavailable at startup (%d), fallback=device", connected);
    } else if (connected == 0) {
        initial_role = APP_USB_ROLE_DEVICE;
        LISA_LOGI(TAG, "startup USB disconnected, default role=device");
    } else {
        LISA_LOGI(TAG, "startup USB role=%s", app_usb_role_name(initial_role));
    }

    s_usb_role_mutex = xSemaphoreCreateMutex();
    if (s_usb_role_mutex == NULL) {
        LISA_LOGE(TAG, "failed to create USB role mutex");
        return -1;
    }
#endif

    ret = app_usb_role_switch(initial_role);

#if defined(CONFIG_APP_USB_ROLE_DETECT) && CONFIG_APP_USB_ROLE_DETECT
    if (xTaskCreate(app_usb_role_task_entry, "usb_role", USB_ROLE_TASK_STACK_SIZE,
                    NULL, tskIDLE_PRIORITY + 1, &s_usb_role_task) != pdPASS) {
        LISA_LOGE(TAG, "failed to create USB role task");
        (void)app_usb_active_role_stop();
        vSemaphoreDelete(s_usb_role_mutex);
        s_usb_role_mutex = NULL;
        return -1;
    }
#endif

    return ret;
}
