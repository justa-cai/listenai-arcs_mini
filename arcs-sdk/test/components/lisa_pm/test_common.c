/*
 * lisa_pm 测试工程 — 通用环境实现
 *
 * 提供 test_common_setup()：完成 vrtc/lisa_pm 初始化、WiFi 连接、
 * 进入 AUTO_LIGHT_SLEEP 的标准流程，所有 case 共享。
 *
 * 派生自 demos/lisa_wifi_pm/src/main.c 的非 mqtt 部分。
 */

#include "test_common.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "IOMuxManager.h"
#include "lisa_log.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "wifi_api.h"
#include "net_al.h"
#include "net_def.h"
#include "ls_event.h"
#include "lisa_pm.h"

#define TAG "test_lisa_pm"

#define TARGET_WIFI_SSID   "Xiaomi_listenai_2.4G"
#define TARGET_WIFI_PWD    "Xiaomi_listenai_2.4G"

#define WIFI_PS_LISTEN_INTERVAL  10
#define WIFI_RECONNECT_DELAY_MS  3000
#define WIFI_RECONNECT_TASK_STACK_SIZE 1024
#define WIFI_RECONNECT_TASK_PRIORITY   4

static mac_manager_t *m_mac_manager;

static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;
static bool g_low_power_enabled = false;

static TaskHandle_t g_wifi_reconnect_task = NULL;
static volatile bool g_wifi_reconnect_requested = false;

static ls_err_t user_wifi_direct_connect(void);
static void set_low_power_enabled(bool enable);

void lisa_uart1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, CSK_IOMUX_FUNC_ALTER3);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, CSK_IOMUX_FUNC_ALTER3);
}

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;
    if (!mac_addr) {
        return -1;
    }
    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", ret,
         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr,
                                  uint32_t netmask, uint32_t gateway, void *arg)
{
    (void)netmask;
    (void)gateway;
    (void)arg;

    if (success) {
        LOGI("DHCP Success: IP=%d.%d.%d.%d",
             ip_addr & 0xff, (ip_addr >> 8) & 0xff,
             (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
        g_get_ip_success = true;
        if (g_wifi_reconnect_requested && (g_wifi_reconnect_task != NULL)) {
            xTaskNotifyGive(g_wifi_reconnect_task);
        }
    } else {
        g_get_ip_success = false;
        LOGI("DHCP Failed on VIF-%d", vif_idx);
    }
}

static bool wifi_is_link_busy(void)
{
    wifi_link_status_t link_status = {0};

    if (wifi_get_link_status(&link_status) != LS_OK) {
        return false;
    }

    return (link_status.state == STA_IN_CONNECTING) ||
           (link_status.state == STA_CONNECTED);
}

static void schedule_wifi_reconnect(void)
{
    g_wifi_reconnect_requested = true;
    if (g_wifi_reconnect_task != NULL) {
        xTaskNotifyGive(g_wifi_reconnect_task);
    }
}

static void wifi_reconnect_task(void *arg)
{
    uint32_t wifi_attempt = 0;

    (void)arg;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (g_wifi_reconnect_requested) {
            if (!g_wifi_connected) {
                if (wifi_is_link_busy()) {
                    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                    continue;
                }

                wifi_attempt++;
                LOGI("WiFi reconnect attempt %lu", (unsigned long)wifi_attempt);

                if (user_wifi_direct_connect() != LS_OK) {
                    LOGW("Failed to trigger WiFi reconnect");
                }

                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));
                continue;
            }

            if (!g_get_ip_success) {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
                continue;
            }

            set_low_power_enabled(true);
            g_wifi_reconnect_requested = false;
            wifi_attempt = 0;
        }
    }
}

static int app_wifi_event_handler(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    (void)arg;
    (void)event_module;
    (void)event_data;
    net_if_t *net_if;

    switch (event_id) {
    case EVENT_WIFI_CONNECTED:
        LOGI("WiFi connected to AP");
        g_wifi_connected = true;
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        } else {
            g_get_ip_success = true;
            if (g_wifi_reconnect_requested && (g_wifi_reconnect_task != NULL)) {
                xTaskNotifyGive(g_wifi_reconnect_task);
            }
        }
        break;
    case EVENT_WIFI_DISCONNECT:
    case EVENT_WIFI_STA_CONNECT_FAIL:
        LOGI("WiFi disconnected or connect failed (event=%d)", event_id);
        g_wifi_connected = false;
        g_get_ip_success = false;
        set_low_power_enabled(false);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        schedule_wifi_reconnect();
        break;
    default:
        break;
    }
    return 0;
}

static void user_mac_manager_init(void)
{
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };
    m_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops,
                                      mac_manager_ops_get()->content_ops, &config);
    if (m_mac_manager == NULL) {
        LOGE("mac_manager_init failed");
    }
}

static ls_err_t user_wifi_direct_connect(void)
{
    wifi_connect_cfg_t cfg = {0};

    strncpy((char *)cfg.ssid, TARGET_WIFI_SSID, WIFI_SSID_LEN);
    strncpy((char *)cfg.key, TARGET_WIFI_PWD, WIFI_PASSWORD_LEN);
    cfg.dhcp_mode = DHCP_CLIENT;
    cfg.sec = WIFI_SEC_AUTO;

    ls_err_t ret = wifi_sta_connect(&cfg);
    if (ret != LS_OK) {
        LOGE("wifi_sta_connect failed: %d", ret);
    } else {
        LOGI("WiFi connecting to %s ...", TARGET_WIFI_SSID);
    }

    return ret;
}

static void cb_lisa_wifi_init_done(void)
{
    LOGI("lisa_wifi_init_done");

    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, app_wifi_event_handler, NULL);
    wifi_sta_mode_enable();
    int ret = wifi_sta_auto_reconnect_enable();
    if (ret != LS_OK) {
        LOGW("Failed to enable WiFi auto reconnect: %d", ret);
    }

    /* lisa_pm_wifi_set_ps_mode(LISTEN, ...) 必须在 connect 前调用，
     * 否则 listen_interval 不会下发到 chip。见 lisa_pm.h note。 */
    lisa_pm_wifi_ps_config_t cfg = {
        .listen_interval = WIFI_PS_LISTEN_INTERVAL,
    };
    if (lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_LISTEN, &cfg) != 0) {
        LOGE("Failed to pre-configure lisa_pm WiFi LISTEN mode");
        return;
    }

    user_wifi_direct_connect();
}

static int wait_for_wifi_connection(void)
{
    int timeout = 30;
    LOGI("Waiting for WiFi connection and IP address...");

    while (timeout > 0) {
        if (g_wifi_connected && g_get_ip_success) {
            LOGI("WiFi connected and IP obtained");
            vTaskDelay(pdMS_TO_TICKS(500));
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
        if (timeout % 5 == 0) {
            LOGI("Waiting... (WiFi:%d, IP:%d, timeout:%d)", g_wifi_connected, g_get_ip_success, timeout);
        }
    }
    LOGI("WiFi connection timeout");
    return -1;
}

static void enable_wifi_power_save(void)
{
    lisa_pm_wifi_ps_config_t cfg = {
        .listen_interval = WIFI_PS_LISTEN_INTERVAL,
    };
    int ret = lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_LISTEN, &cfg);
    if (ret != 0) {
        LOGE("Failed to enable lisa_pm WiFi LISTEN mode: %d", ret);
        return;
    }
    net_enable_keep_alive();
    LOGI("WiFi power save enabled (LISTEN mode, interval=%d)", WIFI_PS_LISTEN_INTERVAL);
}

static void enable_system_low_power(void)
{
    int ret = lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP);
    if (ret != 0) {
        LOGE("Failed to set lisa_pm AUTO_LIGHT_SLEEP policy: %d", ret);
        return;
    }
    LOGI("System power management enabled (Light Sleep mode with keep alive)");
}

static void disable_wifi_power_save(void)
{
    int ret = lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_OFF, NULL);
    if (ret != 0) {
        LOGW("Failed to disable lisa_pm WiFi power save: %d", ret);
        return;
    }
    net_disable_keep_alive();
    LOGI("WiFi power save disabled");
}

static void disable_system_low_power(void)
{
    int ret = lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_ACTIVE);
    if (ret != 0) {
        LOGW("Failed to set lisa_pm ACTIVE policy: %d", ret);
        return;
    }
    LOGI("System power management disabled");
}

static void set_low_power_enabled(bool enable)
{
    if (enable) {
        if (g_low_power_enabled) {
            return;
        }
        enable_wifi_power_save();
        enable_system_low_power();
        g_low_power_enabled = true;
        return;
    }

    if (!g_low_power_enabled) {
        return;
    }
    disable_wifi_power_save();
    disable_system_low_power();
    g_low_power_enabled = false;
}

int test_common_setup(void)
{
    BaseType_t task_ret;

    if (lisa_pm_init() != 0) {
        LOGE("lisa_pm_init failed");
        return -1;
    }

    user_mac_manager_init();
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    task_ret = xTaskCreate(wifi_reconnect_task, "wifi_reconnect",
                           WIFI_RECONNECT_TASK_STACK_SIZE, NULL,
                           WIFI_RECONNECT_TASK_PRIORITY, &g_wifi_reconnect_task);
    if (task_ret != pdPASS) {
        LOGE("Failed to create wifi_reconnect task");
        return -1;
    }

    lisa_wifi_ops_t ops = {
        .init_done  = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    if (wait_for_wifi_connection() != 0) {
        LOGE("Failed to connect to WiFi");
        return -1;
    }

    set_low_power_enabled(true);

    return 0;
}
