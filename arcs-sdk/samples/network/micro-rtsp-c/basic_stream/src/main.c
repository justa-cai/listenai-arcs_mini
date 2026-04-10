#include "lisa_thread.h"
#include <stdbool.h>
#include <assert.h>
#include <FreeRTOS.h>
#include <stdint.h>
#include <stdio.h>
#include <task.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define TAG "rtsp-test"
#include <lisa_log.h>
#include <lisa_kv.h>
#include <mac_manager.h>
#include <mac_manager_ops.h>
#include <wifi_manager/wifi_manager.h>
#include <lisa_wifi.h>
#include <ls_wifi_type.h>
#include <net_al.h>
#include <net_def.h>

#include "rtsp.h"
#include "session.h"
#include "streamer.h"
#include "sample.h"

#define TARGET_WIFI_SSID   "Your_SSID"
#define TARGET_WIFI_PWD     "Your_Password"
#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000
#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };
static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    if (!mac_addr) return -1;
    int8_t ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", ret,
         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr, uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP Success on VIF-%d: IP=%d.%d.%d.%d", vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
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
    mac_manager_config_t config = { .random_mac_if_mac_invalid = false };
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
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_status_cb, NULL);

    int count = wifi_mgr_storage_search_ap(list, WIFI_MGR_SEARCH_AP_BUFFER_SIZE, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        LOGI("Deleting saved AP: ssid=%s", list[i].ssid);
        wifi_mgr_storage_delete_ap(&list[i]);
    }

    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);
    LOGI("WiFi auto-connect started");
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

void handle_client(int client)
{
    rtsp_streamer_t streamer;
    rtsp_session_t session;

    rtsp_streamer_init(&streamer, 800, 600);
    rtsp_session_init(&session, client, &streamer);
    rtsp_streamer_set_uri(&streamer, "0.0.0.0:8554", "mjpeg", "1");

    streamer.is_debug = false;
    streamer.audio_enabled = false;
    session.is_debug = false;

    rtsp_streamer_add_session(&streamer, &session);

    uint8_t flag = 0;
    while (!session.is_stopped) {
        rtsp_streamer_start(&streamer, 1);
        if (session.is_streaming) {
            const uint8_t *capture_buff = capture_jpg;
            uint32_t capture_buff_len = capture_jpg_len;
            if (flag) {
                capture_buff = octo_jpg_800x600;
                capture_buff_len = OCTO_JPG_LEN;
            }
            flag = !flag;
            rtsp_streamer_stream_frame(&streamer, capture_buff, capture_buff_len, 100);
            lisa_thread_mdelay(500);
        }
    }

    close(client);
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    LOGI("RTSP Server Starting...");

    user_mac_manager_init();
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = { .custom_mac = custom_get_wifi_mac };
    lisa_wifi_init(&ops);

    user_fs_init();
    lisa_kv_init();

    user_wifi_manager_init();

    if (wait_for_wifi_connection() != 0) {
        LOGI("Failed to connect to WiFi");
        return -1;
    }

    LOGI("Starting RTSP server on port 8554");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(8554);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        LOGI("failed to reuse addr");
        return 0;
    }

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        LOGI("failed to bind port errno=%d", errno);
        return 0;
    }

    if (listen(sockfd, 5) != 0) return 0;

    for (;;) {
        int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        LOGI("client connected: %s", inet_ntoa(client_addr.sin_addr));
        // Only handle one client at a time, if we want to handle multiple clients, use threads
        handle_client(clientfd);
    }

    close(sockfd);
    return 0;
}
