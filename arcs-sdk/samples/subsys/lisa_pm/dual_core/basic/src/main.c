/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief lisa_pm 双核 basic 示例（CP 核）
 *
 * AP 固件由 remote/ 子工程产出并负责冷启动/PM 唤醒时引导 CP；本文件运行在
 * CP 核，演示 WiFi 连接后通过 lisa_pm 进入 AUTO_LIGHT_SLEEP，并持续校验
 * CP 上下文在睡眠/唤醒后保持一致。
 */

#define LOG_TAG "lisa_pm_dc_basic"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "IOMuxManager.h"

#include "lisa_pm.h"
#include "lisa_wifi.h"
#include "ls_event.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "net_al.h"
#include "net_def.h"
#include "net_ip.h"
#include "wifi_api.h"

#define TARGET_WIFI_SSID            "YOUR_WIFI_SSID"
#define TARGET_WIFI_PWD             "YOUR_WIFI_PASSWORD"

#define WIFI_PS_LISTEN_INTERVAL     10
#define WIFI_WAIT_TIMEOUT_S         30
#define CP_CONTEXT_LOG_MS           5000
#define CP_CONTEXT_MAGIC            0x43504354U
#define CP_CONTEXT_SEED             0x5A5A0000U

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
    LOGI("CP ctx alive: counter=%lu wifi=%d ip=%d lp=%d wake=%s checksum=0x%08lx",
         (unsigned long)counter,
         (int)g_wifi_connected,
         (int)g_ip_ready,
         (int)g_low_power_enabled,
         wakeup_cause_str(lisa_pm_get_wakeup_cause()),
         (unsigned long)g_cp_checksum);
}

static int enable_auto_light_sleep(void)
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
    LOGI("AUTO_LIGHT_SLEEP enabled (WiFi LISTEN interval=%d)",
         WIFI_PS_LISTEN_INTERVAL);
    return 0;
}

static void disable_auto_light_sleep(void)
{
    if (!g_low_power_enabled) {
        return;
    }

    lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_ACTIVE);
    net_disable_keep_alive();
    g_low_power_enabled = false;
    LOGI("AUTO_LIGHT_SLEEP disabled");
}

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
        disable_auto_light_sleep();
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
        disable_auto_light_sleep();
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

    /* LISTEN 模式 listen_interval 必须在 connect 前下发，否则不生效。 */
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

    LOGI("=== lisa_pm dual_core basic CP ===");

    if (lisa_pm_init() != 0) {
        LOGE("lisa_pm_init failed");
        return -1;
    }

    cp_context_init();

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
        enable_auto_light_sleep();
    }

    while (1) {
        if (g_wifi_connected && g_ip_ready && !g_low_power_enabled) {
            enable_auto_light_sleep();
        }

        cp_context_step();
        vTaskDelay(pdMS_TO_TICKS(CP_CONTEXT_LOG_MS));
    }

    return 0;
}
