
#include "ls_event.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"
#include "lwip/dns.h"

#include "lisa_wifi.h"
#include "ls_misc.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"

#include "voice_msg.h"

#define TAG "user_wifi"
#include "lisa_log.h"

static mac_manager_t *m_mac_manager = NULL;

static void wifi_mgr_connection_cb(wifi_mgr_connection_info_t *connection_info, void *arg);

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
        voice_msg_pub(VOICE_MSG_WIFI_IP_GOT, NULL, 0);
        LISA_LOGI(TAG,"DHCP Success on VIF-%d: IP=%d.%d.%d.%d, Mask=%d.%d.%d.%d, GW=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff,
             netmask & 0xff, (netmask >> 8) & 0xff, (netmask >> 16) & 0xff, (netmask >> 24) & 0xff,
             gateway & 0xff, (gateway >> 8) & 0xff, (gateway >> 16) & 0xff, (gateway >> 24) & 0xff);
    } else {
        LISA_LOGI(TAG,"DHCP Failed on VIF-%d", vif_idx);
    }
}


static void cb_lisa_wifi_init_done(void)
{
    LISA_LOGI(TAG,"lisa_wifi_init_done");
    wifi_mgr_autoconn_config_t cnn_cfg = {
        .interval_ms = 3000,
    };
    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();

    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb, NULL);
    /*wifi连接必须在wifi初始化完成以后*/
    wifi_mgr_auto_connect_start(&cnn_cfg);

    LISA_LOGI(TAG,"WIFI manager auto connect started with interval %d ms", cnn_cfg.interval_ms);
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
            }
            
            voice_msg_pub(VOICE_MSG_WIFI_CONNECTED, NULL, 0);
            /*save wifi sta info*/
            memset(connection_info->sta_info->bssid,0,sizeof(connection_info->sta_info->bssid));
            wifi_mgr_storage_save_ap(connection_info->sta_info);
            break;

        case WIFI_MGR_STA_DISCONNECTED:
            LISA_LOGI(TAG,"WiFi disconnected, stopping network interface");
            ls_dhcpc_stop(WIFI_VIF_STA_IDX);
            net_if_down(net_if_get(WIFI_VIF_STA_IDX));
            voice_msg_pub(VOICE_MSG_WIFI_DISCONNECTED, NULL, 0);
            break;

        case WIFI_MGR_STA_CONNECTING:
            LISA_LOGI(TAG,"WiFi connecting...");
            break;

        default:
            break;
    }
}
void sys_wifi_init(void)
{

    user_mac_manager_init();

    // 注册 DHCP 状态回调
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .init_done = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

#if CONFIG_SAL_USING_POSIX
    // Register lwIP network device with priority 50
    extern int app_netdev_register(const char *name, uint8_t priority);
    if (app_netdev_register("wifi0", 200) != 0)
    {
        CLOGW("Failed to register lwIP network device");
    }
#endif

}

void ls_wifi_refresh_dnsserver(const char *const dns_srv)
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