/**
 ****************************************************************************************
 *
 * @file cli_wifi.c
 *
 * @brief Cli cmd for Wifi
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */
#include "cli_main.h"
#include "ls_event.h"
#include "ls_wifi_type.h"
#include "ls_err.h"
#include "wifi_api.h"
#include "nvs.h"
#include "nvds_tag_def.h"
#include "lwip/ip_addr.h"
#include "net_ip.h"
#include "PSRAMManager.h"
#if WIFI_OTA
#include "wifi_ota.h"
#endif
#include "cache.h"
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
static const struct cli_cmd cli_wifi_commands[];

char *wifi_err_to_str(uint32_t code)
{
    switch (code)
    {
        case LS_ERR_WIFI_NOT_CONNECT:
            return "NOT CONNECT";
        case LS_ERR_WIFI_NOT_AX:
            return "NOT 11AX MODE";
        case LS_ERR_WIFI_NOT_STA_MODE:
            return "NOT IN STA MODE";
        case LS_ERR_WIFI_NOT_AP_MODE:
            return "NOT IN AP MODE";
        case LS_ERR_WIFI_INVALID_RATE:
            return "INVALID RATE";
        case LS_ERR_WIFI_INVALID_COUNTRY_CODE:
            return "INVALID COUNTRY CODE";
        case LS_ERR_WIFI_DIS_NETWORK:
            return "DISABLE NETWORK FAIL";
        case LS_ERR_WIFI_VIF_OPT:
            return "VIF OPERATION FAIL";
        case LS_ERR_PARAM:
            return "INVALID PARAMETER";
        default:
            return "FAIL";
    }
}

static int wifi_cli_set_country_code(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
	char *token;

    token = utils_next_token(&params);
    CLI_LOG("set country code %s \r\n", token);

    ret = wifi_set_country_code(token);

    if (ret)
    {
        CLI_LOGE(" %s \r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_get_country_code(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    char code[3] = {0};

    ret = wifi_get_country_code(code);

    if (ret)
    {
        CLI_LOGE(" %s \r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    } 
    else
    {
        CLI_LOG("country code %s \r\n", code);
    }

    return res;
}


static int wifi_cli_get_channel(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    int chan = 0;

    ret = wifi_get_channel(&chan);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
        CLI_LOG("channel %d \r\n", chan);
    }

    return res;
}


static char *wifi_sec_to_str(uint8_t auth)
{
    switch (auth) {
        case WIFI_SEC_AUTO:
        {
            return "AUTO";
        }
        break;
        case WIFI_SEC_WPA3_SAE:
        {
            return "WPA3-SAE";
        }
        break;
        case WIFI_SEC_WPA2_PSK_WPA3_SAE:
        {
            return "WPA2-PSK/WPA3-SAE";
        }
        break;
        case WIFI_SEC_OPEN:
        {
            return "Open";
        }
        break;
        case WIFI_SEC_WEP:
        {
            return "WEP";
        }
        break;
        case WIFI_SEC_WPA_PSK:
        {
            return "WPA-PSK";
        }
        break;
        case WIFI_SEC_WPA2_PSK:
        {
            return "WPA2-PSK";
        }
        break;
        case WIFI_SEC_WPA_PSK_WPA2_PSK:
        {
            return "WPA2-PSK/WPA-PSK";
        }
        break;
        case WIFI_SEC_WPA_ENTERPRISE:
        {
            return "WPA/WPA2-Enterprise";
        }
        break;
        case WIFI_SEC_UNKNOWN:
        {
            return "Unknown";
        }
        break;
        default:
        {
            return "Unknown";
        }
    }
}


static uint8_t wifi_str_to_sec(char *str, wifi_security_e *sec)
{
    uint8_t ret = 0;
    uint8_t len = strlen(str);

    if (strncasecmp(str, "AUTO", 4) == 0 && (len == 4))
    {
        *sec = WIFI_SEC_AUTO;
    }
    else if (strncasecmp(str, "OPEN", 4) == 0 && (len == 4))
    {
        *sec = WIFI_SEC_OPEN;
    }
    else if (strncasecmp(str, "WEP", 3) == 0 && (len == 3))
    {
        *sec = WIFI_SEC_WEP;
    }
    else if (((strncasecmp(str, "RSN", 3) == 0) && (len == 3)) ||
             ((strncasecmp(str, "WPA2", 4) == 0) && (len == 4)))
    {
        *sec = WIFI_SEC_WPA2_PSK;
    }
    else if (((strncasecmp(str, "SAE", 3) == 0) && (len == 3)) ||
             ((strncasecmp(str, "WPA3", 4) == 0) && (len == 4)))
    {
        *sec = WIFI_SEC_WPA3_SAE;
    }
    else if (strncasecmp(str, "WPA", 3) == 0 && (len == 3))
    {
        *sec = WIFI_SEC_WPA_PSK;
    }
    else
    {
        ret = -1;
    }
    return ret;
}


static uint8_t wifi_freq_in_range(uint16_t freq)
{
    uint8_t num, ret = 0;

    for (num = 0; num < 13; num++)
    {
        if (freq == (2412 + num *5))
        {
            ret = 1;
            return ret;
        }
    }

    if (freq == 2484)
    {
        ret = 1;
    }
    // 5G band to do later

    return ret;
}

static int wifi_cli_scan_params(wifi_scan_params_t *cfg, char *params)
{
    int res = CLI_SUCCESS;
    char *token, *next = params;

    while ((token = utils_next_token(&next))) {
        char option;

        if (res || (token[0] != '-') || (token[2] != '\0')) {
            CLI_LOGE("[%s]: SCAN parameter error \r\n", __func__);
            res = CLI_SHOW_USAGE;
            break;
        }

        option = token[1];
        token = utils_next_token(&next);
        if (!token) {
            res = CLI_SHOW_USAGE;
            break;
        }
        switch (option) {
        case 's':
        {
            size_t ssid_len;

            ssid_len = utils_get_proper_ssid_psk(token, cfg->ssid_array, WIFI_SSID_LEN);
            if (!ssid_len || (ssid_len > WIFI_SSID_LEN))
            {
                res = -CLI_ERR_CFGSSID;
                break;
            }
            cfg->ssid_len = ssid_len;
            break;
        }
        case 'b':
            if (utils_parse_mac_addr(token, cfg->bssid))
            {
                res = -CLI_ERR_CFGBSSID;
                break;
            }
            cfg->bssid_set_flag = 1;
            break;
        case 'c':
        {
            unsigned int i;
            char *next_freq = strchr(token, ',');
            for (i = 0; i < MAX_CHANNELS_NUM; i++) {
                cfg->channel[i] = atoi(token);
                cfg->channel_cnt ++;
                if (!next_freq)
                    break;
                *next_freq++ = '\0';
                token = next_freq;
                next_freq = strchr(token, ',');
            }
            break;
        }

        case 'd':
        {
            cfg->duration = atoi(token);
            break;
        }

        default:
            res = CLI_SHOW_USAGE;
            break;
        }
    }

    if (res) {
        CLI_LOGE("[%s]: Exec cmd err = 0x%02x\n", __func__, res);
        res = CLI_SHOW_USAGE;
    }

    return res;
}

#define MAX_SCAN_NUM 32
static int wifi_cli_scan(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    int nb_res, i;
    wifi_scan_params_t scan_param = {0};
    wifi_scan_result_t *scan_results;

    res = wifi_cli_scan_params(&scan_param, params);
    if (res)
    {
        return res;
    }

    ls_event_clear(EVENT_WIFI, EVENT_WIFI_SCAN_DONE);
    ret = wifi_scan_start(&scan_param);
    if (ret != LS_OK)
    {
        CLI_LOGE("Scan error %d\n",ret);
        return CLI_ERROR;
    }

    ls_event_wait(EVENT_WIFI, EVENT_WIFI_SCAN_DONE, LS_NEVER_TIMEOUT);
    // as scan_results will be write on the other core and read on this core,
    // should use rtos_aligned_malloc, aligned by HAL_DCACHE_CFG_LINE_SIZE
    scan_results = rtos_aligned_malloc(sizeof(wifi_scan_result_t) * MAX_SCAN_NUM, HAL_DCACHE_CFG_LINE_SIZE);
    if (!scan_results)
    {
       CLI_LOGE("MALLOC FAIL");
       res = CLI_ERROR;
       return res;
    }
    wifi_sta_scanlist_dump(scan_results, MAX_SCAN_NUM, &nb_res);

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if ((scan_results >= PSRAM_BASE_ADDRESS) && DCachePresent())
    {
        vPortEnterCritical();
        HAL_InvalidateDCache_by_Addr((uint32_t*)scan_results, sizeof(wifi_scan_result_t) * MAX_SCAN_NUM);
        vPortExitCritical();
    }
#endif

    CLI_LOG("Got %d scan results\n", nb_res);
    for (i = 0; i < nb_res; i++)
    {
            CLI_LOG("index[%02d]: channel %02u, bssid %02X:%02X:%02X:%02X:%02X:%02X, rssi %4d, auth %20s SSID %s\n",
                    i,
                    (wifi_scan_result_t *)(scan_results + i)->channel,
                    (wifi_scan_result_t *)(scan_results + i)->bssid[0],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[1],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[2],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[3],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[4],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[5],
                    (wifi_scan_result_t *)(scan_results + i)->rssi,
                    wifi_sec_to_str((wifi_scan_result_t *)(scan_results + i)->auth),
                    (wifi_scan_result_t *)(scan_results + i)->ssid);
    }

    rtos_aligned_free(scan_results);

    return res;
}

static int wifi_cli_get_scan_results(char *params)
{
    ls_err_t ret = LS_OK;
    wifi_scan_result_t *scan_results;
    int8_t cnt=0, i=0;

    ret = wifi_get_scan_result(&scan_results,&cnt);

    for (i = 0; i < MAX_AP_SCAN; i++)
    {
        if ((wifi_scan_result_t *)(scan_results + i)->is_used)
        {
            CLI_LOG("index[%02d]: channel %02u, bssid %02X:%02X:%02X:%02X:%02X:%02X, rssi %4d, auth %20s SSID %s\n",
                    i,
                    (wifi_scan_result_t *)(scan_results + i)->channel,
                    (wifi_scan_result_t *)(scan_results + i)->bssid[0],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[1],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[2],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[3],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[4],
                    (wifi_scan_result_t *)(scan_results + i)->bssid[5],
                    (wifi_scan_result_t *)(scan_results + i)->rssi,
                    wifi_sec_to_str((wifi_scan_result_t *)(scan_results + i)->auth),
                    (wifi_scan_result_t *)(scan_results + i)->ssid);
        }
    }

    return ret;
}


static int wifi_cli_disconnect(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;

    ret = wifi_sta_disconnect();
    //ls_dhcp_stop(WIFI_VIF_STA_IDX);
    if (ret)
    {
        CLI_LOGE(" %s \r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_connet_params(wifi_connect_cfg_t *cfg, char *params)
{
    int res = LS_OK;
    char *token, *next = params;
    int str_len = 0, record = 0;
    uint32_t ip, mask;
    bool is_ssid_configured = false;

    while ((token = utils_next_token(&next))) {
        char option;

        if (res || (token[0] != '-') || (token[2] != '\0')) {
            CLI_LOGE("[%s]: connect parameter error \r\n", __func__);
            res = CLI_SHOW_USAGE;
            break;
        }

        option = token[1];
        token = utils_next_token(&next);
        if (!token) {
            res = CLI_SHOW_USAGE;
            break;
        }
        switch (option) {
        case 's':
        {
            size_t ssid_len;

            ssid_len = utils_get_proper_ssid_psk(token, cfg->ssid, WIFI_SSID_LEN);
            if (!ssid_len || (ssid_len > WIFI_SSID_LEN))
            {
                res = -CLI_ERR_CFGSSID;
                break;
            }
            is_ssid_configured = true;
            break;
        }
        case 'b':
        {
            if (utils_parse_mac_addr(token, cfg->bssid))
                res = -CLI_ERR_CFGBSSID;
            break;
        }
        case 'k':
        {
            size_t key_len;

            key_len = utils_get_proper_ssid_psk(token, (uint8_t *)cfg->key, WIFI_PASSWORD_LEN);
            if (key_len > WIFI_PASSWORD_LEN)
                res = -CLI_ERR_CFGKEY;
            break;
        }
        case 'f':
        {
            unsigned int i;
            char *next_freq = strchr(token, ',');
            for (i = 0; i < 2; i++) {
                cfg->freq[i] = atoi(token);
                if (!wifi_freq_in_range(cfg->freq[i]))
                {
                    res = CLI_ERR_FREQ;
                    break;
                }
                if (!next_freq)
                    break;
                *next_freq++ = '\0';
                token = next_freq;
                next_freq = strchr(token, ',');
            }
            break;
        }
        case 'a':
        {
            cfg->sec = atoi(token);
            if (cfg->sec > WIFI_SEC_WPA2_PSK_WPA3_SAE)
            {
                res = CLI_ERR_CFGSEC;
            }
            break;
        }
        case 'd':
        {
            cfg->dhcp_mode = atoi(token);
            if (cfg->dhcp_mode != STATIC_IPV4 && cfg->dhcp_mode != DHCP_CLIENT)
            {
                res = CLI_ERR_DHCPMODE;
            }
            break;
        }
        case 'i':
        {
            str_len = strlen(token);
            if (cfg->dhcp_mode == STATIC_IPV4 && str_len <= 16 && str_len > 0)
            {
                memcpy(cfg->ip4_ip, token, str_len);
            }
            if (utils_cli_parse_ip4(token, &ip, &mask))
            {
                res = CLI_ERR_IP;
                break;
            }
            record ++;
            break;
        }
        case 'm':
        {
            str_len = strlen(token);

            if (cfg->dhcp_mode == STATIC_IPV4 && str_len <= 16 && str_len > 0)
            {
                memcpy(cfg->ip4_mask, token, str_len);
            }

            if (utils_cli_parse_ip4(token, &ip, &mask))
            {
                res = CLI_ERR_IP;
                break;
            }
            record ++;
            break;
        }
        case 'g':
        {
            str_len = strlen(token);
            if (cfg->dhcp_mode == STATIC_IPV4 && str_len <= 16 && str_len > 0)
            {
                memcpy(cfg->ip4_gw, token, str_len);
            }

            if (utils_cli_parse_ip4(token, &ip, &mask))
            {
                res = CLI_ERR_IP;
                break;
            }
            record ++;
            break;
        }

        default:
            res = CLI_SHOW_USAGE;
            break;
        }
    }

    if (res || !is_ssid_configured ||
            (cfg->dhcp_mode == STATIC_IPV4 && record < 3)) {
        CLI_LOGE("[%s]: Exec cmd err = %d\n", __func__, res);
        res = CLI_SHOW_USAGE;
    }

    if (is_ssid_configured)
    {
        cfg->scan_method = FAST_SCAN;
        //cfg->rssi_threshold = -76;
    }
    return res;
}

static int wifi_cli_connect(char *params)
{
    int res = CLI_SUCCESS;
    wifi_connect_cfg_t config = {0};
    ls_err_t ret = LS_OK;

    ret = wifi_cli_connet_params(&config, params);
    if (ret)
    {
        CLI_LOGD("params error %d \r\n", ret);
        return ret;
    }

    ret = wifi_sta_connect(&config);
    if (ret)
    {
        CLI_LOGE("CONNECT ERR %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
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
    }

    return res;
}

static int wifi_cli_get_rssi(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    int rssi = 0;

    ret = wifi_get_ap_rssi(&rssi);

    if (ret)
    {
        CLI_LOGE(" %s \r\n", (ret == LS_ERR_WIFI_NOT_CONNECT)? "STA not in connected state" : "unknow");
    }
    else
    {
        CLI_LOG("RSSI %d \r\n", rssi);
    }
    return res;
}

static int wifi_cli_get_link_status(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    wifi_link_status_t link_status = {0};
     struct ip_addr_cfg cfg = {0};

    ret = wifi_get_link_status(&link_status);

    if (ret)
    {
        CLI_LOGE(" %s\r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    }
    else
    {
        switch(link_status.state)
        {
            case STA_INACTIVE:
                CLI_LOG("STA state: INACTIVE \r\n");
                break;
            case STA_IN_CONNECTING:
                CLI_LOG("STA state: CONNECTING\r\n");
                break;
            case STA_CONNECTED:
                CLI_LOG("STA state: CONNECTED\r\n");
                break;
            case STA_DISCONNECTED:
                CLI_LOG("STA state: DISCONNECTED\r\n");
                break;
            default:
                CLI_LOG("Not in STA mode \r\n");
                break;
        }
        if (link_status.is_connected)
        {
            ls_get_ip(WIFI_VIF_DEFAULT_IDX, &cfg);
            CLI_LOG("SSID: %s \r\n", link_status.ssid);
            CLI_LOG("AID: %d, operating channel: %d, RSSI: %d (dbm) \r\n", link_status.aid, link_status.channel, link_status.rssi);
            CLI_LOG("AP BSSID: %02X:%02X:%02X:%02X:%02X:%02X \r\n",
                        link_status.bssid[0], link_status.bssid[1],
                        link_status.bssid[2], link_status.bssid[3],
                        link_status.bssid[4], link_status.bssid[5]);
            CLI_LOG("IP: %d.%d.%d.%d\r\n",
                    cfg.ipv4.addr & 0xff, (cfg.ipv4.addr >> 8) & 0xff,
                    (cfg.ipv4.addr >> 16) & 0xff, (cfg.ipv4.addr >> 24) & 0xff);
            CLI_LOG("mask: %d.%d.%d.%d\r\n",
                    cfg.ipv4.mask & 0xff, (cfg.ipv4.mask >> 8) & 0xff,
                    (cfg.ipv4.mask >> 16) & 0xff, (cfg.ipv4.mask >> 24) & 0xff);
            CLI_LOG("gw: %d.%d.%d.%d\r\n",
                    cfg.ipv4.gw & 0xff, (cfg.ipv4.gw >> 8) & 0xff,
                    (cfg.ipv4.gw >> 16) & 0xff, (cfg.ipv4.gw >> 24) & 0xff);
        }
    }
    return res;
}

static int wifi_cli_get_sta_connected_status(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    int link_status = 0;

    ret = wifi_get_sta_state(&link_status);

    if (ret)
    {
        CLI_LOGE(" %s\r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    }
    else
    {
        CLI_LOG("STA: %s \r\n", link_status? "CONNECTED" : "DISCONNECTED");
    }
    return res;
}

static int wifi_cli_powersave(char *params)
{
    char *ptr, *next = params;
    int res = CLI_SUCCESS;

    if (!(ptr = utils_next_token(&next))) {
        return CLI_SHOW_USAGE;
    }

    if (!strcmp(ptr, "status"))
    {
    }
    else if (!strcmp(ptr, "on"))
    {
        wifi_ps_mode_set(WIFI_PS_DEFAULT_TYPE);
    }
    else if (!strcmp(ptr, "off"))
    {
        wifi_ps_mode_set(WIFI_PS_MODE_OFF);
    }
    else
    {
        return CLI_SHOW_USAGE;
    }

    return res;
}

static int wifi_cli_listen_interval_set(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t listen_itv = 0;
    char *token, *next = params;

    token = utils_next_token(&next);
    listen_itv = atoi(token);

    ret = wifi_sta_set_listen_itv(listen_itv);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
       CLI_LOG("SET listen interval %d \r\n", listen_itv);
    }
    return res;
}
static int wifi_cli_listen_interval_get(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t listen_itv = 0;

    ret = wifi_sta_get_listen_itv(&listen_itv);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
       CLI_LOG("Get listen interval %d \r\n", listen_itv);
    }
    return res;
}

static int wifi_cli_dont_wait_bcmc_set(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t dont_wait = 0;
    char *token, *next = params;

    token = utils_next_token(&next);
    if (token)
    {
        dont_wait = atoi(token);

        ret = wifi_sta_set_dont_wait_bcmc(dont_wait);

        if (ret)
        {
            CLI_LOGE(" %d \r\n", ret);
            res = CLI_ERROR;
        }
        else
        {
           CLI_LOG("SET dont_wait_bcmc %d \r\n", dont_wait);
        }
    }

    return res;
}

static int wifi_cli_dont_wait_bcmc_get(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t dont_wait = 0;

    ret = wifi_sta_get_dont_wait_bcmc(&dont_wait);
    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
       CLI_LOG("Get dont_wait_bcmc %d \r\n", dont_wait);
    }

    return res;
}

static int wifi_cli_keepalive_time_set(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t keep_alive_time = 0;
    char *token, *next = params;

    token = utils_next_token(&next);
    if (atoi(token)<=0 || atoi(token) > 255)
    {
        CLI_LOGE(": keep alive time value (%d) invalid, 0< value <= 255 s \r\n", atoi(token));
        res = CLI_ERROR;
        return res;
    }

    keep_alive_time = atoi(token);
    ret = wifi_sta_keepalive_time_set(keep_alive_time);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
       CLI_LOG("keep alive time %d \r\n", keep_alive_time);
    }
    return res;
}

static int wifi_cli_rate_set(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    int rate = 0;
    char *token, *next = params;

    token = utils_next_token(&next);
    rate = strtol(token, NULL, 16);
    if (rate < 0 || rate == 0x4 || (rate > 0x7 && rate < 0x10) \
            || (rate > 0x17 && rate < 0x20) || (rate > 0x29 && rate < 0x30) \
            || (rate > 0x39 && rate < 0x40) || (rate > 0x49 && rate < 0xff))
    {
        CLI_LOGE(": invalid rate value 0x%x\r\n", rate);
        res = CLI_SHOW_USAGE;
        return res;
    }

    ret = wifi_rate_config(rate);

    if (ret)
    {
        CLI_LOGE(" %s \r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_get_ip_addr(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    struct ip_addr_cfg cfg = {0};

    ls_get_ip(WIFI_VIF_DEFAULT_IDX, &cfg);

    if (ret)
    {
        CLI_LOGE(" get ip fail, ret %x \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
        CLI_LOG("IP = %d.%d.%d.%d\r\n",
                    cfg.ipv4.addr & 0xff, (cfg.ipv4.addr >> 8) & 0xff,
                    (cfg.ipv4.addr >> 16) & 0xff, (cfg.ipv4.addr >> 24) & 0xff);
        CLI_LOG("mask = %d.%d.%d.%d\r\n",
                cfg.ipv4.mask & 0xff, (cfg.ipv4.mask >> 8) & 0xff,
                (cfg.ipv4.mask >> 16) & 0xff, (cfg.ipv4.mask >> 24) & 0xff);
        CLI_LOG("gw = %d.%d.%d.%d\r\n",
                cfg.ipv4.gw & 0xff, (cfg.ipv4.gw >> 8) & 0xff,
                (cfg.ipv4.gw >> 16) & 0xff, (cfg.ipv4.gw >> 24) & 0xff);
    }

    return res;
}

static int wifi_cli_set_mac_addr(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    char *token, *next = params;
    uint8_t mac[6] = {0};

    token = utils_next_token(&next);
    res = utils_parse_mac_addr(token, mac);
    if (res)
    {
         return CLI_ERROR;
    }

    ret = wifi_mac_set(mac);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
        CLI_LOG("set mac address: %02X:%02X:%02X:%02X:%02X:%02X \r\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    return res;
}

static int wifi_cli_get_sta_mac_addr(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t mac[6] = {0};

    ret = wifi_get_sta_mac(mac);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
        CLI_LOG("get sta mode mac address: %02X:%02X:%02X:%02X:%02X:%02X \r\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    return res;
}


static int wifi_cli_get_softap_mac_addr(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t mac[6] = {0};

    ret = wifi_get_ap_mac(mac);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    else
    {
        CLI_LOG("get softAP mode mac address: %02X:%02X:%02X:%02X:%02X:%02X \r\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    return res;
}

static int wifi_cli_stop_ap(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;

    ret = wifi_ap_stop();

    if (ret)
    {
        CLI_LOGE(" %s \r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_start_ap_params(wifi_ap_cfg_params_t *cfg, char *params)
{
    char *token, *next = params;
    int res = LS_OK;
    int security;

    while ((token = utils_next_token(&next)))
    {
        char option;

        if ((token[0] != '-') | (token[2] != '\0'))
        {
            res = CLI_SHOW_USAGE;
            break;
        }

        option = token[1];
        token = utils_next_token(&next);
        if (!token)
        {
            res = CLI_SHOW_USAGE;
            break;
        }

        switch (option)
        {
            case 's':
            {
                size_t ssid_len;
                
                ssid_len = utils_get_proper_ssid_psk(token, cfg->ssid, WIFI_SSID_LEN);
                if (!ssid_len || (ssid_len > WIFI_SSID_LEN))
                {
                    res = -CLI_ERR_CFGSSID;
                    break;
                }
                break;
            }
            case 'k':
            {
                size_t key_len;

                key_len = utils_get_proper_ssid_psk(token, (uint8_t *)cfg->pwd, WIFI_PASSWORD_LEN);
                if ((key_len > WIFI_PASSWORD_LEN) || (key_len > 0 && key_len < 8))
                    res = -CLI_ERR_CFGKEY;
                break;
            }
            case 'c':
            {
                if (token)
                {
                    cfg->channel = atoi(token);
                }
                break;
            }
            case 'a':
            {
                if (wifi_str_to_sec(token, &cfg->sec))
                {
                    res = CLI_SHOW_USAGE;
                }
                break;
            }
            default:
            {
                res = CLI_SHOW_USAGE;
                break;
            }
        }
    }

    if (res) {
        CLI_LOGE("[%s]: Exec cmd err = %d\n", __func__, res);
        res = CLI_SHOW_USAGE;
    }

    return res;

}

static int wifi_cli_start_ap(char *params)
{
    wifi_ap_cfg_params_t cfg = {0};
    int res = CLI_SUCCESS;
    ls_err_t ret;

    res = wifi_cli_start_ap_params(&cfg, params);
    if(res)
    {
        return res;
    }

    ret = wifi_ap_start(&cfg);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_get_ap_state(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    int ap_state;

    ret = wifi_get_ap_state(&ap_state);

    if (ret)
    {
        CLI_LOGE(" %s \r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    }
    else
    {
        CLI_LOG("SoftAP %s \r\n", (ap_state)? "ON": "OFF");
    }
    return res;
}

static int wifi_cli_get_ap_info(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    wifi_ap_info_t ap_info = {0};

    ret = wifi_ap_get_basic_info(&ap_info);

    if (ret)
    {
        CLI_LOGE(" %s \r\n", wifi_err_to_str(ret));
        res = CLI_ERROR;
    }
    else
    {
        CLI_LOG("SoftAP info:ssid[%s],pwd[%s]," MACSTR ",chan %d,security %s",
                      ap_info.ssid, ap_info.pwd, MAC2STR(ap_info.bssid), ap_info.channel, wifi_sec_to_str(ap_info.security));
    }
    return res;
}

static int wifi_cli_ap_get_sta_info(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    wifi_sta_basic_info_t sta_info = {0};
    uint8_t i = 0;

    for (i = 0; i < CFG_STA_MAX; i++)
    {
        ret = wifi_ap_get_sta_info(&sta_info, i);

        if (!sta_info.is_used) {
            continue;
        }
        CLI_LOG("STA ID %u       "
            "%02X:%02X:%02X:%02X:%02X:%02X    "
            "AID %d      "
            "\r\n",
            sta_info.sta_idx,
            sta_info.sta_mac[0], sta_info.sta_mac[1], sta_info.sta_mac[2],
            sta_info.sta_mac[3], sta_info.sta_mac[4], sta_info.sta_mac[5],
            sta_info.aid
        );
    }

    return res;
}

static int wifi_cli_ap_deauth_sta(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t sta_idx = 0;
    char *token, *next = params;

    token = utils_next_token(&next);
    sta_idx = atoi(token);

    ret = wifi_ap_sta_delete(sta_idx);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_ap_config_max_sta_num(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t num = 0;
    char *token, *next = params;

    token = utils_next_token(&next);
    if (!token)
    {
        res = CLI_SHOW_USAGE;
        return res;
    }

    num = atoi(token);

    ret = wifi_ap_max_sta_num(num);

    if (ret)
    {
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_sta_vendor_ie_add(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    char ie[48] = {221, 46, 1};

    ret = wifi_sta_set_vendor_ie(ie, 48);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_sta_vendor_ie_del(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;

    ret = wifi_sta_del_vendor_ie();

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_ap_vendor_ie_add(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    char ie[48] = {221, 46, 1};

    ret = wifi_ap_set_vendor_ie(ie, 48);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_ap_vendor_ie_del(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;

    ret = wifi_ap_del_vendor_ie();

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_send_80211_data(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    wifi_80211_tx_info_t tx_info;
    uint8_t pkt[] = {0x88, 0x42, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x26, 0x7f, 0x25, 0x00, 0x05,
        0x2a, 0xa7, 0x8a, 0x51, 0x71, 0x39,
        0x20, 0x00,
        0x00, 0x00,
        0x03, 0x00, 0x00, 0x60, 0x00, 0x00};

    tx_info.pkt = rtos_aligned_malloc(128, HAL_DCACHE_CFG_LINE_SIZE);
    if (!tx_info.pkt)
    {
        return CLI_ERROR;
    }
    tx_info.len = 128;
    tx_info.wifi_vif_idx = WIFI_VIF_DEFAULT_IDX;
    memcpy(tx_info.pkt, pkt, tx_info.len);

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if ((tx_info.pkt >= PSRAM_BASE_ADDRESS) && DCachePresent())
    {
        vPortEnterCritical();
        HAL_FlushDCache_by_Addr((uint32_t*)tx_info.pkt, tx_info.len);
        vPortExitCritical();
    }
#endif

    ret = wifi_send_80211_frame(&tx_info);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }

    rtos_aligned_free(tx_info.pkt);

    return res;
}

static uint16_t chan_to_freq(uint16_t chan)
{
    if (chan >= 1 && chan <= 13)
        return 2407 + 5 * chan;
    if (chan == 14)
        return 2484;
}

static void cli_print_mac(const char *prefix, uint8_t *addr)
{
    CLI_LOG("\t%s: %02x-%02x-%2x-%02x-%2x-%02x\n", prefix,
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void monitor_cb(wifi_sniffer_frame_info_t *info, void *cb_arg)
{
    static int i = 0;

    if (info->payload)
    {
       struct wifi_mac_hdr *hdr = (struct wifi_mac_hdr *)info->payload;
       CLI_LOG("Caputured a frame: len %d ,freq %d \r\n", info->length, info->freq);
       if (info->length > sizeof(struct wifi_mac_hdr))
       {
           CLI_LOG("\tFC: 0x%x, Seq: %u, len: %u\n",
               hdr->fctl, hdr->seq, info->length);
           cli_print_mac("addr1", hdr->addr1);
           cli_print_mac("addr2", hdr->addr2);
           cli_print_mac("addr3", hdr->addr3);
       }
       i++;
    }
    if (i % 30 == 0)
    {
        CLI_LOG("Caputured  frame count %d", i);
    }
}

static int wifi_cli_sniffer_enable(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    wifi_sniffer_para_t sniffer_item = {0};
    char *token, *next = params;
    uint16_t chan = {0};

    token = utils_next_token(&next);
    chan = atoi(token);

    sniffer_item.itf_idx = WIFI_VIF_SNIFFER_IDX;
    sniffer_item.prim20_freq = chan_to_freq(chan);
    sniffer_item.cb = monitor_cb;
    ret = wifi_sniffer_enable(&sniffer_item);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    return res;
}

static int wifi_cli_sniffer_disable(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    wifi_sniffer_para_t sniffer_item = {0};


    sniffer_item.itf_idx = WIFI_VIF_SNIFFER_IDX;

    ret = wifi_sniffer_disable(&sniffer_item);

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }
    return res;
}

static int wifi_cli_twt_setup(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    uint8_t setup_type, min_twt;
    uint16_t mantissa;
    char *token, *next = params;


    while ((token = utils_next_token(&next)))
    {
        if (!strncmp(token, "-h", 2))
        {
            return CLI_SHOW_USAGE;
        }
        else if (!strncmp(token, "-s", 2))
            setup_type = 1;
        else if (!strncmp(token, "-d", 2))
            setup_type = 2;
        else if (!strncmp(token, "intv=", 5))
        {
            mantissa = atoi(&token[5]);
        }
        else if (!strncmp(token, "wake=", 5))
        {
            if (atoi(&token[5]) > 255)
            {
                 res = CLI_ERR_WAKE_DUR;
                 break;
            }
            min_twt = atoi(&token[5]);
        }
    }

    if (res)
    {
        CLI_LOGE("ERR %d \r\n", ret);
        res = CLI_ERROR;
        return res;
    }

    ret = wifi_twt_setup(setup_type, mantissa, min_twt);

    if (ret)
    {
        CLI_LOGE("ERR %d \r\n", ret);
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_twt_teardown(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;

    ret = wifi_twt_teardown();

    if (ret)
    {
        CLI_LOGE(" %d \r\n", ret);
        res = CLI_ERROR;
    }

    return res;
}

static int wifi_cli_pwr_tbl_set(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret = LS_OK;
    struct pwr_table pwr={0};
    uint8_t type = 0, i=0, b_cnt, g_cnt, n_cnt, ax_cnt;
    char *token, *next = params, *p = NULL;

    while ((token = utils_next_token(&next))) {

        if (!strncmp(token, "type=", 5))
        {
            type = atoi(token+5);
            if (type > 3 || type < 0)
            {
                CLI_LOGE("Invlid Type= %d", type);
                res = CLI_ERROR;
            }
        }
        else if (!strncmp(token, "11b=", 4))
        {
            p = strchr(token, ',');
            token +=4;
            for (i = 0; i < 4; i++) {
                pwr.pwr_11b[i] = atoi(token);
                if (pwr.pwr_11b[i] > 21)
                {
                    CLI_LOGE(" 11b max power should not exceed 21 dbm\n");
                    res = CLI_ERROR;
                }
                b_cnt ++;
                if (!p)
                    break;
                *p++ = '\0';
                token = p;
                p = strchr(token, ',');
            }
        }
        else if (!strncmp(token, "11g=", 4))
        {
            p = strchr(token, ',');
            token +=4;
            for (i = 0; i < 8; i++) {
                pwr.pwr_11g[i] = atoi(token);
                if (pwr.pwr_11g[i] > 20)
                {
                    CLI_LOGE(" 11g power should not exceed 20 dbm\n");
                    res = CLI_ERROR;
                }
                g_cnt ++;

                if (!p)
                    break;
                *p++ = '\0';
                token = p;
                p = strchr(token, ',');
            }
        }
        else if (!strncmp(token, "11n=", 4))
        {
            p = strchr(token, ',');
            token +=4;
            for (i = 0; i < 8; i++) {
                pwr.pwr_11n_ht20[i] = atoi(token);
                if (pwr.pwr_11n_ht20[i] > 19)
                {
                    CLI_LOGE(" 11n max power should not exceed 19 dbm\n");
                    res = CLI_ERROR;
                }
                n_cnt ++;

                if (!p)
                    break;
                *p++ = '\0';
                token = p;
                p = strchr(token, ',');
            }
        }
        else if (!strncmp(token, "11ax=", 5))
        {
            p = strchr(token, ',');
            token +=5;
            for (i = 0; i < 10; i++) {
                pwr.pwr_11ax_he20[i] = atoi(token);
                if (pwr.pwr_11ax_he20[i] > 19)
                {
                    CLI_LOGE(" 11ax max power should not exceed 19 dbm\n");
                    res = CLI_ERROR;
                }
                ax_cnt ++;
                if (!p)
                    break;
                *p++ = '\0';
                token = p;
                p = strchr(token, ',');
            }
            break;
        }

    }
    CLI_LOG("11b pwr:%d %d %d %d", pwr.pwr_11b[0],pwr.pwr_11b[1],pwr.pwr_11b[2],pwr.pwr_11b[3]);
    CLI_LOG("11g pwr: %d %d %d %d %d %d %d %d", pwr.pwr_11g[0],pwr.pwr_11g[1],pwr.pwr_11g[2],pwr.pwr_11g[3],pwr.pwr_11g[4],pwr.pwr_11g[5],pwr.pwr_11g[6],pwr.pwr_11g[7]);
    CLI_LOG("11n pwr: %d %d %d %d %d %d %d %d ", pwr.pwr_11n_ht20[0],pwr.pwr_11n_ht20[1],pwr.pwr_11n_ht20[2],pwr.pwr_11n_ht20[3],pwr.pwr_11n_ht20[4],pwr.pwr_11n_ht20[5],pwr.pwr_11n_ht20[6],pwr.pwr_11n_ht20[7]);
    CLI_LOG("11ax pwr: %d %d %d %d %d %d %d %d %d %d", pwr.pwr_11ax_he20[0], pwr.pwr_11ax_he20[1],pwr.pwr_11ax_he20[2],pwr.pwr_11ax_he20[3],pwr.pwr_11ax_he20[4],pwr.pwr_11ax_he20[5],pwr.pwr_11ax_he20[6],pwr.pwr_11ax_he20[7],pwr.pwr_11ax_he20[8],pwr.pwr_11ax_he20[9]);

    if (res || g_cnt!=8 || b_cnt!=4 || n_cnt!=8 || ax_cnt!=10)
    {
        CLI_LOGE(" %d 11b cnt %d 11g cnt %d 11n cnt %d 11ax cnt %d \r\n", res, b_cnt, g_cnt, n_cnt, ax_cnt);
    }
    ret = wifi_set_max_tx_pwr(&pwr, type);
    // get power table to verify
    memset(&pwr, 0, sizeof(struct pwr_table));
    wifi_get_max_tx_pwr(&pwr, type);
    CLI_LOG("Get back tx power table");
    CLI_LOG("11b pwr:%d %d %d %d", pwr.pwr_11b[0],pwr.pwr_11b[1],pwr.pwr_11b[2],pwr.pwr_11b[3]);
    CLI_LOG("11g pwr: %d %d %d %d %d %d %d %d", pwr.pwr_11g[0],pwr.pwr_11g[1],pwr.pwr_11g[2],pwr.pwr_11g[3],pwr.pwr_11g[4],pwr.pwr_11g[5],pwr.pwr_11g[6],pwr.pwr_11g[7]);
    CLI_LOG("11n pwr: %d %d %d %d %d %d %d %d ", pwr.pwr_11n_ht20[0],pwr.pwr_11n_ht20[1],pwr.pwr_11n_ht20[2],pwr.pwr_11n_ht20[3],pwr.pwr_11n_ht20[4],pwr.pwr_11n_ht20[5],pwr.pwr_11n_ht20[6],pwr.pwr_11n_ht20[7]);
    CLI_LOG("11ax pwr: %d %d %d %d %d %d %d %d %d %d", pwr.pwr_11ax_he20[0], pwr.pwr_11ax_he20[1],pwr.pwr_11ax_he20[2],pwr.pwr_11ax_he20[3],pwr.pwr_11ax_he20[4],pwr.pwr_11ax_he20[5],pwr.pwr_11ax_he20[6],pwr.pwr_11ax_he20[7],pwr.pwr_11ax_he20[8],pwr.pwr_11ax_he20[9]);

    if (ret)
    {
       res = CLI_ERROR;
    }
    return res;
}

static int wifi_cli_dbg_level_set(char *params)
{
    int res = CLI_SUCCESS;
    ls_err_t ret;
    char *token, *next = params;
    uint16_t fw_filter_module = 0xffff;
    uint8_t fw_filter_severity = 3, wpa_dbg_level = 4;  // default level in firmware

    while ((token = utils_next_token(&next)))
    {
        if (!strncmp(token, "-h", 2))
        {
            return CLI_SHOW_USAGE;
        }
        else if (!strncmp(token, "wpa=", 4))
        {
            wpa_dbg_level = strtol(&token[4], NULL, 16);//atoi(&token[4]);
            if (wpa_dbg_level < 0 || wpa_dbg_level > 5)
            {
                res = CLI_SHOW_USAGE;
            }
        }
        else if (!strncmp(token, "fw_mod=", 7))
        {
            fw_filter_module = strtol(&token[7], NULL, 16);//atoi(&token[7]);
            if (fw_filter_module & 0xf800)
            {
                res = CLI_SHOW_USAGE;
            }
        }
        else if (!strncmp(token, "fw_level=", 9))
        {
            fw_filter_severity = strtol(&token[9], NULL, 16);//atoi(&token[9]);
            if (fw_filter_severity < 0 || fw_filter_severity > 5)
            {
                res = CLI_SHOW_USAGE;
            }
        }
    }
    if (res)
    {
        CLI_LOGE(": invalid dbg value setting \r\n");
        return res;
    }
    CLI_LOGI("  FW filter module 0x%x fw filter severity %d WPA dbg level %d",fw_filter_module, fw_filter_severity, wpa_dbg_level);

    ret = wifi_dbg_level_set(fw_filter_module, fw_filter_severity, wpa_dbg_level);

    if (ret)
    {
        res = CLI_ERROR;
    }

    return res;
}

int wifi_cli_exec_sta_auto_conn(void)
{
#if CFG_NVS
    uint8_t sta_auto_conn_en = 0;
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
            sta_config.scan_method = FAST_SCAN;
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

static int wifi_cli_autoconn(char *params)
{
    uint8_t auto_conn_en = 0;
    char *token, *next = params;

    token = utils_next_token(&next);
    if (token == NULL)
    {
        return CLI_SHOW_USAGE;
    }
    auto_conn_en = atoi(token);
#if CFG_NVS
    nvds_put(NVDS_TAG_WIFI_STA_AUTOCONN, NVDS_LEN_WIFI_STA_AUTOCONN, &auto_conn_en);
#endif

    return CLI_SUCCESS;
}

static int wifi_cli_wifi_on(char *params)
{

    wifi_on();

    return CLI_SUCCESS;
}

static int wifi_cli_wifi_off(char *params)
{

    wifi_off();

    return CLI_SUCCESS;
}

#if WIFI_OTA
static int wifi_cli_ota(char *params)
{
    char *token, *next = params;
    uint8_t file_name[32] = {0};
    uint8_t ip_str[16] = {0};
    uint16_t port;
    int i;

    while ((token = utils_next_token(&next))) {
        if (token[0] != '-')
            return CLI_SHOW_USAGE;

        switch (token[1]) {
            case ('p'):
                token = fhost_cli_next_token(&next);
                if (!token)
                    return CLI_SHOW_USAGE;
                port = atoi(token);
                break;
            case ('i'):
                token = fhost_cli_next_token(&next);
                if (!token)
                    return CLI_SHOW_USAGE;
                memcpy(ip_str, token, strlen(token));
                break;
            case ('f'):
                token = fhost_cli_next_token(&next);
                if (!token || (strlen(token) >= sizeof(file_name)))
                    return CLI_SHOW_USAGE;
                memcpy(file_name, token, strlen(token));
                break;
            case ('a'):
                /* Not yet, for flash addr*/
                break;
            default:
                return CLI_SHOW_USAGE;
      }
    }

    wifi_ota_start(file_name, ip_str, 0, "Listenai");

    return CLI_SUCCESS;
}
#endif

static int wifi_cli_help(char *params)
{
    uint8_t i = 0;

    for (; cli_wifi_commands[i].exec != NULL; i++)
    {
        CLI_LOG(" - %s %s\r\n", cli_wifi_commands[i].name, cli_wifi_commands[i].params);
    }

    return CLI_SUCCESS;
}

/// Array of supported CLI command
static const struct cli_cmd cli_wifi_commands[] =
{
#ifndef WIFI_RAM_ATE
    {wifi_cli_help, "wifi?", ""},
    {wifi_cli_set_country_code, "wifi_set_country", "US/EU/CN/JP"},
    {wifi_cli_get_country_code, "wifi_get_country", ""},
    {wifi_cli_get_channel, "wifi_get_chan", ":get current operating channel"},
    {wifi_cli_scan, "wifi_scan", "[-s <ssid>] [-b <bssid>] [-c <channel1,channel2,>] [-d <duration in ms, max 150 ms>]\r\n"
     "            example: wifi_scan -c 1,2,5,6,11"},
    {wifi_cli_get_scan_results, "wifi_scan_results", ":show scan results"},
    {wifi_cli_disconnect, "wifi_disconnect", ""},
    {wifi_cli_connect, "wifi_connect", "-s <ssid> [-k <pwd>]"
     "[-f <freq>[,freq]] [-b <bssid>]"
     "[-d <0:dhcp client mode/1:static ip mode>] [-i <static ip>] [-m <static ip mask>] [-g <static ip gateway>] \r\n"
     "            static ip: -d 1 -i 192.168.1.100 -m 255.255.255.0 -g 192.168.1.1 \r\n"
     "            example: wifi_connect -s test -k 12345678 \r\n"
    },
    {wifi_cli_rate_set, "wifi_set_rate", "<rate vaule>\r\n"
    "            refer rate define in wifi_phy_rate_e \r\n"
    "            auto rate 0xff \r\n"
    "            11B 1Mbps ~ 11Mbps long preamble : 0x0 ~ 0x3 \r\n"
    "            11B 2Mbps ~ 11Mbps short preamble : 0x5 ~ 0x7 \r\n"
    "            11G 6Mbps ~ 54Mbps  : 0x10 ~ 0x17 \r\n"
    "            MCS0 ~ MCS9 (Long GI or 11ax 1.6 us GI) : 0x20 ~ 0x29\r\n"
    "            MCS0 ~ MCS9 (Short GI or 11ax 0.8 us GI) : 0x30 ~ 0x39\r\n"
    "            MCS0 ~ MCS9 ( 11ax 3.2 us GI) : 0x40 ~ 0x49\r\n"
    },
    {wifi_cli_get_ip_addr, "wifi_ip", ":get sta or softAP IP address" },
    {wifi_cli_get_rssi, "wifi_rssi", ": get sta mode rssi strength in connection state"},
    {wifi_cli_get_link_status, "wifi_link_status", ": get sta link status"},
    {wifi_cli_get_sta_connected_status, "wifi_sta_state", ": check wifi connected or not"},
     {wifi_cli_powersave, "wifi_powersave", "[-m <keep alive or not:0|1>]\n"
                                        "                  off\n"
                                        "                  status"},
    {wifi_cli_listen_interval_set, "wifi_set_listen_interval", "<value>" " max value should less than 20"},
    {wifi_cli_listen_interval_get, "wifi_get_listen_interval", ""},
    {wifi_cli_dont_wait_bcmc_set, "wifi_set_dont_wait_bcmc", "<enable>"},
    {wifi_cli_dont_wait_bcmc_get, "wifi_get_dont_wait_bcmc", ""},
    {wifi_cli_keepalive_time_set, "wifi_keepalive", "<value, max value 255, unit:seconds>"},
    {wifi_cli_set_mac_addr, "wifi_set_mac", "<mac address xx:xx:xx:xx:xx:xx>"},
    {wifi_cli_get_sta_mac_addr, "wifi_sta_mac", ": get sta mac address"},
    {wifi_cli_get_softap_mac_addr, "wifi_softap_mac", ": get ap interface mac address"},
    {wifi_cli_stop_ap, "wifi_stop_ap", ""},
    {wifi_cli_start_ap, "wifi_start_ap", "-s <ssid> [-k <pwd>] [-a <security mode>]"
     "[-c <channel 1 ~ 13> ]\r\n"
     "            security mode: OPEN/WPA/WPA2/WPA3, if not set, can determine automatically"},
    {wifi_cli_get_ap_state, "wifi_ap_state", ":check SoftAp started success or not"},
    {wifi_cli_get_ap_info, "wifi_ap_info", ":show AP basic info"},
    {wifi_cli_ap_get_sta_info, "wifi_ap_sta_list", ": show connected sta list"},
    {wifi_cli_ap_deauth_sta, "wifi_deauth_sta", "<sta_id>"},
    {wifi_cli_ap_config_max_sta_num, "wifi_ap_max_sta", "<config max sta number>  should set max sta num before AP start"},
    {wifi_cli_sta_vendor_ie_add, "wifi_sta_vsie_add", ""},
    {wifi_cli_sta_vendor_ie_del, "wifi_sta_vsie_del", ""},
    {wifi_cli_ap_vendor_ie_add, "wifi_ap_vsie_add", ""},
    {wifi_cli_ap_vendor_ie_del, "wifi_ap_vsie_del", ""},
    {wifi_cli_send_80211_data, "wifi_send_packet", ""},
    {wifi_cli_sniffer_enable, "wifi_sniffer_enable", " <channel 1~13>"},
    {wifi_cli_sniffer_disable, "wifi_sniffer_disable", ""},
    {wifi_cli_twt_setup, "wifi_twt_setup", "-d or -s (setup type :demand suggest) "
    "intv=<val> (wake interval unit:ms)  wake=<val> (wake duration, unit: ms, max 255)"},
    {wifi_cli_twt_teardown, "wifi_twt_teardown", ""},
    {wifi_cli_pwr_tbl_set, "wifi_pwr_set", "type=<val> 11b=<p1,p2,p3,p4> 11g=<p1,p2,p3,p4,p5,p6,p7,p8> 11n=<p1,p2,p3,p4,p5,p6,p7,p8> 11ax=<p1,p2,p3,p4,p5,p6,p7,p8,p9,p10>\n"
     "type: 0 for all channel, 1 for low channel 1, 2 for middle channel 2~10, 3 for high channel 11~13\r\n"
     "wifi_pwr_set type=0 11b=19,19,19,19 11g=17,17,17,17,17,17,16,16 11n=16,16,16,16,15,15,14,14 11ax=16,16,16,16,16,16,15,15,14,14\r\n"
    },
    {wifi_cli_dbg_level_set, "wifi_dbg", "fw_level=<val> fw_mod=<val> wpa=<val>\r\n"
     "            fw_level 0~5: none/CRT/ERR/WAR/INFO/VRB \r\n"
     "            fw module (HEX) BIT0 ~ BIT10: KE/DBG/IPC/DMA/MM/TX/RX/PHY/SM/FHOST/ME \r\n"
     "            wpa level 0~5: MSG_EXCESSIVE/MSG_MSGDUMP/MSG_DEBUG/MSG_INFO/MSG_WARNING/MSG_ERROR \r\n"
    },
    {wifi_cli_autoconn, "wifi_autoconn", "<enable>\n"
     "            enable : 1 means enable sta mode auto connect after reboot, 0 means disable sta mode auto connect after reboot\n"},
    {wifi_cli_wifi_on, "wifi_on", "restart wifi \r\n"},
    {wifi_cli_wifi_off, "wifi_off", "turn off wifi \r\n"},

#if WIFI_OTA
    {wifi_cli_ota, "wifi_ota", "-p <port> | -i <ip_addr> | -f <fw>\r\n"
     "            fw: target fw name, should be less than 32 bytes\r\n"},
#endif
#else
    {wifi_cli_help, "wifi?", ""},
    {wifi_cli_set_mac_addr, "wifi_set_mac", "<mac address xx:xx:xx:xx:xx:xx>"},
    {wifi_cli_get_sta_mac_addr, "wifi_sta_mac", ": get sta mac address"},
#endif
    {NULL, "", ""}
};

uint32_t wifi_cmd_handler(char* command, int len)
{
    uint32_t res;
    char *param;
    const struct cli_cmd *cmd;

    param = strchr(command, ' ');
    if (param)
    {
        *param++ = '\0';
        while (*param == ' ')
            param++;
    }
    else
    {
        /* be sure to have \0 in command */
        command[len - 1] = '\0';
    }

    cmd = cli_wifi_commands;
    while (cmd->exec)
    {
        if (!strcmp(command, cmd->name))
            break;
        cmd++;
    }

    if (cmd->exec)
    {
        res = (uint32_t)cmd->exec(param);
        /* Add default response */
        if (res == CLI_SHOW_USAGE)
        {
            CLI_LOG("Usage:\n%s %s\r\n",
                        cmd->name, cmd->params);
        }
    }
    else
    {
        res = CLI_UNKNOWN_CMD;
    }

    return res;
}

/**
 * @}
 */
