#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "assert.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "websocket-test"
#include "lisa_log.h"
#include "lisa_kv.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "user_fs.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"
#include "lisa_thread.h"
#include "lisa_websocket.h"

// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#define WS_TEST_SCHEME "ws"             // WebSocket 协议，通常为 "ws" 或 "wss"
#define WS_TEST_HOST   "your_server_ip" // WebSocket 服务器 IP，例如："192.168.1.100"
#define WS_TEST_PORT   "9001"             // WebSocket 服务器端口，例如："9001"
#define WS_TEST_PATH   "/"              // WebSocket 路径，例如："/"

#define TARGET_WIFI_SSID "your_wifi_ssid" // 修改为目标 WiFi 的 SSID
#define TARGET_WIFI_PWD  "your_wifi_pwd"  // 修改为目标 WiFi 的密码

// 编译时检查：确保用户已修改默认配置
#ifndef WS_TEST_SCHEME
#error "请修改 WS_TEST_SCHEME 为 WebSocket 协议（例如：ws 或 wss）"
#endif

#ifndef WS_TEST_HOST
#error "请修改 WS_TEST_HOST 为实际的 WebSocket 服务器 IP（例如：192.168.1.100）"
#endif

#ifndef WS_TEST_PORT
#error "请修改 WS_TEST_PORT 为实际的 WebSocket 服务器端口（例如：9001）"
#endif

#ifndef WS_TEST_PATH
#error "请修改 WS_TEST_PATH 为 WebSocket 路径（例如：/）"
#endif

#ifndef TARGET_WIFI_SSID
#error "请修改 TARGET_WIFI_SSID 为实际的 WiFi SSID"
#endif

#ifndef TARGET_WIFI_PWD
#error "请修改 TARGET_WIFI_PWD 为实际的 WiFi 密码"
#endif

#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000

#define WS_SEND_RETRY_DELAY      (10000)
#define WS_SEND_RETRY_SLEEP_TIME (100)

#define WS_TEST_CONTENT   "lisa_test"
#define WS_TEST_MAX_COUNT (10)

#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

static lisa_ws_event_e g_event = LISA_WS_ON_DISCONNECTED;
static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr) {
        return -1;
    }

    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", ret, mac_addr[0], mac_addr[1], mac_addr[2],
         mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr, uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP Success on VIF-%d: IP=%d.%d.%d.%d, Mask=%d.%d.%d.%d, GW=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff,
             netmask & 0xff, (netmask >> 8) & 0xff, (netmask >> 16) & 0xff, (netmask >> 24) & 0xff,
             gateway & 0xff, (gateway >> 8) & 0xff, (gateway >> 16) & 0xff, (gateway >> 24) & 0xff);
        g_get_ip_success = true;
    } else {
        LOGI("DHCP Failed on VIF-%d", vif_idx);
    }
}

static void wifi_mgr_connection_status_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    net_if_t *net_if;

    LOGI("WiFi connection status: %d", connection_info->status);

    switch (connection_info->status) {
    case WIFI_MGR_STA_CONNECTED:
        LOGI("WiFi connected to AP");
        g_wifi_connected = true;
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        }
        break;

    case WIFI_MGR_STA_DISCONNECTED:
        LOGI("WiFi disconnected from AP");
        g_wifi_connected = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;

    default:
        break;
    }
}

static void user_mac_manager_init(void)
{
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };

    m_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops, mac_manager_ops_get()->content_ops, &config);
    if (m_mac_manager == NULL) {
        LOGI("mac_manager_init failed\n");
        assert(0);
    }
}

static void user_wifi_manager_init(void)
{
    wifi_mgr_sta_config_t cfg = {
        .ssid = TARGET_WIFI_SSID,
        .pwd = TARGET_WIFI_PWD,
    };

    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = WIFI_MGR_AUTO_CONNECT_INTERVAL_MS,
    };

    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();

    // 注册 WiFi 连接状态回调
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_status_cb, NULL);

    /* 搜索并删除所有已保存的 AP */
    int count = wifi_mgr_storage_search_ap(list, WIFI_MGR_SEARCH_AP_BUFFER_SIZE, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        LOGI("Deleting saved AP: ssid=%s", list[i].ssid);
        wifi_mgr_storage_delete_ap(&list[i]);
    }

    // 保存 AP 配置并启动自动连接
    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);

    LOGI("WiFi auto-connect started");
}

static int wait_for_wifi_connection(void)
{
    int timeout = 30; // 30 秒超时

    LOGI("Waiting for WiFi connection and IP address...");

    // 等待 WiFi 连接和 IP 获取
    while (timeout > 0) {
        if (g_wifi_connected && g_get_ip_success) {
            LOGI("WiFi connected and IP obtained, starting WebSocket test...");
            vTaskDelay(pdMS_TO_TICKS(500)); // 短暂延迟确保网络稳定
            return 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;

        if (timeout % 5 == 0) {
            LOGI("Waiting... (WiFi:%d, IP:%d, timeout:%d)", g_wifi_connected, g_get_ip_success, timeout);
        }
    }

    LOGI("WiFi connection or IP acquisition timeout (WiFi:%d, IP:%d)", g_wifi_connected, g_get_ip_success);
    return -1;
}

static void get_ws_data_cb(lisa_ws_data_t *data)
{
    LOGI("ws recv data: %s", data->buf);
}

static void get_ws_event_cb(lisa_ws_event_t *event)
{
    g_event = event->what;
    if (event->what == LISA_WS_ON_CONNECTED) {
        LOGI("lisa ws connect");
    } else if (event->what == LISA_WS_ON_DISCONNECTED) {
        LOGI("lisa ws disconnect");
    } else {
        LOGI("event %d", event->what);
    }
}

int main(int argc, char **argv)
{
    int ret;

    LOGI("WebSocket Test Starting...");

    // 初始化 MAC 管理器
    user_mac_manager_init();

    // 注册 DHCP 状态回调
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    // 初始化 LISA WiFi
    lisa_wifi_ops_t ops = {
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    // 初始化文件系统和 KV 存储
    user_fs_init();
    lisa_kv_init();

    // 初始化 WiFi 管理器并启动自动连接
    user_wifi_manager_init();

    // 等待 WiFi 连接成功
    ret = wait_for_wifi_connection();
    if (ret != 0) {
        LOGI("Failed to connect to WiFi");
        return -1;
    }

    LOGI("\n========================================");
    LOGI("WebSocket 功能测试");
    LOGI("========================================\n");

    // 初始化 WebSocket 请求
    g_event = LISA_WS_ON_DISCONNECTED;
    lisa_ws_request_t lisa_ws_req;
    lisa_ws_req.scheme = WS_TEST_SCHEME;
    lisa_ws_req.host = WS_TEST_HOST;
    lisa_ws_req.port = WS_TEST_PORT;
    lisa_ws_req.path = WS_TEST_PATH;
    lisa_ws_req.timeout = WS_SEND_RETRY_DELAY;
    lisa_ws_req.on_data = get_ws_data_cb;
    lisa_ws_req.on_event = get_ws_event_cb;
    lisa_ws_req.user = NULL;
    lisa_ws_req.extra_header = NULL;

    LOGI("Connecting to WebSocket server: %s://%s:%s%s", WS_TEST_SCHEME, WS_TEST_HOST, WS_TEST_PORT, WS_TEST_PATH);

    lisa_ws_t *lisa_ws_ins = lisa_ws_init(&lisa_ws_req);
    if (lisa_ws_ins == NULL) {
        LOGE("lisa ws init failed");
        return -1;
    }

    if (lisa_ws_connect(lisa_ws_ins) != 0) {
        LOGE("lisa evs websocket connect error");
        ret = -1;
        goto ERR;
    }

    // 等待 WebSocket 连接成功
    int check_conn_count = 0;
    while (g_event != LISA_WS_ON_CONNECTED) {
        lisa_thread_mdelay(WS_SEND_RETRY_SLEEP_TIME);
        check_conn_count++;
        if (check_conn_count >= (WS_SEND_RETRY_DELAY / WS_SEND_RETRY_SLEEP_TIME)) {
            LOGE("lisa ws connect timeout");
            goto ERR;
        }
    }

    LOGI("\nWebSocket 连接成功，开始发送测试数据...\n");

    // 发送测试数据
    int ws_test_count = 0;
    while (1) {
        LOGI("发送消息 %d/%d: %s", ws_test_count + 1, WS_TEST_MAX_COUNT, WS_TEST_CONTENT);
        lisa_ws_send_text(lisa_ws_ins, WS_TEST_CONTENT);
        lisa_thread_mdelay(1000);
        ws_test_count++;
        if (ws_test_count >= WS_TEST_MAX_COUNT) {
            break;
        }
    }

    LOGI("\n========================================");
    LOGI("WebSocket 测试完成");
    LOGI("发送消息数: %d", ws_test_count);
    LOGI("========================================\n");

    ret = 0;
ERR:
    if (lisa_ws_ins != NULL) {
        lisa_ws_disconnect(lisa_ws_ins);
        lisa_ws_cleanup(lisa_ws_ins);
        lisa_ws_ins = NULL;
    }
    return ret;
}
