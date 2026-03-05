/**
 ****************************************************************************************
 *
 * @file atcmd_wifi.c
 *
 * @brief
 *
 * Copyright (C) ListenAI  2023-2024
 *
 ****************************************************************************************
 */
#include <stdbool.h>
#include "atcmd.h"
#include "log_print.h"
#include "ls_event.h"
#include "wifi_api.h"
#include "nvs.h"
#include "nvds_tag_def.h"
#include "atcmd_wifi.h"
#include "atcmd_hash.h"
#include "lwip/ip_addr.h"
#include "net_ip.h"

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

extern char *utils_next_token(char **params);
extern int utils_parse_mac_addr(char *str, uint8_t *addr);
extern char *wifi_err_to_str(uint32_t code);

static wifi_scan_result_filter_t g_scan_filter = {
    .print_mask = 0xFFFF,
    .rssi_filter = -120,
    .authmode_mask = 0xFFFF
};

// '"' and '\' need escape character
int atcmd_get_proper_ssid_psk(char *token, uint8_t *str, uint8_t len)
{
    int i = 0;
    char *cur = token;
    char *next;

    if (!token)
    {
        return 0;
    }

    memset(str, 0, len);

    while (*cur && i < len)
    {
        if (*cur != '\\')
        {
            str[i++] = *cur++;
        }
        else
        {
            next = cur + 1;
            if (*next != '"' && *next != '\\')
            {
                // ignore this illegal '\\'
                CLOGI("unexpected \\ in ssid/psk");
                cur++;
            }
            else
            {
                str[i++] = *next;
                cur = next + 1;
            }
        }
    }
    //CLOGI("%s : %s %d\n", __func__, str, i);
    return i;
}

int atcmd_parse_mac_addr(char *str, uint8_t *addr)
{
    char *ptr = str;
    uint32_t i;

    if (!str || (strlen(str) < 17) || !addr)
        return -1;

    for (i = 0; i < 6; i++)
    {
        char *next;
        long int hex = strtol(ptr, &next, 16);
        if (((unsigned)hex > 255) || ((hex == 0) && (next == ptr)) ||
            ((i < 5) && (*next != ':')) ||
            ((i == 5) && (*next != '\0')))
            return -1;

        addr[i] = (uint8_t)hex;
        ptr = ++next;
    }

    return 0;
}

int cwjap_parse_connect_params(char *params, wifi_connect_cfg_t *config)
{
    char *cur;
    char *next = params;
    int8_t token_idx = -1;
    int dhcp_mode = -1;

    do
    {
        cur = atcmd_next_token(&next);
        token_idx++;

        switch (token_idx)
        {
            case 0:
            {
                int ssid_len;

                ssid_len = atcmd_get_proper_ssid_psk(cur, config->ssid, WIFI_SSID_LEN);
                if (!ssid_len || (ssid_len > WIFI_SSID_LEN))
                {
                    return -ATCMD_ERR_CFG_SSID;
                }
                break;
            }
            case 1:
            {
                int key_len;

                key_len = atcmd_get_proper_ssid_psk(cur, config->key, WIFI_PASSWORD_LEN);
                if (key_len > WIFI_PASSWORD_LEN)
                {
                    return -ATCMD_ERR_CFG_KEY;
                }
                break;
            }
            case 2:
            {
                if (cur && atcmd_parse_mac_addr(cur, config->bssid))
                {
                    return -ATCMD_ERR_CFG_BSSID;
                }
                break;
            }
            case 3:
            {
                if (cur)
                {
                    dhcp_mode = atoi(cur);
                    if (dhcp_mode == DHCP_CLIENT || dhcp_mode == STATIC_IPV4)
                    {
                        config->dhcp_mode = dhcp_mode;
                    }
                }
                break;
            }
            case 4:
            {
                int str_len = strlen(cur);

                if (dhcp_mode == STATIC_IPV4 && str_len <= 16 && str_len > 0)
                {
                    memcpy(config->ip4_ip, cur, str_len);
                }
                break;
            }
            case 5:
            {
                int str_len = strlen(cur);

                if (dhcp_mode == STATIC_IPV4 && str_len <= 16 && str_len > 0)
                {
                    memcpy(config->ip4_mask, cur, str_len);
                }
                break;
            }
            case 6:
            {
                int str_len = strlen(cur);

                if (dhcp_mode == STATIC_IPV4 && str_len <= 16 && str_len > 0)
                {
                    memcpy(config->ip4_gw, cur, str_len);
                }
                break;
            }
            default:
                //CLOGI("%s %d\n", __func__, __LINE__);
                break;
        }
    } while(next);

    return 0;
}

int atcmd_cwjap(int type, char *params)
{
    int res;
    ls_err_t ret;
    wifi_connect_cfg_t config = {0};
    wifi_link_status_t status = {0};

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        res = cwjap_parse_connect_params(params, &config);
        if (res)
        {
            atcmd_rspdata("CWJAP:%d", res);
            return ATCMD_ERROR;
        }

        ret = wifi_sta_connect(&config);
        if (ret != LS_OK)
        {
            atcmd_rspdata("CWJAP:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        if (config.dhcp_mode == STATIC_IPV4)
        {
            struct ip_addr_cfg ip_cfg = {0};

            ip_cfg.mode = IP_ADDR_STATIC_IPV4;
            ip_cfg.default_output = true;
            ipaddr_aton(config.ip4_ip, (ip_addr_t*)(&ip_cfg.ipv4.addr));
            ipaddr_aton(config.ip4_mask, (ip_addr_t*)(&ip_cfg.ipv4.mask));
            ipaddr_aton(config.ip4_gw, (ip_addr_t*)(&ip_cfg.ipv4.gw));
            ls_set_static_ip(WIFI_VIF_STA_IDX, &ip_cfg);
        }

        #if CFG_NVS
        nvds_put(NVDS_TAG_WIFI_STA_SSID, NVDS_LEN_WIFI_STA_SSID, config.ssid);
        if (strlen(config.key))
        {
            nvds_put(NVDS_TAG_WIFI_STA_PWD, NVDS_LEN_WIFI_STA_PWD, config.key);
        }
        else
        {
            nvds_del(NVDS_TAG_WIFI_STA_PWD);
        }
#endif
        ret = ls_event_wait(EVENT_WIFI, EVENT_WIFI_CONNECTED, 10000);
        if (ret != LS_OK) {
            ls_event_clear(EVENT_WIFI, EVENT_WIFI_CONNECTED);
            atcmd_rspdata("CWJAP:%d", -ATCMD_ERR_TIMEOUT);
            return ATCMD_ERROR;
        } else {
            atcmd_rspinfor("WIFI CONNECTED");
        }

        ret = ls_event_wait(EVENT_WIFI, EVENT_WIFI_GOT_IP, 20000);
        if (ret != LS_OK)
        {
            ls_event_clear(EVENT_WIFI, EVENT_WIFI_GOT_IP);

            atcmd_rspdata("CWJAP:%d", -ATCMD_ERR_TIMEOUT);
            return ATCMD_ERROR;
        }
        atcmd_rspinfor("WIFI GOT IP");

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        struct ip_addr_cfg cfg = {0};


        if (!status.is_connected)
        {
            atcmd_rspdata("CWJAP:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }

        ls_get_ip(WIFI_VIF_DEFAULT_IDX, &cfg);
        atcmd_rspdata("CWJAP:%s," MACSTR ",%d,%d,%d,%d.%d.%d.%d",
                      status.ssid, MAC2STR(status.bssid), status.channel, status.rssi, status.aid,
                      cfg.ipv4.addr & 0xff, (cfg.ipv4.addr >> 8) & 0xff, (cfg.ipv4.addr >> 16) & 0xff, (cfg.ipv4.addr >> 24) & 0xff);

        return ATCMD_OK;
    }
}

int atcmd_cwqap(int type, char *params)
{
    ls_err_t ret;

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        ret = wifi_sta_disconnect();
        //ls_dhcp_stop(WIFI_VIF_STA_IDX);

        if (ret != LS_OK)
        {
            atcmd_rspdata("CWQAP:%d", ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        ret = ls_event_wait(EVENT_WIFI, EVENT_WIFI_DISCONNECT, 3000);
        if (ret != LS_OK)
        {
            atcmd_rspdata("CWQAP:%d", -ATCMD_ERR_TIMEOUT);
            return ATCMD_ERROR;
        }
        atcmd_rspinfor("WIFI DISCONNECT");

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int cwjap_parse_sap_params(char *params, wifi_ap_cfg_params_t *config)
{
    char *cur;
    char *next = params;
    int8_t token_idx = -1;
    int chan;
    int security;

    do
    {
        cur = atcmd_next_token(&next);
        token_idx++;

        switch (token_idx)
        {
            case 0:
            {
                int ssid_len;

                ssid_len = atcmd_get_proper_ssid_psk(cur, (uint8_t *)config->ssid, WIFI_SSID_LEN);
                if (!ssid_len || (ssid_len > WIFI_SSID_LEN))
                {
                    return -ATCMD_ERR_CFG_SSID;
                }
                break;
            }
            case 1:
            {
                int key_len;

                key_len = atcmd_get_proper_ssid_psk(cur, (uint8_t *)config->pwd, WIFI_PASSWORD_LEN);
                if ((key_len > WIFI_PASSWORD_LEN) || (key_len > 0 && key_len < 8))
                {
                    return -ATCMD_ERR_CFG_KEY;
                }
                break;
            }
            case 2:
            {
                if (cur)
                {
                    config->channel = atoi(cur);
                }
                break;
            }
            case 3:
            {
                if (cur)
                {
                    security = atoi(cur);
                    if (security == WIFI_SEC_OPEN
                        || security == WIFI_SEC_WPA_PSK
                        || security == WIFI_SEC_WPA2_PSK)
                    {
                        config->sec = security;
                    }
                }
                break;
            }
            default:
                //CLOGI("%s %d\n", __func__, __LINE__);
                break;
        }
    } while(next);

    return 0;
}

int atcmd_cwsap(int type, char *params)
{
    int res;
    ls_err_t ret;
    wifi_ap_cfg_params_t config = {0};

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        res = cwjap_parse_sap_params(params, &config);
        if (res)
        {
            atcmd_rspdata("CWSAP:%d", res);
            return ATCMD_ERROR;
        }

        ret = wifi_ap_start(&config);
        if (ret != LS_OK)
        {
            atcmd_rspdata("CWSAP:%d", ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        wifi_ap_info_t ap_info = {0};

        ret = wifi_ap_get_basic_info(&ap_info);
        if (ret != LS_OK)
        {
            atcmd_rspdata("CWSAP:%d", ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        atcmd_rspdata("CWSAP:%s,%s," MACSTR ",%d,%d",
                      ap_info.ssid, ap_info.pwd, MAC2STR(ap_info.bssid), ap_info.channel, ap_info.security);

        return ATCMD_OK;
    }
}

int atcmd_cwcap(int type, char *params)
{
    ls_err_t ret;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        ret = wifi_ap_stop();

        if (ret != LS_OK)
        {
            atcmd_rspdata("CWCAP:%d", ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        return ATCMD_UNKNOWN;
    }
}

int cwlap_parse_scan_params(char *params, wifi_scan_params_t *config)
{
    char *cur;
    char *next = params;
    int8_t token_idx = -1;
    int channel;

    if (!params)
    {
        return 0;
    }

    do
    {
        cur = atcmd_next_token(&next);
        token_idx++;

        switch (token_idx)
        {
            case 0: //ssid
            {
                int ssid_len;

                if (cur)
                {
                    ssid_len = atcmd_get_proper_ssid_psk(cur, config->ssid_array, WIFI_SSID_LEN);
                    if (!ssid_len || (ssid_len > WIFI_SSID_LEN))
                    {
                        return -ATCMD_ERR_CFG_SSID;
                    }
                    config->ssid_len = ssid_len;
                }
                break;
            }
            case 1: //bssid
            {
                if (cur)
                {
                    if (atcmd_parse_mac_addr(cur, config->bssid))
                    {
                        return -ATCMD_ERR_CFG_BSSID;
                    }
                    config->bssid_set_flag = 1;
                }
                break;
            }
            case 2: //channel
            {
                if (cur)
                {
                    channel = atoi(cur);
                    if (channel > 0 && channel < 14)
                    {
                        config->channel[0] = channel;
                        config->channel_cnt = 1;
                    }
                }
                break;
            }
            default:
                //CLOGI("%s %d\n", __func__, __LINE__);
                break;
        }
    } while(next);

    return 0;
}

void cwlap_rsp_scan_results(wifi_scan_result_t *results, uint32_t num)
{
    uint32_t i;
    char scan_resp[255] = {0};
    uint8_t offset = 0;

    for (i = 0; i < num; i++)
    {
        ///????rssi§³???Ú…???AP???
        if(results[i].rssi < g_scan_filter.rssi_filter) {
            continue;
        }
        ///????????????§Û???
        if(results[i].auth == WIFI_SEC_OPEN && !(g_scan_filter.authmode_mask & SCAN_AUTH_MODE_OPEN_MASK))
            continue;
        else if(results[i].auth == WIFI_SEC_WEP && !(g_scan_filter.authmode_mask & SCAN_AUTH_MODE_WEP_MASK))
            continue;
        else if(results[i].auth == WIFI_SEC_WPA_PSK && !(g_scan_filter.authmode_mask & SCAN_AUTH_MODE_WPA_PSK_MASK))
            continue;
        else if(results[i].auth == WIFI_SEC_WPA2_PSK && !(g_scan_filter.authmode_mask & SCAN_AUTH_MODE_WPA2_PSK_MASK))
            continue;
        else if(results[i].auth == WIFI_SEC_WPA_PSK_WPA2_PSK && !(g_scan_filter.authmode_mask & SCAN_AUTH_MODE_WPA_WPA2_PSK_MASK))
            continue;
        else if(results[i].auth == WIFI_SEC_WPA_ENTERPRISE && !(g_scan_filter.authmode_mask & SCAN_AUTH_MODE_WPA2_ENTERPRISE_MASK))
            continue;
        else if(results[i].auth == WIFI_SEC_WPA3_SAE && !(g_scan_filter.authmode_mask & SCAN_AUTH_MODE_WPA3_PSK_MASK))
            continue;
        else if(results[i].auth == WIFI_SEC_WPA2_PSK_WPA3_SAE && !(g_scan_filter.authmode_mask & SCAN_AUTH_MODE_WPA2_WPA3_PSK_MASK))
            continue;
        ///???????mask???§Ý???????mask?0?????????????+CWLAP?????
        offset = 0;
        memset(scan_resp, 0, sizeof(scan_resp));
        strcat(scan_resp, "CWLAP:");
        offset += strlen("CWLAP:");
        if(g_scan_filter.print_mask & SCAN_PRINT_ENC_MASK) {
            offset += sprintf(&scan_resp[offset], "%d,", results[i].auth);
        }
        if(g_scan_filter.print_mask & SCAN_PRINT_SSID_MASK) {
            strcat(&scan_resp[offset], results[i].ssid);
            offset += results[i].ssid_len;
            strcat(&scan_resp[offset], ",");
            offset += 1;
        }
        if(g_scan_filter.print_mask & SCAN_PRINT_RSSI_MASK) {
            offset += sprintf(&scan_resp[offset], "%d,", results[i].rssi);
        }
        if(g_scan_filter.print_mask & SCAN_PRINT_MAC_MASK) {
            offset += sprintf(&scan_resp[offset], MACSTR, MAC2STR(results[i].bssid));
            strcat(&scan_resp[offset], ",");
            offset += 1;
        }
        if(g_scan_filter.print_mask & SCAN_PRINT_CHANNEL_MASK) {
            offset += sprintf(&scan_resp[offset], "%d,", results[i].channel);
        }
        if(g_scan_filter.print_mask & SCAN_PRINT_BGN_MASK) {
            offset += sprintf(&scan_resp[offset], "%d,", results[i].mode);
        }
        if(g_scan_filter.print_mask & SCAN_PRINT_WPS_MASK) {
            offset += sprintf(&scan_resp[offset], "%d", results[i].wps);
        }
        ///+CWLAP:(<ecn>,<ssid>,<rssi>,<mac>,<channel>,<bgn>,<wps>)
        atcmd_rspdata("%s", scan_resp);
        // atcmd_rspdata("CWLAP:%d,\"%s\",%d," MACSTR ",%d,%d,%d",
        //               results[i].auth, results[i].ssid, results[i].rssi,
        //               MAC2STR(results[i].bssid), results[i].channel,
        //               results[i].mode, results[i].wps);
    }
}

int atcmd_cwlap(int type, char *params)
{
    int res;
    ls_err_t ret;
    wifi_scan_params_t config = {0};
    int ap_num;
    wifi_scan_result_t *results;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        res = cwlap_parse_scan_params(params, &config);
        if (res)
        {
            atcmd_rspdata("CWLAP:%d", res);
            return ATCMD_ERROR;
        }

        ls_event_clear(EVENT_WIFI, EVENT_WIFI_SCAN_DONE);
        ret = wifi_scan_start(&config);
        if (ret != LS_OK)
        {
            atcmd_rspdata("CWLAP:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        ret = ls_event_wait(EVENT_WIFI, EVENT_WIFI_SCAN_DONE, 3000);
        if (ret != LS_OK)
        {
            atcmd_rspdata("CWLAP:%d", -ATCMD_ERR_TIMEOUT);
            return ATCMD_ERROR;
        }

        wifi_get_sta_scanlist_nums(&ap_num);
        if (ap_num == 0)
        {
            atcmd_rspdata("CWLAP:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }

        results = (wifi_scan_result_t *)rtos_malloc(sizeof(wifi_scan_result_t) * ap_num);
        if (!results)
        {
            atcmd_rspdata("CWLAP:%d", -ATCMD_ERR_NO_MEM);
            return ATCMD_ERROR;
        }
        memset(results, 0, sizeof(wifi_scan_result_t) * ap_num);

        wifi_sta_scanlist_dump(results, ap_num, &ap_num);
        if (ap_num)
        {
            cwlap_rsp_scan_results(results, ap_num);
        }

        rtos_free(results);
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int cwlapopt_parse_params(char *params, wifi_scan_result_filter_t *filter_config)
{
    char *cur;
    char *next = params;
    int8_t token_idx = -1, rssi_filter = 0;
    uint16_t print_mask = 0, authmode_mask = 0;

    if (!params) {
        return -1;
    }
    do {
        cur = atcmd_next_token(&next);
        token_idx++;
        switch (token_idx)
        {
            case 0: //print mask
            {
                if(cur) {
                    print_mask = atoi(cur);
                    filter_config->print_mask = print_mask;
                }
                break;
            }
            case 1: //rssi filter:[?C100,40]
            {
                if(cur) {
                    rssi_filter = atoi(cur);
                    if(rssi_filter <= 40 && rssi_filter >= -100) {
                        filter_config->rssi_filter = rssi_filter;
                    }
                }
                break;
            }
            case 2: //authmode mask
            {
                if (cur)
                {
                    authmode_mask = atoi(cur);
                    filter_config->authmode_mask = authmode_mask;
                }
                break;
            }
            default:
                break;
        }
    } while(next);

    return 0;
}

int atcmd_cwlapopt(int type, char *params)
{
    int res;
    ls_err_t ret;

    if (type == ATCMD_PARAM) {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC) {

        if(cwlapopt_parse_params(params, &g_scan_filter)) {
            atcmd_rspdata("CWLAPOPT:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        return ATCMD_UNKNOWN;
    }
}

int atcmd_cwlif(int type, char *params)
{
    ls_err_t ret;
    int i;
    int j = 0;
    wifi_sta_basic_info_t *sta_info;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        sta_info = (wifi_sta_basic_info_t *)rtos_malloc(sizeof(wifi_sta_basic_info_t) * CFG_STA_MAX);
        if (!sta_info)
        {
            atcmd_rspdata("CWLIF:%d", -ATCMD_ERR_NO_MEM);
            return ATCMD_ERROR;
        }
        memset(sta_info, 0, sizeof(wifi_sta_basic_info_t) * CFG_STA_MAX);

        for (i = 0; i < CFG_STA_MAX; i++)
        {
            ret = wifi_ap_get_sta_info(&sta_info[j] , i);
            if (ret == LS_OK)
            {
                j++;
            }
        }

        if (j == 0)
        {
            rtos_free(sta_info);
            atcmd_rspdata("CWLIF:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }

        for (i = 0; i < j; i++)
        {
            atcmd_rspdata("CWLIF:" MACSTR , MAC2STR(sta_info[i].sta_mac));
        }

        rtos_free(sta_info);
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int atcmd_cwautoconn(int type, char *params)
{
    ls_err_t ret;
    uint8_t enable = 0;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        params = atcmd_next_token(&params);
        if (!params)
        {
            atcmd_rspdata("CWAUTOCONN:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        enable = atoi(params);

        #if CFG_NVS
        nvds_put(NVDS_TAG_WIFI_STA_AUTOCONN, NVDS_LEN_WIFI_STA_AUTOCONN, &enable);
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int atcmd_sta_auto_conn(void)
{
#if CFG_NVS
    uint8_t sta_auto_conn_en;
    uint8_t ssid[WIFI_SSID_LEN + 1] = {0};
    uint8_t pwd[WIFI_PASSWORD_LEN + 1] = {0};
    size_t len;
    int ret;
    wifi_connect_cfg_t sta_config = {0};

    len = NVDS_LEN_WIFI_STA_AUTOCONN;
    ret = nvds_get(NVDS_TAG_WIFI_STA_AUTOCONN, &len, &sta_auto_conn_en);
    if (ret == NVDS_OK && sta_auto_conn_en == 1)
    {
        len = WIFI_SSID_LEN;
        ret = nvds_get(NVDS_TAG_WIFI_STA_SSID, &len, ssid);
        if (ret == NVDS_OK && len <= WIFI_SSID_LEN)
        {
            memcpy(sta_config.ssid, ssid, len);

            len = WIFI_PASSWORD_LEN;
            ret = nvds_get(NVDS_TAG_WIFI_STA_PWD, &len, pwd);
            if (ret == NVDS_OK)
            {
                if (len > WIFI_PASSWORD_LEN)
                {
                    return -1;
                }
                memcpy(sta_config.key, pwd, len);
            }

            ret = wifi_sta_connect(&sta_config);
            if (ret == LS_OK)
            {
                return 0;
            }
        }
    }
#endif
    return -1;
}

int atcmd_cwmode(int type, char *params)
{
    ls_err_t ret;
    uint8_t enable = 0;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {

        return ATCMD_UNKNOWN;
    }
    else
    {
        wifi_mode_e mode;
        wifi_get_mode(WIFI_VIF_DEFAULT_IDX, &mode);
        atcmd_rspdata("CWMODE:%d", mode);
        return ATCMD_OK;
    }
}

int atcmd_cwreconncfg(int type, char *params)
{
    ls_err_t ret;
    uint8_t enable = 0;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {

        return ATCMD_OK;
    }
    else
    {
        return ATCMD_UNKNOWN;
    }
}

static int atcmd_cipsta(int type, char *params)
{
    ls_err_t ret;
    struct ip_addr_cfg cfg = {0};

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        ///???????IP???????
        return ATCMD_OK;
    }
    else
    {
        ret = ls_get_ip(WIFI_VIF_DEFAULT_IDX, &cfg);
        if (ret) {
            atcmd_rspdata("CIPSTA:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }
        else {
            atcmd_rspdata("CIPSTA:ip:%d.%d.%d.%d",
                        cfg.ipv4.addr & 0xff, (cfg.ipv4.addr >> 8) & 0xff,
                        (cfg.ipv4.addr >> 16) & 0xff, (cfg.ipv4.addr >> 24) & 0xff);
            atcmd_rspdata("CIPSTA:gateway:%d.%d.%d.%d",
                        cfg.ipv4.mask & 0xff, (cfg.ipv4.mask >> 8) & 0xff,
                        (cfg.ipv4.mask >> 16) & 0xff, (cfg.ipv4.mask >> 24) & 0xff);
            atcmd_rspdata("CIPSTA:netmask:%d.%d.%d.%d",
                        cfg.ipv4.gw & 0xff, (cfg.ipv4.gw >> 8) & 0xff,
                        (cfg.ipv4.gw >> 16) & 0xff, (cfg.ipv4.gw >> 24) & 0xff);
        }
        return ATCMD_OK;
    }
}

static int atcmd_cwcountry(int type, char *params)
{
    ls_err_t ret;
    char code[3] = {0};

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        char *token;
        token = utils_next_token(&params);
        ret = wifi_set_country_code(token);
        if (ret) {
            atcmd_rspdata("CWCOUNTRY:%d", -ATCMD_ERR_CFG_COUNTRY);
            return ATCMD_ERROR;
        }
        return ATCMD_OK;
    }
    else
    {
        ret = wifi_get_country_code(code);
        if (ret) {
            atcmd_rspdata("CWCOUNTRY:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }
        atcmd_rspdata("CWCOUNTRY:country code:%s", code);
        return ATCMD_OK;
    }
}

static int atcmd_cipstamac(int type, char *params)
{
    ls_err_t ret;
    uint8_t mac[6] = {0};
    char *token, *next = params;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        token = utils_next_token(&next);
        ret = utils_parse_mac_addr(token, mac);
        if (ret) {
            atcmd_rspdata("CIPSTAMAC:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }
        ret = wifi_mac_set(mac);
        if (ret) {
            atcmd_rspdata("CIPSTAMAC:%d", ret);
            return ATCMD_ERROR;
        }
        return ATCMD_OK;
    }
    else
    {
        ret = wifi_get_sta_mac(mac);
        if (ret) {
            atcmd_rspdata("CIPSTAMAC:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }
        else {
            atcmd_rspdata("CIPSTAMAC:"MACSTR, MAC2STR(mac));
        }
        return ATCMD_OK;
    }
}

int atcmd_cwchan(int type, char *params)
{
    ls_err_t ret;
    int chan = 0;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        return ATCMD_UNKNOWN;
    }
    else
    {
        ret = wifi_get_channel(&chan);
        if (ret) {
            atcmd_rspdata("CWCHAN:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }
        atcmd_rspdata("CWCHAN:%d", chan);
        return ATCMD_OK;
    }
}

int atcmd_cwrssi(int type, char *params)
{
    ls_err_t ret;
    int rssi = 0;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        return ATCMD_UNKNOWN;
    }
    else
    {
        ret = wifi_get_ap_rssi(&rssi);
        if (ret) {
            atcmd_rspdata("CWRSSI:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }
        atcmd_rspdata("CWRSSI:%d", rssi);
        return ATCMD_OK;
    }
}

int atcmd_cwstate(int type, char *params)
{
    ls_err_t ret;
    wifi_link_status_t link_status = {0};

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        return ATCMD_UNKNOWN;
    }
    else
    {
        ret = wifi_get_link_status(&link_status);
        if (ret) {
            atcmd_rspdata("CWSTATE:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }
        switch(link_status.state)
        {
            case STA_INACTIVE:
                atcmd_rspinfor("STA state: INACTIVE \r\n");
                break;
            case STA_IN_CONNECTING:
                atcmd_rspinfor("STA state: CONNECTING\r\n");
                break;
            case STA_CONNECTED:
                atcmd_rspinfor("STA state: CONNECTED\r\n");
                break;
            case STA_DISCONNECTED:
                atcmd_rspinfor("STA state: DISCONNECTED\r\n");
                break;
            default:
                atcmd_rspinfor("Not in STA mode \r\n");
                break;
        }
        if (link_status.is_connected) {
            ///+CWSTATE:(<ssid>,<link_state>,<aid>,<channel>,<rssi>,<bssid>)
            atcmd_rspdata("CWSTATE:\"%s\",%d,%d,%d,%d," MACSTR,
                link_status.ssid, link_status.state, link_status.aid,
                link_status.channel, link_status.rssi, MAC2STR(link_status.bssid));
        }
        return ATCMD_OK;
    }
}

int atcmd_cwrate(int type, char *params)
{
    ls_err_t ret;
    int rate = 0;
    char *token, *next = params;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        token = utils_next_token(&next);
        rate = strtol(token, NULL, 16);
        if (rate < 0 || rate == 0x4 || (rate > 0x7 && rate < 0x10) \
            || (rate > 0x17 && rate < 0x20) || (rate > 0x29 && rate < 0x30) \
            || (rate > 0x39 && rate < 0x40) || (rate > 0x49 && rate < 0xff))
        {
            atcmd_rspinfor("invalid rate value 0x%x", rate);
            return ATCMD_ERROR;
        }
        ret = wifi_rate_config(rate);
        if (ret) {
            atcmd_rspinfor("%s", wifi_err_to_str(ret));
            return ATCMD_ERROR;
        }
        return ATCMD_OK;
    }
    else
    {
        return ATCMD_OK;
    }
}

int atcmd_cwpw(int type, char *params)
{
    ls_err_t ret;
    char *token, *next = params;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        while ((token = utils_next_token(&next)))
        {
            if (strncmp(token, "on", 2) == 0)
            {
                wifi_ps_mode_set(WIFI_PS_DEFAULT_TYPE);
            }
            else if (strncmp(token, "off", 3) == 0)
            {
                wifi_ps_mode_set(WIFI_PS_MODE_OFF);
            }
            else
            {
                atcmd_rspdata("CIPSTAMAC:%d", -ATCMD_ERR_UNSPECIF);
                return ATCMD_ERROR;
            }
        }
        return ATCMD_OK;
    }
    else
    {
        return ATCMD_UNKNOWN;
    }
}

int atcmd_cwlsinterval(int type, char *params)
{
    ls_err_t ret;
    uint8_t listen_itv = 0;
    char *token, *next = params;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        token = utils_next_token(&next);
        listen_itv = atoi(token);
        atcmd_rspinfor("params = %s, token = %s, listen_itv = %d", params, token, listen_itv);
        ret = wifi_sta_set_listen_itv(listen_itv);
        if (ret) {
            atcmd_rspdata("CWLSINTERVAL:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }
        return ATCMD_OK;
    }
    else
    {
        ret = wifi_sta_get_listen_itv(&listen_itv);
        if (ret) {
            atcmd_rspdata("CWLSINTERVAL:%d", -ATCMD_ERR_QUERY_NO_RESULT);
            return ATCMD_ERROR;
        }
        atcmd_rspdata("CWLSINTERVAL:%d", listen_itv);
        return ATCMD_OK;
    }
}

int atcmd_cwsend80211(int type, char *params)
{
    ls_err_t ret;
    wifi_80211_tx_info_t tx_info;
    uint8_t pkt[128] = {0x88, 0x42, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x26, 0x7f, 0x25, 0x00, 0x05,
        0x2a, 0xa7, 0x8a, 0x51, 0x71, 0x39,
        0x20, 0x00,
        0x00, 0x00,
        0x03, 0x00, 0x00, 0x60, 0x00, 0x00};
    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        tx_info.pkt = pkt;
        tx_info.len = 128;
        tx_info.wifi_vif_idx = WIFI_VIF_DEFAULT_IDX;
        ret = wifi_send_80211_frame(&tx_info);
        if (ret) {
            atcmd_rspdata("CWSEND802.11:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }
        return ATCMD_OK;
    }
    else
    {
        return ATCMD_UNKNOWN;
    }
}

static void monitor_cb(wifi_sniffer_frame_info_t *info, void *cb_arg)
{
    static int i = 0;

    if (info->payload){
       i++;
    }
    if (i % 30 == 0) {
        atcmd_rspinfor("Caputured  frame %d", i);
    }
}

int atcmd_cwsniffer(int type, char *params)
{
    ls_err_t ret;
    wifi_sniffer_para_t sniffer_item = {0};
    char *token, *next = params;
    uint16_t chan = 0;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        token = atcmd_next_token(&next);
        if (strncmp(token, "off", 3) == 0)
        {
            sniffer_item.itf_idx = WIFI_VIF_SNIFFER_IDX;
            ret = wifi_sniffer_disable(&sniffer_item);
            return ATCMD_OK;
        } else if(strncmp(token, "on", 2) == 0) {
            token = atcmd_next_token(&next);
            chan = atoi(token);
            if(chan < 1 || chan > 13) {
                atcmd_rspdata("CWSNIFFER:%d", -ATCMD_ERR_UNSPECIF);
                return ATCMD_ERROR;
            }
            sniffer_item.itf_idx = WIFI_VIF_SNIFFER_IDX;
            sniffer_item.prim20_freq = 2407 + 5 * chan;//chan_to_freq(chan);
            sniffer_item.cb = monitor_cb;
            ret = wifi_sniffer_enable(&sniffer_item);
            if (ret) {
                atcmd_rspdata("CWSNIFFER:%d", -ATCMD_ERR_CFG_SNIFFER);
                return ATCMD_ERROR;
            }
            return ATCMD_OK;
        }
        else {
            atcmd_rspdata("CWSNIFFER:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }
    }
    else
    {
        return ATCMD_UNKNOWN;
    }
}

/** There are 4 kinds of AT cmd
 *  (1) AT+<x>=?      (ATCMD_PARAM)  at cmd to get all configurable parameters range
 *  (2) AT+<x>?       (ATCMD_QUERY)  at cmd to get informations
 *  (3) AT+<x>=<...>  (ATCMD_EXEC)   at cmd with parameters
 *  (4) AT+<x>        (ATCMD_EXEC)   at cmd without parameters
**/
const atcmd_item_t atcmd_wifi_table[] =
{
    ///???
    {atcmd_cwlap, "AT+CWLAP",
                  "AT+CWLAP : default scan without parameter specified\r\n"
                  "AT+CWLAP=[<ssid>],[<bssid>],[<channel>]"
                  " : use this cmd to do scan\r\n"
                  "<ssid> format example: \"test_ap\"\r\n"
                  "<bssid> format example: 00:11:22:33:44:55\r\n"
                  "<channel> 1 ~ 14\r\n"
                  "the rsp is scaned ap info, format is as below:"
                  "+CWLAP:(<ecn>,<ssid>,<rssi>,<mac>,<channel>,<bgn>,<wps>)\r\n"
                  "<enc> refers to wifi_security_e\r\n"},
    ///????
    {atcmd_cwjap, "AT+CWJAP",
                  "AT+CWJAP=<ssid>,[<pwd>],[<bssid>],[<dhcp_mode>],[<ip>],[<mask>],[<gw>]"
                  " : use this cmd to connect AP\r\n"
                  "<ssid> format example: \"test_ap\"\r\n"
                  "<pwd> format example: \"12345678\"\r\n"
                  "<bssid> format example: 11:22:33:44:55:66 \r\n"
                  "<dhcp_mode>: 0 or 1, 0 means dhcp client, 1 means static ipv4\r\n"
                  "<ip> format example: 192.168.1.100, valid only when dhcp_mode is 1\r\n"
                  "<mask> format example: 255.255.255.0, valid only when dhcp_mode is 1\r\n"
                  "<gw> format example: 192.168.1.1, valid only when dhcp_mode is 1\r\n"
                  "AT+CWJAP?"
                  " : use this command to get infor of connected AP, rsp is as below:\r\n"
                  "+CWJAP:<is_connected>,<ssid>,<bssid>,<channel>,<rssi>,<aid>,<ip>\r\n"},
    ///????
    {atcmd_cwqap, "AT+CWQAP", "disconnect connected AP\r\n"},
    ///?????softAP??
    {atcmd_cwsap, "AT+CWSAP",
                  "AT+CWSAP=<ssid>,[<pwd>],[<channel>],[<security>]"
                  " : use this cmd to start softap mode\r\n"
                  "<ssid> format example: \"test_ap\"\r\n"
                  "<pwd> format example: \"12345678\"\r\n"
                  "<channel>: 1 ~ 13, if not set, will use a default channel\r\n"
                  "<security>: 1/3/4(1:OPEN, 3:WPA, 4:WPA2), if not set, can determine automatically\r\n"
                  "AT+CWSAP?"
                  " : use this command to get infor of started softap:\r\n"
                  "+CWSAP:<ssid>,<pwd>,<bssid>,<channel>,<security>\r\n"},
    ///???softAP??
    {atcmd_cwcap, "AT+CWCAP", "close softap mode\r\n"},
    ///????AT+CWLAP????????????????<print mask>[,<rssi filter>][,<authmode mask>]
    {atcmd_cwlapopt, "AT+CWLAPOPT", "set scan result filter"},
    ///???softAP?????????????????station???
    {atcmd_cwlif, "AT+CWLIF", "get connected sta info in softap mode, format is as below:\r\n"
                  "+CWLIF:<mac>,<ip>\r\n"},

    ///????station??????????AP
    {atcmd_cwautoconn, "AT+CWAUTOCONN", "set dev to auto connect AP after dev power on:\r\n"
                       "AT+CWAUTOCONN=<enable>\r\n"},
    ///????õô?? Wi-Fi ??
    {atcmd_cwmode, "AT+CWMODE", "AT+CWMODE?: get WIFI mode\r\n"
                   "<mode> refers to wifi_mode_e\r\n"},
    ///??? Wi-Fi ???????¨¢?interval_second:?????????repeat_count??????????
    {atcmd_cwreconncfg, "AT+CWRECONNCFG", "set the Wi-Fi reconnection configuration\r\n"
                        "AT+CWRECONNCFG=<interval_second>,<repeat_count>\r\n"},
    ///???????station????IP???,AT+CIPSTA=<"ip">[,<"gateway">,<"netmask">]
    {atcmd_cipsta, "AT+CIPSTA", "AT+CIPSTA?: get station ip\r\n <ip> ip addr\r\n"},
    ///???/???? Wi-Fi ???????
    {atcmd_cwcountry, "AT+CWCOUNTRY", "AT+CWCOUNTRY?: wifi get country\r\n"
                   "<country code> wifi set country: US/EU/CN/JP\r\n"},
    ///???/????MAC???
    {atcmd_cipstamac, "AT+CIPSTAMAC", "AT+CIPSTAMAC?: get station mac address\r\n"
                   "<mac> mac addr\r\n"},
    ///??????station????channel
    {atcmd_cwchan, "AT+CWCHAN", "AT+CWCHAN?: get current operating channel\r\n"},
    ///???????????AP??rssi?
    {atcmd_cwrssi, "AT+CWRSSI", "AT+CWRSSI?: get sta mode rssi strength in connection state\r\n"},
    ///??????station link???????,resp:<ssid>,<link_state>,<aid>,<channel>,<rssi>,<bssid>
    {atcmd_cwstate, "AT+CWSTATE", "AT+CWSTATE?: get sta link status\r\n"},
    ///????wifi????????,???????????AP????????
    {atcmd_cwrate, "AT+CWRATE", "<rate>: refer rate define in wifi_phy_rate_e\r\n"
                    "auto rate 0xff \r\n"
                    "11B 1Mbps ~ 11Mbps long preamble : 0x0 ~ 0x3 \r\n"
                    "11B 2Mbps ~ 11Mbps short preamble : 0x5 ~ 0x7 \r\n"
                    "11G 6Mbps ~ 54Mbps  : 0x10 ~ 0x17 \r\n"
                    "MCS0 ~ MCS9 (Long GI or 11ax 1.6 us GI) : 0x20 ~ 0x29\r\n"
                    "MCS0 ~ MCS9 (Short GI or 11ax 0.8 us GI) : 0x30 ~ 0x39\r\n"
                    "MCS0 ~ MCS9 ( 11ax 3.2 us GI) : 0x40 ~ 0x49\r\n"},
    ///????wifi????power save????on??off?????,???????SYS_PSM=1
    {atcmd_cwpw, "AT+CWPW", "AT+CWPW=<on/off>: on/off wifi powersave\r\n"},
    ///????/??? wifi listen interval
    {atcmd_cwlsinterval, "AT+CWLSINTERVAL", "AT+CWLSINTERVAL?,AT+CWLSINTERVAL=<value>,max value should less than 20\r\n"},
    ///????802.11?
    {atcmd_cwsend80211, "AT+CWSEND80211", "AT+CWSEND80211 wifi send 802.11 packet\r\n"},
    ///???/??? sniffer??
    {atcmd_cwsniffer, "AT+CWSNIFFER", "AT+CWSNIFFER=<on/off>,[<channel>: 1~13]\r\n"},
};

void atcmd_wifi_register(void)
{
    atcmd_entry_add_table(atcmd_wifi_table, sizeof(atcmd_wifi_table)/sizeof(atcmd_item_t));
}

void atcmd_wifi_help(void)
{
    int i;
    int item_len;
    item_len = sizeof(atcmd_wifi_table)/sizeof(atcmd_item_t);
    for (i = 0; i < item_len; i++)
        CLOGI("%s: %s\n", atcmd_wifi_table[i].atcmd_entry.name, atcmd_wifi_table[i].atcmd_entry.help);
}
