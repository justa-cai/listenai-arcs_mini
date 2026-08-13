/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define TAG "wifi_pm_dc_cp"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "IOMuxManager.h"
#include "ic_lock.h"
#include "ipc.h"
#include "lisa_log.h"
#include "lisa_wifi.h"
#include "ls_err.h"
#include "ls_event.h"
#include "ls_wifi_type.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "net_al.h"
#include "net_def.h"
#include "net_ip.h"
#include "pm.h"
#include "vrtc.h"
#include "wifi_api.h"

#define TARGET_WIFI_SSID            "YOUR_WIFI_SSID"
#define TARGET_WIFI_PWD             "YOUR_WIFI_PASSWORD"

#define WIFI_PS_LISTEN_INTERVAL 10
#define WIFI_WAIT_TIMEOUT_S     30
#define CP_CONTEXT_LOG_MS       5000
#define CP_CONTEXT_MAGIC        0x43504354U
#define CP_CONTEXT_SEED         0x5A5A0000U

#ifndef EXPECTED_CP_FLASH_BASE
#define EXPECTED_CP_FLASH_BASE 0x30100000U
#endif

#ifndef EXPECTED_CP_PSRAM_BASE
#define EXPECTED_CP_PSRAM_BASE 0x28400000U
#endif

#ifndef EXPECTED_CP_PSRAM_SIZE
#define EXPECTED_CP_PSRAM_SIZE 0x00400000U
#endif

_Static_assert(CONFIG_MEM_FLASH_BASE == EXPECTED_CP_FLASH_BASE,
               "CP flash base must be 0x30100000");
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
    LOGI("CP ctx alive: counter=%lu wifi=%d ip=%d lp=%d checksum=0x%08lx",
         (unsigned long)counter,
         (int)g_wifi_connected,
         (int)g_ip_ready,
         (int)g_low_power_enabled,
         (unsigned long)g_cp_checksum);
}

static void set_low_power_enabled(bool enable)
{
    pm_config_t config;

    if (enable) {
        if (g_low_power_enabled) {
            return;
        }

        if (wifi_ps_mode_set(WIFI_PS_MODE_DTIM) != LS_OK) {
            LOGE("Failed to enable WiFi power save");
            return;
        }

        if (net_enable_keep_alive() != 0) {
            LOGE("Failed to enable network keep alive");
            wifi_ps_mode_set(WIFI_PS_MODE_OFF);
            return;
        }

        memset(&config, 0, sizeof(config));
        config.mode = PM_MODE_LIGHT_SLEEP;
        config.clock_level = PM_CLOCK_LEVEL0;
        config.auto_mode = 0;
        // config.dbg_level = PM_DBG_VRB;
        if (pm_set_config(&config) != 0) {
            LOGE("Failed to enable HAL PM light sleep");
            net_disable_keep_alive();
            wifi_ps_mode_set(WIFI_PS_MODE_OFF);
            return;
        }

        g_low_power_enabled = true;
        LOGI("WiFi power save enabled");
        LOGI("System HAL PM light sleep enabled");
        return;
    }

    if (!g_low_power_enabled) {
        return;
    }

    wifi_ps_mode_set(WIFI_PS_MODE_OFF);
    net_disable_keep_alive();

    memset(&config, 0, sizeof(config));
    config.mode = PM_MODE_ACTIVE;
    config.clock_level = PM_CLOCK_LEVEL0;
    config.auto_mode = 0;
    config.dbg_level = PM_DBG_INF;
    pm_set_config(&config);

    g_low_power_enabled = false;
    LOGI("System HAL PM returned to active mode");
}

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret;

    if ((mac_addr == NULL) || (g_mac_manager == NULL)) {
        return -1;
    }

    ret = mac_manager_get(g_mac_manager, mac_addr, 6);
    LOGI("custom MAC ret=%d %02X:%02X:%02X:%02X:%02X:%02X",
         ret,
         mac_addr[0], mac_addr[1], mac_addr[2],
         mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static int user_mac_manager_init(void)
{
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };

    g_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops,
                                     mac_manager_ops_get()->content_ops,
                                     &config);
    if (g_mac_manager == NULL) {
        LOGE("mac_manager_init failed");
        return -1;
    }

    return 0;
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr,
                                 uint32_t netmask, uint32_t gateway, void *arg)
{
    (void)netmask;
    (void)gateway;
    (void)arg;

    if (success) {
        g_ip_ready = true;
        LOGI("DHCP Success on VIF-%d: IP=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff,
             (ip_addr >> 8) & 0xff,
             (ip_addr >> 16) & 0xff,
             (ip_addr >> 24) & 0xff);
    } else {
        g_ip_ready = false;
        set_low_power_enabled(false);
        LOGI("DHCP Failed on VIF-%d", vif_idx);
    }
}

static ls_err_t app_wifi_event_handler(void *arg, event_module_t event_module,
                                       int event_id, void *event_data)
{
    net_if_t *net_if;

    (void)arg;
    (void)event_module;
    (void)event_data;

    switch (event_id) {
    case EVENT_WIFI_CONNECTED:
        g_wifi_connected = true;
        LOGI("WiFi connected");
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        } else {
            g_ip_ready = true;
        }
        break;

    case EVENT_WIFI_DISCONNECT:
    case EVENT_WIFI_STA_CONNECT_FAIL:
        LOGI("WiFi disconnected or connect failed: event=%d", event_id);
        set_low_power_enabled(false);
        g_wifi_connected = false;
        g_ip_ready = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;

    default:
        break;
    }

    return LS_OK;
}

static ls_err_t user_wifi_direct_connect(void)
{
    wifi_connect_cfg_t cfg = {0};
    ls_err_t ret;

    strncpy((char *)cfg.ssid, TARGET_WIFI_SSID, WIFI_SSID_LEN);
    strncpy((char *)cfg.key, TARGET_WIFI_PWD, WIFI_PASSWORD_LEN);
    cfg.dhcp_mode = DHCP_CLIENT;
    cfg.sec = WIFI_SEC_AUTO;

    ret = wifi_sta_connect(&cfg);
    if (ret == LS_OK) {
        LOGI("WiFi connecting to %s", TARGET_WIFI_SSID);
    } else {
        LOGE("wifi_sta_connect failed: %d", ret);
    }

    return ret;
}

static void cb_lisa_wifi_init_done(void)
{
    int ret;

    LOGI("lisa_wifi init done");

    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, app_wifi_event_handler, NULL);
    wifi_sta_mode_enable();

    ret = wifi_sta_auto_reconnect_enable();
    if (ret != LS_OK) {
        LOGE("wifi_sta_auto_reconnect_enable failed: %d", ret);
    }

    ret = wifi_sta_set_listen_itv(WIFI_PS_LISTEN_INTERVAL);
    if (ret != LS_OK) {
        LOGE("wifi_sta_set_listen_itv failed: %d", ret);
    }

    user_wifi_direct_connect();
}

static int wait_for_wifi_ready(void)
{
    int timeout = WIFI_WAIT_TIMEOUT_S;

    LOGI("Waiting for WiFi connection and IP address");
    while (timeout > 0) {
        if (g_wifi_connected && g_ip_ready) {
            LOGI("WiFi connected and IP obtained");
            return 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
        if ((timeout % 5) == 0) {
            LOGI("Waiting... wifi=%d ip=%d timeout=%d",
                 (int)g_wifi_connected,
                 (int)g_ip_ready,
                 timeout);
        }
    }

    LOGE("WiFi/IP wait timeout");
    return -1;
}

int main(int argc, char **argv)
{
    lisa_wifi_ops_t ops = {
        .custom_mac = custom_get_wifi_mac,
        .init_done = cb_lisa_wifi_init_done,
    };

    (void)argc;
    (void)argv;

    LOGI("=== wifi_pm_dual_core CP ===");

    if (pm_init() != 0) {
        LOGE("CP pm_init failed");
        return -1;
    }

    if (vrtc_init() != 0) {
        LOGE("CP vrtc_init failed");
        return -1;
    }

    pm_register_gpio_retention(CSK_IOMUX_PAD_A, 2);
    pm_register_gpio_retention(CSK_IOMUX_PAD_A, 3);
    LOGI("CP pm/vrtc init ok");

    cp_context_init();
    if (user_mac_manager_init() != 0) {
        return -1;
    }
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    if (lisa_wifi_init(&ops) != 0) {
        LOGE("lisa_wifi_init failed");
        return -1;
    }

    if (wait_for_wifi_ready() == 0) {
        set_low_power_enabled(true);
    }

    while (1) {
        if (g_wifi_connected && g_ip_ready && !g_low_power_enabled) {
            set_low_power_enabled(true);
        }

        cp_context_step();
        vTaskDelay(pdMS_TO_TICKS(CP_CONTEXT_LOG_MS));
    }

    return 0;
}
