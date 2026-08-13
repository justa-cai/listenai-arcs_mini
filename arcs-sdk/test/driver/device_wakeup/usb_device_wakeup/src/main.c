/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief lisa_pm 双核 USB UVC 设备睡眠/唤醒验证（CP 核）
 *
 * 验证 CherryUSB UVC 设备在睡前断开、唤醒后重新枚举：
 *   1) 开机连 WiFi，配置 GPIOB9 为唤醒源，默认 ACTIVE 策略，启动 shell。
 *   2) `uvc_test`  ：启动 CherryUSB UVC 静态 YUYV 流，等待 PC 枚举/打开摄像头。
 *   3) `low_power` ：先 usbd_deinitialize() 软件断开 USB，再进 AUTO_LIGHT_SLEEP。
 *   4) 按 GPIOB9 唤醒：after_wake 回调切回 ACTIVE，并重新 start UVC 触发 USB 枚举。
 *   5) PC 端重新识别 UVC 摄像头后再次打开视频流，应持续收到帧。
 *
 * AP 固件由上级 device_wakeup/remote 共用（与 audio/gpiob_device_wakeup 同一份）。
 */

#define LOG_TAG "lisa_pm_dc_uvc"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "IOMuxManager.h"
#include "shell.h"
#include "lisa_shell.h"

#include "lisa_pm.h"
#include "lisa_device.h"
#include "lisa_gpio.h"
#include "lisa_wifi.h"
#include "ls_event.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "net_al.h"
#include "net_def.h"
#include "net_ip.h"
#include "wifi_api.h"

#include "soc/chip.h"
#include "usbd_core.h"
#include "usbd_video.h"

#define TARGET_WIFI_SSID            "Xiaomi_listenai_2.4G"
#define TARGET_WIFI_PWD             "Xiaomi_listenai_2.4G"

#define WIFI_PS_LISTEN_INTERVAL     10
#define WIFI_WAIT_TIMEOUT_S         30
#define CP_CONTEXT_LOG_MS           5000
#define CP_CONTEXT_MAGIC            0x43504354U
#define CP_CONTEXT_SEED             0x5A5A0000U
#define WAKEUP_GPIO_DEV_NAME        "gpiob"
#define WAKEUP_GPIO_PIN             9

#define USB_BUS_ID                  0
#define UVC_TASK_STACK              4096
#define UVC_TASK_PRIO               4
#define UVC_FRAME_WIDTH             64U
#define UVC_FRAME_HEIGHT            48U
#define UVC_FRAME_FPS               30U
#define UVC_FRAME_INTERVAL_100NS    (10000000UL / UVC_FRAME_FPS)
#define UVC_FRAME_BYTES             (UVC_FRAME_WIDTH * UVC_FRAME_HEIGHT * 2U)
#define UVC_MIN_BIT_RATE            (UVC_FRAME_WIDTH * UVC_FRAME_HEIGHT * 16U * UVC_FRAME_FPS)
#define UVC_MAX_BIT_RATE            UVC_MIN_BIT_RATE
#define UVC_IN_EP                   0x81
#define UVC_INT_EP                  0x83

#ifdef CONFIG_USB_HS
#define UVC_MAX_PAYLOAD_SIZE        1024U
#else
#define UVC_MAX_PAYLOAD_SIZE        1020U
#endif
#define UVC_PACKET_SIZE             ((UVC_MAX_PAYLOAD_SIZE) | (0x00U << 11))
#define UVC_VS_HEADER_SIZE          (VIDEO_SIZEOF_VS_INPUT_HEADER_DESC(1, 1) + \
                                     VIDEO_SIZEOF_VS_FORMAT_UNCOMPRESSED_DESC + \
                                     VIDEO_SIZEOF_VS_FRAME_UNCOMPRESSED_DESC(1))
#define UVC_DESC_SIZE               (9UL + VIDEO_VC_NOEP_DESCRIPTOR_LEN + 9UL + \
                                     UVC_VS_HEADER_SIZE + 6UL + 9UL + 7UL)

#ifndef EXPECTED_CP_FLASH_BASE
#define EXPECTED_CP_FLASH_BASE      0x30100000U
#endif

#ifndef EXPECTED_CP_PSRAM_BASE
#define EXPECTED_CP_PSRAM_BASE      0x28400000U
#endif

#ifndef EXPECTED_CP_PSRAM_SIZE
#define EXPECTED_CP_PSRAM_SIZE      0x00400000U
#endif

_Static_assert(CONFIG_MEM_FLASH_BASE == EXPECTED_CP_FLASH_BASE,
               "CP flash base must match top-level CMake/remote memap");
_Static_assert(CONFIG_MEM_PSRAM_BASE == EXPECTED_CP_PSRAM_BASE,
               "CP PSRAM base must be 0x28400000");
_Static_assert(CONFIG_MEM_PSRAM_SIZE == EXPECTED_CP_PSRAM_SIZE,
               "CP PSRAM size must be 4M");

static mac_manager_t *g_mac_manager;
static volatile bool g_wifi_connected;
static volatile bool g_ip_ready;
static bool g_low_power_enabled;
static volatile uint32_t g_cp_magic;
static volatile uint32_t g_cp_counter;
static volatile uint32_t g_cp_checksum;
static volatile uint32_t g_cp_failed;

static TaskHandle_t g_uvc_task;
static volatile bool g_uvc_started;
static volatile bool g_uvc_streaming;
static volatile bool g_uvc_tx_busy;
static volatile bool g_uvc_configured;
static volatile uint32_t g_uvc_frame_count;
static bool g_uvc_resume_after_wake;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t g_uvc_packet[UVC_MAX_PAYLOAD_SIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t g_uvc_frame[UVC_FRAME_BYTES];

void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, WAKEUP_GPIO_PIN,
                              CSK_IOMUX_FUNC_DEFAULT);
}

static const char *wakeup_cause_str(lisa_pm_wakeup_cause_t cause)
{
    switch (cause) {
    case LISA_PM_WAKEUP_TIMER:   return "TIMER";
    case LISA_PM_WAKEUP_RTC:     return "RTC";
    case LISA_PM_WAKEUP_BT:      return "BT";
    case LISA_PM_WAKEUP_WIFI:    return "WIFI";
    case LISA_PM_WAKEUP_GPIO:    return "GPIO";
    default:                     return "UNKNOWN";
    }
}

static void gpiob_irq_callback(uint32_t pin, void *user_data)
{
    (void)user_data;
    LOGI("GPIOB_%lu wake IRQ", (unsigned long)pin);
}

static int setup_gpiob_wakeup_source(void)
{
    lisa_device_t *gpiob = lisa_device_get(WAKEUP_GPIO_DEV_NAME);
    int ret;
    int level;

    if (!lisa_device_ready(gpiob)) {
        LOGE("%s device not ready", WAKEUP_GPIO_DEV_NAME);
        return -1;
    }

    if (!lisa_device_wakeup_is_capable(gpiob)) {
        LOGE("%s device is not wakeup-capable", WAKEUP_GPIO_DEV_NAME);
        return -1;
    }

    ret = lisa_gpio_configure(gpiob, WAKEUP_GPIO_PIN,
                              LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
    if (ret != 0) {
        LOGE("configure GPIOB_%d input failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    ret = lisa_gpio_configure_irq(gpiob, WAKEUP_GPIO_PIN,
                                  LISA_GPIO_IRQ_EDGE_FALLING,
                                  gpiob_irq_callback, NULL);
    if (ret != 0) {
        LOGE("configure GPIOB_%d IRQ failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    ret = lisa_gpio_enable_irq(gpiob, WAKEUP_GPIO_PIN);
    if (ret != 0) {
        LOGE("enable GPIOB_%d IRQ failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    ret = lisa_gpio_configure_wakeup(gpiob, WAKEUP_GPIO_PIN,
                                     LISA_GPIO_WAKEUP_LEVEL_LOW);
    if (ret != 0) {
        LOGE("configure GPIOB_%d wakeup failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    ret = lisa_device_wakeup_enable(gpiob, true);
    if (ret != 0) {
        LOGE("enable GPIOB_%d wakeup failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    level = lisa_gpio_read_pin(gpiob, WAKEUP_GPIO_PIN);
    if (level == LISA_GPIO_LOW) {
        LOGW("GPIOB_%d is already low; release the key before sleep to avoid immediate wakeup",
             WAKEUP_GPIO_PIN);
    } else if (level < 0) {
        LOGW("read GPIOB_%d level failed: %d", WAKEUP_GPIO_PIN, level);
    }

    LOGI("GPIOB_%d configured as LEVEL_LOW wakeup source", WAKEUP_GPIO_PIN);
    return 0;
}

/* ======================================================================
 * CherryUSB UVC 静态 YUYV 设备
 * ====================================================================== */

static const uint8_t g_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xef, 0x02, 0x01, 0x12D1, 0x1081, 0x0001, 0x01)
};

static const uint8_t g_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(UVC_DESC_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, 100),
    VIDEO_VC_NOEP_DESCRIPTOR_INIT(0x00, UVC_INT_EP, 0x0100, VIDEO_VC_TERMINAL_LEN, 48000000, 0x02),
    VIDEO_VS_DESCRIPTOR_INIT(0x01, 0x00, 0x00),
    VIDEO_VS_INPUT_HEADER_DESCRIPTOR_INIT(0x01, UVC_VS_HEADER_SIZE, UVC_IN_EP, 0x00),
    VIDEO_VS_FORMAT_UNCOMPRESSED_DESCRIPTOR_INIT(0x01, 0x01, VIDEO_GUID_YUY2),
    VIDEO_VS_FRAME_UNCOMPRESSED_DESCRIPTOR_INIT(0x01, UVC_FRAME_WIDTH, UVC_FRAME_HEIGHT,
                                                UVC_MIN_BIT_RATE, UVC_MAX_BIT_RATE,
                                                UVC_FRAME_BYTES, DBVAL(UVC_FRAME_INTERVAL_100NS),
                                                0x01, DBVAL(UVC_FRAME_INTERVAL_100NS)),
    VIDEO_VS_COLOR_MATCHING_DESCRIPTOR_INIT(),
    VIDEO_VS_DESCRIPTOR_INIT(0x01, 0x01, 0x01),
    USB_ENDPOINT_DESCRIPTOR_INIT(UVC_IN_EP, 0x05, UVC_PACKET_SIZE, 0x01),
};

static const uint8_t g_device_quality_descriptor[] = {
    0x0a, USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
};

static const char *g_string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },
    "ListenAI",
    "ListenAI CherryUSB UVC Wakeup",
    "2026063001",
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return g_device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return g_config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return g_device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;
    if (index >= (sizeof(g_string_descriptors) / sizeof(g_string_descriptors[0]))) {
        return NULL;
    }
    return g_string_descriptors[index];
}

static const struct usb_descriptor g_uvc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};

static struct usbd_interface g_uvc_intf0;
static struct usbd_interface g_uvc_intf1;

static void fill_yuyv_test_frame(uint32_t seq)
{
    for (uint32_t y = 0; y < UVC_FRAME_HEIGHT; y++) {
        for (uint32_t x = 0; x < UVC_FRAME_WIDTH; x += 2) {
            uint32_t off = ((y * UVC_FRAME_WIDTH) + x) * 2U;
            uint8_t bar = (uint8_t)(((x + seq) / 8U) & 0x07U);
            uint8_t luma0 = (uint8_t)(32U + bar * 24U);
            uint8_t luma1 = (uint8_t)(224U - bar * 20U);

            g_uvc_frame[off + 0] = luma0;
            g_uvc_frame[off + 1] = (uint8_t)(96U + (bar * 8U));
            g_uvc_frame[off + 2] = luma1;
            g_uvc_frame[off + 3] = (uint8_t)(160U - (bar * 8U));
        }
    }
}

static void uvc_usb_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;

    switch (event) {
    case USBD_EVENT_RESET:
        LOGI("USB reset");
        break;
    case USBD_EVENT_CONNECTED:
        LOGI("USB connected");
        break;
    case USBD_EVENT_DISCONNECTED:
        LOGI("USB disconnected");
        g_uvc_configured = false;
        g_uvc_streaming = false;
        g_uvc_tx_busy = false;
        break;
    case USBD_EVENT_CONFIGURED:
        LOGI("USB configured");
        g_uvc_configured = true;
        g_uvc_tx_busy = false;
        break;
    case USBD_EVENT_DEINIT:
        LOGI("USB deinit");
        break;
    default:
        break;
    }
}

void usbd_video_open(uint8_t busid, uint8_t intf)
{
    (void)busid;
    LOGI("UVC stream open intf=%u", intf);
    g_uvc_streaming = true;
    g_uvc_tx_busy = false;
}

void usbd_video_close(uint8_t busid, uint8_t intf)
{
    (void)busid;
    LOGI("UVC stream close intf=%u", intf);
    g_uvc_streaming = false;
    g_uvc_tx_busy = false;
}

static void uvc_video_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if (nbytes == 0U) {
        return;
    }

    if (usbd_video_stream_split_transfer(busid, ep)) {
        g_uvc_tx_busy = false;
        g_uvc_frame_count++;
    }
}

static struct usbd_endpoint g_uvc_in_ep = {
    .ep_cb = uvc_video_in_callback,
    .ep_addr = UVC_IN_EP,
};

static int uvc_device_start(void)
{
    if (g_uvc_started) {
        LOGI("UVC already started");
        return 0;
    }

    memset(g_uvc_packet, 0, sizeof(g_uvc_packet));
    fill_yuyv_test_frame(0);

    usbd_desc_register(USB_BUS_ID, &g_uvc_descriptor);
    usbd_add_interface(USB_BUS_ID,
                       usbd_video_init_intf(USB_BUS_ID, &g_uvc_intf0,
                                            UVC_FRAME_INTERVAL_100NS,
                                            UVC_FRAME_BYTES,
                                            UVC_MAX_PAYLOAD_SIZE));
    usbd_add_interface(USB_BUS_ID,
                       usbd_video_init_intf(USB_BUS_ID, &g_uvc_intf1,
                                            UVC_FRAME_INTERVAL_100NS,
                                            UVC_FRAME_BYTES,
                                            UVC_MAX_PAYLOAD_SIZE));
    usbd_add_endpoint(USB_BUS_ID, &g_uvc_in_ep);

    int ret = usbd_initialize(USB_BUS_ID, USBC_BASE, uvc_usb_event_handler);
    if (ret != 0) {
        LOGE("usbd_initialize failed: %d", ret);
        return ret;
    }

    g_uvc_started = true;
    g_uvc_configured = false;
    g_uvc_streaming = false;
    g_uvc_tx_busy = false;
    LOGI("UVC TEST READY: connect/open PC camera app for %lux%lu YUYV @ %lu fps",
         (unsigned long)UVC_FRAME_WIDTH,
         (unsigned long)UVC_FRAME_HEIGHT,
         (unsigned long)UVC_FRAME_FPS);
    return 0;
}

static void uvc_device_stop(void)
{
    if (!g_uvc_started) {
        return;
    }

    g_uvc_streaming = false;
    g_uvc_tx_busy = false;
    g_uvc_configured = false;
    (void)usbd_deinitialize(USB_BUS_ID);
    g_uvc_started = false;
    LOGI("UVC software disconnect done before sleep");
}

static void uvc_stream_task(void *arg)
{
    (void)arg;

    while (1) {
        if (!g_uvc_started || !g_uvc_streaming || g_uvc_tx_busy) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        fill_yuyv_test_frame(g_uvc_frame_count);
        g_uvc_tx_busy = true;
        int ret = usbd_video_stream_start_write(USB_BUS_ID, UVC_IN_EP,
                                                g_uvc_packet, g_uvc_frame,
                                                sizeof(g_uvc_frame), false);
        if (ret != 0) {
            g_uvc_tx_busy = false;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

static int ensure_uvc_task(void)
{
    if (g_uvc_task != NULL) {
        return 0;
    }

    if (xTaskCreate(uvc_stream_task, "uvc_stream",
                    UVC_TASK_STACK / sizeof(StackType_t), NULL,
                    UVC_TASK_PRIO, &g_uvc_task) != pdPASS) {
        LOGE("create uvc stream task failed");
        g_uvc_task = NULL;
        return -1;
    }

    return 0;
}

/* ======================================================================
 * 低功耗进入 / 唤醒恢复
 * ====================================================================== */

static int enter_low_power(void)
{
    if (g_low_power_enabled) {
        return 0;
    }

    if (net_enable_keep_alive() != 0) {
        LOGE("net_enable_keep_alive failed");
        return -1;
    }

    if (lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP) != 0) {
        LOGE("set AUTO_LIGHT_SLEEP failed");
        net_disable_keep_alive();
        return -1;
    }

    g_low_power_enabled = true;
    LOGI("AUTO_LIGHT_SLEEP enabled (WiFi LISTEN interval=%d, wake GPIOB_%d)",
         WIFI_PS_LISTEN_INTERVAL, WAKEUP_GPIO_PIN);

    return 0;
}

static void exit_low_power(void)
{
    if (!g_low_power_enabled) {
        return;
    }

    lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_ACTIVE);
    net_disable_keep_alive();
    g_low_power_enabled = false;
    LOGI("AUTO_LIGHT_SLEEP disabled, back to ACTIVE");
}

static void app_after_wake(void *user_data, lisa_pm_wakeup_cause_t cause)
{
    (void)user_data;

    LOGI("after_wake cause=%s", wakeup_cause_str(cause));

    if (cause != LISA_PM_WAKEUP_GPIO) {
        return;
    }

    exit_low_power();

    if (g_uvc_resume_after_wake) {
        if (uvc_device_start() == 0) {
            LOGI("UVC TEST PASS: restarted after wake; host should re-enumerate");
        } else {
            LOGE("UVC TEST FAIL: restart after wake failed");
        }
    }
}

/* ======================================================================
 * shell 命令
 * ====================================================================== */

static int cmd_uvc_test(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (ensure_uvc_task() != 0) {
        return -1;
    }

    if (uvc_device_start() != 0) {
        return -1;
    }

    LOGI("Run `low_power` after PC has enumerated/opened the UVC camera");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 uvc_test, cmd_uvc_test, start CherryUSB UVC device test);

static int cmd_low_power(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!g_uvc_started) {
        LOGW("run `uvc_test` first to validate normal UVC enumeration");
        return -1;
    }

    g_uvc_resume_after_wake = true;
    uvc_device_stop();

    if (enter_low_power() != 0) {
        LOGE("enter low power failed");
        return -1;
    }

    LOGI("entering low power; USB is disconnected, press GPIOB_%d to wake and re-enumerate",
         WAKEUP_GPIO_PIN);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 low_power, cmd_low_power, disconnect UVC and enter AUTO_LIGHT_SLEEP);

/* ======================================================================
 * CP 上下文存活校验（沿用 gpio_wakeup 示例）
 * ====================================================================== */

static uint32_t cp_checksum(uint32_t counter)
{
    return CP_CONTEXT_SEED ^ (counter * 2246822519UL) ^ (counter << 5);
}

static void cp_context_init(void)
{
    g_cp_magic = CP_CONTEXT_MAGIC;
    g_cp_counter = 0;
    g_cp_checksum = cp_checksum(0);
    g_cp_failed = 0;
}

static void cp_context_step(void)
{
    uint32_t counter = g_cp_counter;
    uint32_t expected = cp_checksum(counter);

    if ((g_cp_magic != CP_CONTEXT_MAGIC) || (g_cp_checksum != expected)) {
        g_cp_failed = 1;
        LOGE("CP CTX CHECK FAILED: magic=0x%08lx counter=%lu checksum=0x%08lx expected=0x%08lx",
             (unsigned long)g_cp_magic,
             (unsigned long)counter,
             (unsigned long)g_cp_checksum,
             (unsigned long)expected);
        return;
    }

    counter++;
    g_cp_counter = counter;
    g_cp_checksum = cp_checksum(counter);
    LOGI("CP ctx alive: counter=%lu wifi=%d ip=%d lp=%d wake=%s uvc_started=%d cfg=%d streaming=%d frames=%lu checksum=0x%08lx",
         (unsigned long)counter,
         (int)g_wifi_connected,
         (int)g_ip_ready,
         (int)g_low_power_enabled,
         wakeup_cause_str(lisa_pm_get_wakeup_cause()),
         (int)g_uvc_started,
         (int)g_uvc_configured,
         (int)g_uvc_streaming,
         (unsigned long)g_uvc_frame_count,
         (unsigned long)g_cp_checksum);
}

/* ======================================================================
 * WiFi 接入（沿用 gpio_wakeup 示例，去掉自动进入低功耗）
 * ====================================================================== */

static int8_t custom_get_wifi_mac(uint8_t mac[6])
{
    int8_t ret;

    if ((mac == NULL) || (g_mac_manager == NULL)) {
        return -1;
    }

    ret = mac_manager_get(g_mac_manager, mac, 6);
    LOGI("custom MAC ret=%d %02X:%02X:%02X:%02X:%02X:%02X",
         ret, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ret;
}

static void dhcp_status_callback(int vif, bool success, uint32_t ip,
                                 uint32_t mask, uint32_t gw, void *arg)
{
    (void)mask;
    (void)gw;
    (void)arg;

    if (success) {
        g_ip_ready = true;
        LOGI("DHCP success on VIF-%d: %d.%d.%d.%d",
             vif,
             ip & 0xff,
             (ip >> 8) & 0xff,
             (ip >> 16) & 0xff,
             (ip >> 24) & 0xff);
    } else {
        g_ip_ready = false;
        exit_low_power();
        LOGI("DHCP failed on VIF-%d", vif);
    }
}

static int app_wifi_event_handler(void *arg, event_module_t mod, int id, void *data)
{
    net_if_t *nif;

    (void)arg;
    (void)mod;
    (void)data;

    switch (id) {
    case EVENT_WIFI_CONNECTED:
        g_wifi_connected = true;
        LOGI("WiFi connected");
        nif = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(nif);
        if (!nif->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        } else {
            g_ip_ready = true;
        }
        break;

    case EVENT_WIFI_DISCONNECT:
    case EVENT_WIFI_STA_CONNECT_FAIL:
        LOGI("WiFi disconnected or connect failed: event=%d", id);
        exit_low_power();
        g_wifi_connected = false;
        g_ip_ready = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;

    default:
        break;
    }

    return 0;
}

static void connect_target_ap(void)
{
    wifi_connect_cfg_t conn = {0};

    strncpy((char *)conn.ssid, TARGET_WIFI_SSID, sizeof(conn.ssid) - 1);
    strncpy((char *)conn.key, TARGET_WIFI_PWD, sizeof(conn.key) - 1);
    conn.dhcp_mode = DHCP_CLIENT;
    conn.sec = WIFI_SEC_AUTO;

    if (wifi_sta_connect(&conn) != 0) {
        LOGE("wifi_sta_connect failed");
        return;
    }

    LOGI("WiFi connecting to %s", TARGET_WIFI_SSID);
}

static void cb_lisa_wifi_init_done(void)
{
    lisa_pm_wifi_ps_config_t cfg = {
        .listen_interval = WIFI_PS_LISTEN_INTERVAL,
    };

    LOGI("lisa_wifi init done");
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, app_wifi_event_handler, NULL);
    wifi_sta_mode_enable();
    wifi_sta_auto_reconnect_enable();

    if (lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_LISTEN, &cfg) != 0) {
        LOGE("set WiFi LISTEN power save failed");
    }

    connect_target_ap();
}

static int wait_for_wifi_ready(void)
{
    int timeout = WIFI_WAIT_TIMEOUT_S;

    LOGI("Waiting for WiFi + IP...");
    while (timeout > 0) {
        if (g_wifi_connected && g_ip_ready) {
            LOGI("WiFi connected and IP obtained");
            return 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
        if ((timeout % 5) == 0) {
            LOGI("Waiting... wifi=%d ip=%d timeout=%d",
                 (int)g_wifi_connected, (int)g_ip_ready, timeout);
        }
    }

    LOGE("WiFi/IP wait timeout");
    return -1;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LOGI("=== lisa_pm dual_core usb_device_wakeup UVC CP ===");

    int ret = lisa_shell_init();
    if (ret != 0) {
        LOGE("Failed to initialize shell (error: %d)\n", ret);
        return ret;
    }

    if (lisa_pm_init() != 0) {
        LOGE("lisa_pm_init failed");
        return -1;
    }

    lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_ACTIVE);
    cp_context_init();

    if (setup_gpiob_wakeup_source() != 0) {
        return -1;
    }

    lisa_pm_sleep_callback_t pm_cb = {
        .before_sleep = NULL,
        .after_wake   = app_after_wake,
    };
    if (lisa_pm_sleep_callback_register(&pm_cb) != 0) {
        LOGE("register PM sleep callback failed");
        return -1;
    }

    mac_manager_config_t mcfg = {
        .random_mac_if_mac_invalid = false,
    };
    g_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops,
                                     mac_manager_ops_get()->content_ops,
                                     &mcfg);
    if (g_mac_manager == NULL) {
        LOGE("mac_manager_init failed");
        return -1;
    }

    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .init_done = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    if (lisa_wifi_init(&ops) != 0) {
        LOGE("lisa_wifi_init failed");
        return -1;
    }

    if (wait_for_wifi_ready() == 0) {
        LOGI("Ready. policy=ACTIVE. Run `uvc_test`, verify PC UVC, then `low_power`.");
    } else {
        LOGW("WiFi not ready; you can still run `uvc_test` over shell");
    }

    while (1) {
        cp_context_step();
        vTaskDelay(pdMS_TO_TICKS(CP_CONTEXT_LOG_MS));
    }

    return 0;
}
