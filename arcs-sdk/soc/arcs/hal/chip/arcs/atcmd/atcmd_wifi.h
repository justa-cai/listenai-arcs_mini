#ifndef __ATCMD_WIFI_H__
#define __ATCMD_WIFI_H__
                  
#define BIT(pos)                (1UL << (pos))
/*
    bit 0: 是否显示 <ecn>
    bit 1: 是否显示 <ssid>
    bit 2: 是否显示 <rssi>
    bit 3: 是否显示 <mac>
    bit 4: 是否显示 <channel>
    bit 5: 是否显示 <bgn>
    bit 6: 是否显示 <wps>
*/
#define SCAN_PRINT_ENC_MASK         BIT(0)
#define SCAN_PRINT_SSID_MASK        BIT(1)
#define SCAN_PRINT_RSSI_MASK        BIT(2)
#define SCAN_PRINT_MAC_MASK         BIT(3)
#define SCAN_PRINT_CHANNEL_MASK     BIT(4)
#define SCAN_PRINT_BGN_MASK         BIT(5)
#define SCAN_PRINT_WPS_MASK         BIT(6)
/*
    bit 0: 是否显示 OPEN 认证方式的 AP
    bit 1: 是否显示 WEP 认证方式的 AP
    bit 2: 是否显示 WPA_PSK 认证方式的 AP
    bit 3: 是否显示 WPA2_PSK 认证方式的 AP
    bit 4: 是否显示 WPA_WPA2_PSK 认证方式的 AP
    bit 5: 是否显示 WPA2_ENTERPRISE 认证方式的 AP
    bit 6: 是否显示 WPA3_PSK 认证方式的 AP
    bit 7: 是否显示 WPA2_WPA3_PSK 认证方式的 AP
*/
#define SCAN_AUTH_MODE_OPEN_MASK                BIT(0)
#define SCAN_AUTH_MODE_WEP_MASK                 BIT(1)
#define SCAN_AUTH_MODE_WPA_PSK_MASK             BIT(2)
#define SCAN_AUTH_MODE_WPA2_PSK_MASK            BIT(3)
#define SCAN_AUTH_MODE_WPA_WPA2_PSK_MASK        BIT(4)
#define SCAN_AUTH_MODE_WPA2_ENTERPRISE_MASK     BIT(5)
#define SCAN_AUTH_MODE_WPA3_PSK_MASK            BIT(6)
#define SCAN_AUTH_MODE_WPA2_WPA3_PSK_MASK       BIT(7)

typedef struct wifi_scan_result_filter
{
    uint16_t print_mask;
    int8_t   rssi_filter;
    uint16_t authmode_mask;
} wifi_scan_result_filter_t;



#endif // !__ATCMD_WIFI_H__