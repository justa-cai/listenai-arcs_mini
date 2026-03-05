#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "assert.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "http-test"
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
#include "lisa_http.h"

// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#define SERVER_URL     "http://192.168.1.100:8080"

#define TARGET_WIFI_SSID   "your_wifi_ssid"
#define TARGET_WIFI_PWD     "your_wifi_pwd"

// 编译时检查：确保用户已修改默认配置
#ifndef SERVER_URL
#error "请修改 SERVER_URL 为实际的测试服务器 URL（例如：http://192.168.1.100:8080）"
#endif

#ifndef TARGET_WIFI_SSID
#error "请修改 TARGET_WIFI_SSID 为实际的 WiFi SSID"
#endif

#ifndef TARGET_WIFI_PWD
#error "请修改 TARGET_WIFI_PWD 为实际的 WiFi 密码"
#endif

#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000

#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

#define HTTP_TEST_HEAD "Transfer-Encoding: chunked\r\n"

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr)
        return -1;

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
            LOGI("WiFi connected and IP obtained, starting HTTP tests...");
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

static void http_data_cb(lisa_http_data_t *data)
{
    if (!data || !data->buf) {
        LOGE("http data is null");
        return;
    } else if (data->len <= 0) {
        LOGE("http data len is 0");
        return;
    } else {
        LOGI("\nhttp response : %s\n", data->buf);
    }
}

static void *http_request_heads(void)
{
    return HTTP_TEST_HEAD;
}

int main(int argc, char **argv)
{
    int ret;
    lisa_http_t *http = NULL;

    LOGI("HTTP Test Starting...");

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
    LOGI("HTTP 功能测试");
    LOGI("========================================\n");

    // 测试 1: HTTP GET 请求
    LOGI("=== 测试 1: HTTP GET 请求 ===");
    lisa_http_request_t req;

    memset(&req, 0, sizeof(lisa_http_request_t));
    req.method = LISA_HTTP_GET;
    req.url = SERVER_URL;
    req.timeout = 10;
    req.on_data = http_data_cb;
    req.body = NULL;
    req.body_len = 0;
    req.headers = NULL;

    http = lisa_http_init(&req);
    if (!http) {
        LOGE("[http] init err\n");
        goto ERR;
    }

    if (lisa_http_perform(http) != 0) {
        LOGE("http get request info err..\n");
        goto ERR;
    }

    if (http) {
        lisa_http_cleanup(http);
        http = NULL;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 测试 2: HTTP POST 请求
    LOGI("\n=== 测试 2: HTTP POST 请求 ===");
    memset(&req, 0, sizeof(lisa_http_request_t));
    req.method = LISA_HTTP_POST;
    req.url = SERVER_URL "/foobar";
    req.timeout = 10;
    req.on_data = http_data_cb;
    req.body = "foobar";
    req.body_len = strlen(req.body);
    req.headers = NULL;

    http = lisa_http_init(&req);
    if (!http) {
        LOGE("[http] init err\n");
        goto ERR;
    }

    if (lisa_http_perform(http) != 0) {
        LOGE("http post request info err..\n");
        goto ERR;
    }

    if (http) {
        lisa_http_cleanup(http);
        http = NULL;
    }

    LOGI("\n========================================");
    LOGI("所有 HTTP 测试完成");
    LOGI("========================================\n");

ERR:
    if (http) {
        lisa_http_cleanup(http);
        http = NULL;
    }

    return 0;
}
