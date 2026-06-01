#include <string.h>
#include "ls_event.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"
#include "lwip/dns.h"

#include "lisa_wifi.h"
#include "ls_misc.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "sys_wifi.h"
#include "wifi_manager/wifi_manager.h"

#include "voice_msg.h"

#if CONFIG_SAL_USING_POSIX
#include "netdev.h"
#endif

#define TAG "user_wifi"
#include "lisa_log.h"

typedef struct {
    bool stack_inited;
    bool ready;
    bool started;
    bool connected;
    bool autoconnect_requested;
    bool autoconnect_enabled;
    bool netdev_registered;
    bool dhcp_cb_registered;
    sys_wifi_connect_result_t connect_result;
} sys_wifi_status_t;

static sys_wifi_status_t s_wifi = {0};

static mac_manager_t *m_mac_manager = NULL;

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg);
static void wifi_mgr_scan_done_cb(wifi_mgr_scan_info_t *aps_info, int ap_num, void *arg);
static int sys_wifi_register_netdev(void);
static void sys_wifi_start_autoconnect(void);
static void sys_wifi_stop_autoconnect(void);
static int sys_wifi_init_sta_config(wifi_mgr_sta_config_t *sta_config,
                                    const char *ssid,
                                    const char *pwd,
                                    const char *bssid);
bool sys_wifi_has_ap(void);

static bool sys_wifi_is_password_error_code(int code)
{
    return code == WIFI_ERROR_STA_AUTH_FAIL ||
           code == WIFI_ERROR_WPA3_PWD_OR_AUTH_FAIL ||
           code == WIFI_ERROR_FOUND_SSID_BUT_KEY_MISMATCH ||
           code == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT;
}

static bool sys_wifi_is_password_error(const wifi_mgr_connection_info_t *connection_info)
{
    return sys_wifi_is_password_error_code(connection_info->error_code) ||
           sys_wifi_is_password_error_code(connection_info->status_code) ||
           sys_wifi_is_password_error_code(connection_info->reason);
}

static bool sys_wifi_is_no_ap_error_code(int code)
{
    return code == WIFI_ERROR_STA_CONNECT_NO_TARGET_AP;
}

static bool sys_wifi_is_no_ap_error(const wifi_mgr_connection_info_t *connection_info)
{
    return sys_wifi_is_no_ap_error_code(connection_info->error_code) ||
           sys_wifi_is_no_ap_error_code(connection_info->status_code) ||
           sys_wifi_is_no_ap_error_code(connection_info->reason);
}

static void sys_wifi_reset_connect_result(void)
{
    s_wifi.connect_result = SYS_WIFI_CONNECT_RESULT_SUCCESS;
}

static void sys_wifi_set_connect_result(sys_wifi_connect_result_t result)
{
    s_wifi.connect_result = result;
}

/* extern 声明 SDK nv_config.c 中的 MAC eFuse 接口 */
extern int8_t nv_efuse_read_mac(uint8_t *mac_addr);
#ifdef CONFIG_VENDOR_STORAGE_ENABLE
#include "vendor_ops.h"

#define ARCS_MAC_HEADER_0 0x26
#define ARCS_MAC_HEADER_1 0x48

static int vendor_storage_random(uint8_t *mac, size_t *mac_len)
{
    unsigned int curTickCount = (unsigned int)xTaskGetTickCount();

    LISA_LOGI(TAG, "vendor_storage_random curTickCount:%d\n", curTickCount);
    srand(curTickCount);

    mac[0] = 0x26;
    mac[1] = 0x48;
    for (size_t i = 2; i < *mac_len; i++) {
        mac[i] = rand() % 256;
    }

    return 0;
}

static int vendor_storage_get(uint8_t *mac_buf, size_t *mac_buf_len)
{
    if (!mac_buf || !mac_buf_len || *mac_buf_len < 6) {
        return -1;
    }

    if (vendor_storage_read(VENDOR_WIFI_MAC_ID, mac_buf, *mac_buf_len) == 0) {
        return 0;
    }

    efuse_init();
    uint64_t uuid = efuse_read_uuid();
    if (uuid == 0) {
        LISA_LOGE(TAG, "efuse read empty uuid");
        return -1;
    }
    mac_buf[0] = ARCS_MAC_HEADER_0;
    mac_buf[1] = ARCS_MAC_HEADER_1;
    mac_buf[2] = (uuid & 0xFF);
    mac_buf[3] = (uuid >> 32 >> 16) & 0xFF;
    mac_buf[4] = (uuid >> 32 >> 8) & 0xFF;
    mac_buf[5] = (uuid >> 32) & 0xFF;

    *mac_buf_len = 6;
    return 0;
}

static int vendor_storage_set(const uint8_t *mac, size_t mac_len)
{
    if (!mac || mac_len != 6) {
        return -1;
    }

    return vendor_storage_write(VENDOR_WIFI_MAC_ID, (void *)mac, mac_len);
}

static int vendor_storage_del(void)
{
    return -1;
}

static mac_manager_content_ops_t mac_manager_venor_ops = {
    .random = vendor_storage_random,
    .get    = vendor_storage_get,
    .set    = vendor_storage_set,
    .del    = vendor_storage_del,
};
#endif

#ifdef CONFIG_MAC_EFUSE_ENABLE
#include "Driver_EFUSE.h"

#define ARCS_MAC_HEADER_0 0x26
#define ARCS_MAC_HEADER_1 0x48

/**
 * @brief 从 eFuse 读取 MAC 地址，读取失败时使用 UUID 生成确定性 MAC
 * @param mac_buf 输出 MAC 地址缓冲区
 * @param mac_buf_len MAC 地址长度（应为 6）
 * @return 0=成功, -1=失败
 */
static int efuse_mac_get(uint8_t *mac_buf, size_t *mac_buf_len)
{
    if (!mac_buf || !mac_buf_len || *mac_buf_len < 6) {
        return -1;
    }

    /* 调用 SDK 底层接口读取 eFuse MAC */
    int8_t ret = nv_efuse_read_mac(mac_buf);
    if (ret == 0) {
        *mac_buf_len = 6;
        LISA_LOGI(TAG, "Read MAC from eFuse: %02X:%02X:%02X:%02X:%02X:%02X",
                  mac_buf[0], mac_buf[1], mac_buf[2],
                  mac_buf[3], mac_buf[4], mac_buf[5]);
        return 0;
    }

    /* eFuse MAC 读取失败，使用 UUID 生成确定性 MAC */
    LISA_LOGW(TAG, "eFuse MAC not found, generating MAC from UUID");
    efuse_init();
    uint64_t uuid = efuse_read_uuid();
    if (uuid == 0) {
        LISA_LOGE(TAG, "efuse read empty uuid");
        return -1;
    }
    mac_buf[0] = ARCS_MAC_HEADER_0;
    mac_buf[1] = ARCS_MAC_HEADER_1;
    mac_buf[2] = (uuid & 0xFF);
    mac_buf[3] = (uuid >> 32 >> 16) & 0xFF;
    mac_buf[4] = (uuid >> 32 >> 8) & 0xFF;
    mac_buf[5] = (uuid >> 32) & 0xFF;

    *mac_buf_len = 6;
    return 0;
}

/**
 * @brief eFuse MAC 不支持运行时写入（仅产测固件支持）
 */
static int efuse_mac_set(const uint8_t *mac, size_t mac_len)
{
    return -1;
}

/**
 * @brief eFuse MAC 不支持删除
 */
static int efuse_mac_del(void)
{
    return -1;
}

/**
 * @brief eFuse MAC 不支持随机生成（get 已通过 UUID 回退处理）
 */
static int efuse_mac_random(uint8_t *mac, size_t *mac_len)
{
    return -1;
}

static mac_manager_content_ops_t mac_manager_efuse_ops = {
    .random = efuse_mac_random,
    .get    = efuse_mac_get,
    .set    = efuse_mac_set,
    .del    = efuse_mac_del,
};
#endif

static void user_mac_manager_init(void)
{
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };


    m_mac_manager = mac_manager_init(
        mac_manager_ops_get()->mem_ops,
#ifdef CONFIG_MAC_EFUSE_ENABLE
        &mac_manager_efuse_ops,  /* 优先使用 eFuse MAC */
#elif defined(CONFIG_VENDOR_STORAGE_ENABLE)
        &mac_manager_venor_ops,  /* 备选方案：vendor_storage */
#else
        mac_manager_ops_get()->content_ops,  /* 默认方案 */
#endif
        &config);
  
    if (m_mac_manager == NULL) {
        LISA_LOGE(TAG,"[user_wifi]mac_manager_init failed\n");
        assert(0);
    }
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr, uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        s_wifi.connected = true;
        sys_wifi_set_connect_result(SYS_WIFI_CONNECT_RESULT_SUCCESS);
        voice_msg_pub(VOICE_MSG_WIFI_IP_GOT, NULL, 0);
        LISA_LOGI(TAG,"DHCP Success on VIF-%d: IP=%d.%d.%d.%d, Mask=%d.%d.%d.%d, GW=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff,
             netmask & 0xff, (netmask >> 8) & 0xff, (netmask >> 16) & 0xff, (netmask >> 24) & 0xff,
             gateway & 0xff, (gateway >> 8) & 0xff, (gateway >> 16) & 0xff, (gateway >> 24) & 0xff);
    } else {
        s_wifi.connected = false;
        sys_wifi_set_connect_result(SYS_WIFI_CONNECT_RESULT_FAIL_IP);
        LISA_LOGI(TAG,"DHCP Failed on VIF-%d", vif_idx);
    }
}

static int sys_wifi_register_netdev(void)
{
#if CONFIG_SAL_USING_POSIX
    if (s_wifi.netdev_registered) {
        return 0;
    }

    if (app_netdev_register("wifi0", 200) != 0) {
        LISA_LOGW(TAG, "Failed to register WiFi network device");
        return -1;
    }

    s_wifi.netdev_registered = true;
#endif

    return 0;
}

static void sys_wifi_unregister_netdev(void)
{
#if CONFIG_SAL_USING_POSIX
    if (!s_wifi.netdev_registered) {
        return;
    }

    netdev_unregister_by_name("wifi0");
    s_wifi.netdev_registered = false;
#endif
}

static void sys_wifi_start_autoconnect(void)
{
    if (!s_wifi.ready || s_wifi.autoconnect_enabled) {
        return;
    }

    wifi_mgr_autoconn_config_t cnn_cfg = {
        .interval_ms = 1000,
        .max_interval_ms = 5000,
    };

    wifi_mgr_auto_connect_start(&cnn_cfg);
    s_wifi.autoconnect_enabled = true;

    LISA_LOGI(TAG, "WiFi manager auto connect started with interval %d ms, max %d ms",
              cnn_cfg.interval_ms, cnn_cfg.max_interval_ms);
}

static void sys_wifi_stop_autoconnect(void)
{
    if (!s_wifi.ready || !s_wifi.autoconnect_enabled) {
        return;
    }

    wifi_mgr_auto_connect_stop();
    s_wifi.autoconnect_enabled = false;
    LISA_LOGI(TAG, "WiFi manager auto connect stopped");
}

static void wifi_mgr_init_done_cb(void)
{
    LISA_LOGI(TAG, "lisa_wifi_init_done");
    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();

    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);
    wifi_mgr_add_scan_done_cb(wifi_mgr_scan_done_cb, NULL);
    s_wifi.ready = true;

    if (s_wifi.started) {
        sys_wifi_register_netdev();
        if (s_wifi.autoconnect_requested) {
            sys_wifi_start_autoconnect();
        }
    }
}

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr)
        return -1;

    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LISA_LOGI(TAG,"custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", ret, mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void wifi_mgr_scan_done_cb(wifi_mgr_scan_info_t *aps_info, int ap_num, void *arg)
{
    bool has_candidate = false;

    (void)arg;

    if (!s_wifi.started ||
        s_wifi.connected) {
        return;
    }

    if (!(s_wifi.autoconnect_requested || s_wifi.autoconnect_enabled) || !sys_wifi_has_ap()) {
        return;
    }

    if (ap_num > 0 && aps_info != NULL) {
        for (int idx = 0; idx < ap_num; idx++) {
            wifi_mgr_sta_config_t ap = {0};
            int count = wifi_mgr_storage_search_ap(&ap, 1, SEARCH_BY_BSSID, aps_info[idx].bssid);

            if (count <= 0 || ap.ssid[0] == '\0') {
                count = wifi_mgr_storage_search_ap(&ap, 1, SEARCH_BY_SSID, aps_info[idx].ssid);
            }

            if (count > 0 && ap.ssid[0] != '\0') {
                if (ap.bssid[0] != '\0' && strcmp(ap.bssid, aps_info[idx].bssid) != 0) {
                    continue;
                }

                has_candidate = true;
                break;
            }
        }
    }

    if (has_candidate) {
        return;
    }

    LISA_LOGI(TAG, "WiFi scan done, no saved AP available");
    sys_wifi_set_connect_result(SYS_WIFI_CONNECT_RESULT_FAIL_NO_AP);
    voice_msg_pub(VOICE_MSG_WIFI_DISCONNECTED, NULL, 0);
}

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    net_if_t *net_if;

    LISA_LOGI(TAG,"mgr connection: %d", connection_info->status);

    switch (connection_info->status)
    {
        case WIFI_MGR_STA_CONNECTED:
            LISA_LOGI(TAG,"WiFi connected, starting network interface");
            net_if = net_if_get(WIFI_VIF_STA_IDX);
            net_if_up(net_if);
            if (!net_if->static_ip) {
                ls_dhcpc_start(WIFI_VIF_STA_IDX);
            } else {
                s_wifi.connected = true;
                sys_wifi_set_connect_result(SYS_WIFI_CONNECT_RESULT_SUCCESS);
                voice_msg_pub(VOICE_MSG_WIFI_IP_GOT, NULL, 0);
            }
            
            voice_msg_pub(VOICE_MSG_WIFI_CONNECTED, NULL, 0);
            /*save wifi sta info*/
            memset(connection_info->sta_info->bssid,0,sizeof(connection_info->sta_info->bssid));
            wifi_mgr_storage_save_ap(connection_info->sta_info);
            break;

        case WIFI_MGR_STA_DISCONNECTED:
        case WIFI_MGR_STA_CONNECT_FAILED:
        {
            LISA_LOGI(TAG,"WiFi disconnected, stopping network interface");
            ls_dhcpc_stop(WIFI_VIF_STA_IDX);
            net_if_down(net_if_get(WIFI_VIF_STA_IDX));
            s_wifi.connected = false;
            if (sys_wifi_is_no_ap_error(connection_info)) {
                sys_wifi_set_connect_result(SYS_WIFI_CONNECT_RESULT_FAIL_NO_AP);
            } else if (sys_wifi_is_password_error(connection_info)) {
                sys_wifi_set_connect_result(SYS_WIFI_CONNECT_RESULT_FAIL_PASSWORD);
            } else {
                sys_wifi_set_connect_result(SYS_WIFI_CONNECT_RESULT_FAIL_OTHER);
            }
            LISA_LOGI(TAG, "WiFi connect result status:%d error:%d assoc:%d reason:%d",
                      connection_info->status, connection_info->error_code,
                      connection_info->status_code, connection_info->reason);
            voice_msg_pub(VOICE_MSG_WIFI_DISCONNECTED, NULL, 0);
            break;
        }

        case WIFI_MGR_STA_CONNECTING:
            LISA_LOGI(TAG,"WiFi connecting...");
            break;

        default:
            break;
    }
}

static int sys_wifi_init_sta_config(wifi_mgr_sta_config_t *sta_config,
                                    const char *ssid,
                                    const char *pwd,
                                    const char *bssid)
{
    if (sta_config == NULL || ssid == NULL || ssid[0] == '\0') {
        return -1;
    }

    memset(sta_config, 0, sizeof(*sta_config));

    strncpy(sta_config->ssid, ssid, sizeof(sta_config->ssid) - 1);
    sta_config->ssid[sizeof(sta_config->ssid) - 1] = '\0';

    if (pwd != NULL) {
        strncpy(sta_config->pwd, pwd, sizeof(sta_config->pwd) - 1);
        sta_config->pwd[sizeof(sta_config->pwd) - 1] = '\0';
    }

    if (bssid != NULL) {
        strncpy(sta_config->bssid, bssid, sizeof(sta_config->bssid) - 1);
        sta_config->bssid[sizeof(sta_config->bssid) - 1] = '\0';
    }

    sta_config->encryption_mode = WIFI_MGR_WIFI_AUTH_AUTO;
    return 0;
}

int sys_wifi_init(void)
{
    if (s_wifi.stack_inited) {
        return 0;
    }

    user_mac_manager_init();

    if (!s_wifi.dhcp_cb_registered) {
        net_dhcp_register_status_callback(dhcp_status_callback, NULL);
        s_wifi.dhcp_cb_registered = true;
    }

    lisa_wifi_ops_t ops = {
        .init_done = wifi_mgr_init_done_cb,
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);
    s_wifi.stack_inited = true;

    return 0;
}

int sys_wifi_start(bool autoconnect)
{
    if (!s_wifi.stack_inited) {
        return -1;
    }

    s_wifi.started = true;
    s_wifi.autoconnect_requested = autoconnect;

    if (!s_wifi.ready) {
        return 0;
    }

    if (sys_wifi_register_netdev() != 0) {
        return -1;
    }

    if (autoconnect) {
        sys_wifi_start_autoconnect();
    } else {
        sys_wifi_stop_autoconnect();
    }

    return 0;
}

int sys_wifi_stop(void)
{
    s_wifi.started = false;
    s_wifi.autoconnect_requested = false;

    sys_wifi_unregister_netdev();

    if (!s_wifi.ready) {
        s_wifi.connected = false;
        return 0;
    }

    sys_wifi_stop_autoconnect();
    wifi_mgr_sta_disconnect(true);
    s_wifi.connected = false;

    return 0;
}

int sys_wifi_connect(const char *ssid, const char *pwd, const char *bssid)
{
    wifi_mgr_sta_config_t sta_config;

    if (!s_wifi.ready || !s_wifi.started) {
        return -1;
    }

    if (sys_wifi_init_sta_config(&sta_config, ssid, pwd, bssid) != 0) {
        return -1;
    }

    return wifi_mgr_sta_connect(&sta_config, false);
}

sys_wifi_connect_result_t sys_wifi_get_connect_result(void)
{
    return s_wifi.connect_result;
}

int sys_wifi_save_ap(const char *ssid, const char *pwd, const char *bssid)
{
    wifi_mgr_sta_config_t sta_config;
    int ret;

    if (!s_wifi.ready) {
        return -1;
    }

    if (sys_wifi_init_sta_config(&sta_config, ssid, pwd, bssid) != 0) {
        return -1;
    }

    ret = wifi_mgr_storage_save_ap(&sta_config);

    return ret;
}

int sys_wifi_clear_saved_aps(void)
{
    wifi_mgr_sta_config_t list[16] = {0};
    int batch_size = (int)(sizeof(list) / sizeof(list[0]));
    int count = 0;

    if (!s_wifi.ready) {
        s_wifi.connected = false;
        return 0;
    }

    (void)wifi_mgr_sta_disconnect(true);

    do {
        count = wifi_mgr_storage_search_ap(list, batch_size, SEARCH_ALL, NULL);
        if (count <= 0) {
            break;
        }

        for (int i = 0; i < count && i < batch_size; i++) {
            if (wifi_mgr_storage_delete_ap(&list[i]) != 0) {
                return -1;
            }
        }

        memset(list, 0, sizeof(list));
    } while (count >= batch_size);

    return 0;
}

bool sys_wifi_has_ap(void)
{
    wifi_mgr_sta_config_t ap = {0};
    int count = wifi_mgr_storage_search_ap(&ap, 1, SEARCH_ALL, NULL);
    return count > 0;
}

void sys_wifi_refresh_dnsserver(const char *const dns_srv)
{
    if (DNS_MAX_SERVERS >= 2) {
        ip_addr_t dns_ip_addr;
        if (ipaddr_aton(dns_srv, &dns_ip_addr)) {
            const ip_addr_t *current = dns_getserver(DNS_MAX_SERVERS - 1);
            if (current->addr != dns_ip_addr.addr) {
                LISA_LOGD(TAG, "%dth old dns server: %s", DNS_MAX_SERVERS, ipaddr_ntoa(current));
                dns_setserver(DNS_MAX_SERVERS - 1, &dns_ip_addr);
                current = dns_getserver(DNS_MAX_SERVERS - 1);
                LISA_LOGD(TAG, "%dth new dns server: %s", DNS_MAX_SERVERS, ipaddr_ntoa(current));
            }
        }
    }
}

bool sys_wifi_get_signal_quality(int *rssi)
{
    wifi_mgr_sta_config_t wifi_sta_info = {0};

    if (rssi == NULL || !s_wifi.ready || !s_wifi.connected) {
        return false;
    }

    if (wifi_mgr_sta_get_connected_info(&wifi_sta_info) != 0) {
        return false;
    }

    *rssi = wifi_sta_info.rssi;
    return true;
}

bool sys_wifi_is_ready(void)
{
    return s_wifi.ready;
}

bool sys_wifi_is_started(void)
{
    return s_wifi.started;
}

bool sys_wifi_is_connected(void)
{
    return s_wifi.connected;
}
