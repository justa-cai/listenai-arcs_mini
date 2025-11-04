/**
 ****************************************************************************************
 *
 * @file cli_bt.c
 *
 * @brief bt cli cmd implementation
 *
 * Copyright (C) ListenAI 2024-2099
 *
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup CLI CMD BT
 
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "cli_main.h"

#include "bt_api.h"

#include "ls_event.h"

#include "atcmd_bt_if.h"


/*
 * EXPORTED FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

typedef enum
{
    TYPE_UINT8,
    TYPE_UINT16,
    TYPE_UINT24,
    TYPE_UINT32,
    TYPE_STRING,
}param_type_t;

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
static const struct cli_cmd cli_bt_commands[];

#if BT_WIFI_COEX
extern void lsip_reset(void);
extern void hci_event_notify_reg(void *notify);
#endif

typedef struct {
    uint8_t (*func)(char *params);
    const char *name;
    const char *help;
} command_entry_t;

int bt_parse_mac_addr(const char *mac_str, uint8_t *addr)
{   int n = sscanf(mac_str, "%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x", &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
    //CLOGI("n=%d", n);
    return  n!= 6;
}

uint8_t parse_params(char *params, const char *keys[], const uint8_t types[], const char *fmts[], void *values[], int count)
{
    if (!params) return CLI_SUCCESS;

        char *ptr = params;
        char *val_start;
        uint32_t seen = 0; //
        uint32_t type = 0;
        while (ptr && *ptr)
        {
            int i;
            for (i = 0; i < count; i++)
            {
                size_t key_len = strlen(keys[i]);
                //CLOGI("key_len:%d", key_len);
                //CLOGI("keys[i]:%s", keys[i]);
                //CLOGI("ptr start:%s", ptr);
                if (strncmp(ptr, keys[i], key_len) == 0)
                {
                    if (seen & (1 << i))
                    {
                        return CLI_INVALID_PARAM; //
                    }
                    val_start = ptr + key_len;
                    while (*val_start == ' ')
                    {
                        val_start++; // 跳过空格
                    }
                    if (*val_start == '\0')
                    {
                        return CLI_ERROR; //
                    }
                    //CLOGI("val_start:%s", val_start);
                    //CLOGI("fmts[i]:%s", fmts[i]);
                    //type = types[i];
                    switch(types[i])
                    {
                        case TYPE_UINT8:
                        {
                            unsigned int temp;
                            if (sscanf(val_start, fmts[i], &temp) != 1)
                            {
                                return CLI_ERROR;
                            }
                            *(uint8_t*)values[i] = (uint8_t)temp;
                            break;
                        }
                        case TYPE_UINT16:
                        {
                            unsigned int temp;
                            if (sscanf(val_start, fmts[i], &temp) != 1)
                            {
                                return CLI_ERROR;
                            }
                            *(uint16_t*)values[i] = (uint16_t)temp;
                            break;
                        }
                        case TYPE_UINT24:
                        {
                            unsigned int temp;
                            if (sscanf(val_start, fmts[i], &temp) != 1)
                            {
                                return CLI_ERROR;
                            }
                            *(uint32_t*)values[i] = (temp | (*(uint32_t*)values[i]));
                            break;
                        }
                        case TYPE_UINT32:
                        {
                            unsigned int temp;
                            if (sscanf(val_start, fmts[i], &temp) != 1)
                            {
                                return CLI_ERROR;
                            }
                            *(uint32_t*)values[i] = temp;
                            break;
                        }
                        case TYPE_STRING:
                        {
                            if (sscanf(val_start, fmts[i], (char*)values[i]) != 1)
                            {
                                return CLI_ERROR;
                            }
                            break;
                        }
                        default:
                            return CLI_ERROR;
                    }
                    //CLOGI("values[i]:0x%x", *(uint32_t*)values[i]);
                    seen |= (1 << i);
                    break;
                }
            }
            if (i == count)
            {
                return CLI_ERROR; //
            }
            ptr = val_start + strcspn(val_start, " ");
            while (*ptr == ' ') ptr++;
            //CLOGI("ptr end:%s", ptr);
        }
        return CLI_SUCCESS;

}


void bt_not_support(void)
{
    CLOGI("bt_not_support, open BT_WIFI_COEX");
}
void ble_not_support(void)
{
    CLOGI("ble_not_support, open BT_WIFI_COEX");
}

/* BLE Commands */
int ble_init(char *params)
{
    int init = 1;
    const char *keys[] = {"-i"};
    const char *fmts[] = {"%i"};
    const uint8_t types[] = {TYPE_UINT32};
    void *values[] = {&init};
    uint8_t result = parse_params(params, keys, types, fmts, values, 1);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEINIT: init=%d", init);
#ifdef BT_WIFI_COEX
    atcmd_ble_init_send(init);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_name(char *params)
{
    char name[BD_NAME_SIZE + 1] = "ARCSD_CMCC";
    const char *keys[] = {"-n"};
    const char *fmts[] = {"%31s"};
    const uint8_t types[] = {TYPE_STRING};
    void *values[] = {name};
    uint8_t result = parse_params(params, keys, types, fmts, values, 1);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLENAME: name=%s", name);
#ifdef BT_WIFI_COEX
    atcmd_blename_send(name);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_scan_param(char *params)
{
    ble_scan_params_t config = {.scan_type = 1, .scan_filt_policy = 1, .own_addr_type=0, .scan_intv = 100, .scan_window = 50};
    const char *keys[] = {"-t", "-f", "-i", "-w"};
    const char *fmts[] = {"%i", "%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT8, TYPE_UINT16, TYPE_UINT16};
    void *values[] = {&config.scan_type, &config.scan_filt_policy, &config.scan_intv, &config.scan_window};
    uint8_t result = parse_params(params, keys, types, fmts, values, 4);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLESCANPARAM: scan_type=%d, scan_filt_policy=%d, scan_intv=%d, scan_window=%d",
          config.scan_type, config.scan_filt_policy, config.scan_intv, config.scan_window);
#ifdef BT_WIFI_COEX
    atcmd_ble_scan_param_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_scan(char *params)
{
    ble_scan_t config = {.enable = 0, .intv = 0, .filter_type = 1}; //
    char mac_str[18] = {0};
    const char *keys[] = {"-e", "-i", "-f", "-m"};
    const char *fmts[] = {"%i", "%i", "%i", "%17s"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT16, TYPE_UINT16, TYPE_STRING};
    void *values[] = {&config.enable, &config.intv, &config.filter_type, mac_str};
    uint8_t result = parse_params(params, keys, types, fmts, values, 4);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLESCAN: enable=%d, intv=%u, filter_type=%d, mac=%s",
          config.enable, config.intv, config.filter_type, mac_str[0] ? mac_str : "none");
    if (mac_str[0] && (config.filter_type == 1) && bt_parse_mac_addr(mac_str, config.filter_param))
    {
        //CLOGI("CLI_INVALID_PARAM");
        return CLI_INVALID_PARAM;
    }

#ifdef BT_WIFI_COEX
    if (config.filter_type == 1 && mac_str[0])
    {
        extern struct out_bd_addr ble_scan_filter_bd_addr;
        CLOGI("MAC %02x:%02x:%02x:%02x:%02x:%02x\n",config.filter_param[0], config.filter_param[1],config.filter_param[2],config.filter_param[3],config.filter_param[4],config.filter_param[5]);
        memcpy(&ble_scan_filter_bd_addr.addr[0], config.filter_param, BD_ADDR_LEN);
    }
    atcmd_ble_scan_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_scan_rsp_data(char *params)
{
    ble_scan_rspdata_t config = {.type = 1, .data_len = 10};
    char data_str[SCAN_RSP_DATA_LEN + 1] = "default_data";
    const char *keys[] = {"-t", "-l", "-d"};
    const char *fmts[] = {"%i", "%i", "%31s"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT8, TYPE_STRING};
    void *values[] = {&config.type, &config.data_len, data_str};
    uint8_t result = parse_params(params, keys, types, fmts, values, 3);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLESCANRSPDATA: type=%d, data_len=%d, data=%s",
          config.type, config.data_len, data_str);
    memcpy(config.data.data, data_str, SCAN_RSP_DATA_LEN);
#ifdef BT_WIFI_COEX
    atcmd_ble_scan_rsp_data_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_adv_param(char *params)
{
    ble_adv_param_t config = {.adv_type = 1, .adv_mode = 1, .adv_int_min = 0x0100, .adv_int_max = 0x0200}; // 默认非0
    const char *keys[] = {"-t", "-m", "-i", "-x"};
    const char *fmts[] = {"%i", "%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT8, TYPE_UINT16, TYPE_UINT16};
    void *values[] = {&config.adv_type, &config.adv_mode, &config.adv_int_min, &config.adv_int_max};
    uint8_t result = parse_params(params, keys, types, fmts, values, 4);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEADVPARAM: adv_type=%d, adv_mode=%d, adv_int_min=%u, adv_int_max=%u",
          config.adv_type, config.adv_mode, config.adv_int_min, config.adv_int_max);
    if (config.adv_type > 2 || config.adv_mode > 2 ||
        config.adv_int_min < ADV_INTERVAL_MIN || config.adv_int_min > ADV_INTERVAL_MAX ||
        config.adv_int_max < config.adv_int_min || config.adv_int_max > ADV_INTERVAL_MAX)
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_ble_adv_param_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_adv_data(char *params)
{
    ble_adv_data_t config = {.adv_type = 1, .data_len = 10};
    char data_str[ADV_DATA_LEN + 1] = "adv_default";
    const char *keys[] = {"-t", "-l", "-d"};
    const char *fmts[] = {"%i", "%i", "%31s"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT16, TYPE_STRING};
    void *values[] = {&config.adv_type, &config.data_len, data_str};
    uint8_t result = parse_params(params, keys, types, fmts, values, 3);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEADVDATA: adv_type=%d, data_len=%d, data=%s",
          config.adv_type, config.data_len, data_str);
    if (config.adv_type > 2)
    {
        return CLI_INVALID_PARAM;
    }
    memcpy(config.data.data, data_str, ADV_DATA_LEN);
#ifdef BT_WIFI_COEX
    atcmd_ble_adv_data_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_adv_start(char *params)
{
    ble_adv_en_t config = {.adv_en = 1};
    const char *keys[] = {"-e"};
    const char *fmts[] = {"%i"};
    const uint8_t types[] = {TYPE_UINT8};
    void *values[] = {&config.adv_en};
    uint8_t result = parse_params(params, keys, types, fmts, values, 1);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEADVSTART: adv_en=%d", config.adv_en);
#ifdef BT_WIFI_COEX
    atcmd_ble_adv_start_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_adv_stop(char *params)
{
    ble_adv_en_t config = {.adv_en = 0};
    const char *keys[] = {"-e"};
    const char *fmts[] = {"%i"};
    const uint8_t types[] = {TYPE_UINT8};
    void *values[] = {&config.adv_en};
    uint8_t result = parse_params(params, keys, types, fmts, values, 1);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEADVSTOP: adv_en=%d", config.adv_en);
#ifdef BT_WIFI_COEX
    atcmd_ble_adv_stop_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_conn(char *params)
{
    ble_conn_t config = {.addr_type = 1, .timeout = 1000, .remote_addr = 0x123456123456};
    char mac_str[18] = {0};
    const char *keys[] = {"-t", "-m", "-o"};
    const char *fmts[] = {"%i", "%17s", "%i"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_STRING, TYPE_UINT16};
    void *values[] = {&config.addr_type, mac_str, &config.timeout};
    uint8_t result = parse_params(params, keys, types, fmts, values, 3);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLECONN: addr_type=%d, mac=%s, timeout=%u",
          config.addr_type, mac_str[0] ? mac_str : "none", config.timeout);
    if (mac_str[0] && bt_parse_mac_addr(mac_str, config.remote_addr.addr))
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_ble_conn_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_conn_param(char *params)
{
    ble_conn_param_t config = {.conn_index = 0, .min_interval = 0x0010, .max_interval = 0x0020, .con_latency = 4, .timeout = 0x0190};
    const char *keys[] = {"-c", "-i", "-x", "-l", "-t"};
    const char *fmts[] = {"%i", "%i", "%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_UINT16, TYPE_UINT16, TYPE_UINT16, TYPE_UINT16, TYPE_UINT16};
    void *values[] = {&config.conn_index, &config.min_interval, &config.max_interval, &config.con_latency, &config.timeout};
    uint8_t result = parse_params(params, keys, types, fmts, values, 5);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLECONNPARAM: conn_index=%d, min_interval=%u, max_interval=%u, con_latency=%u, timeout=%u",
          config.conn_index, config.min_interval, config.max_interval, config.con_latency, config.timeout);

    if (config.conn_index > 10 || config.min_interval < CON_INTERVAL_MIN || config.min_interval > CON_INTERVAL_MAX ||
        config.max_interval < config.min_interval || config.max_interval > CON_INTERVAL_MAX ||
        config.timeout < CON_SUP_TO_MIN || config.timeout > CON_SUP_TO_MAX)
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_ble_conn_param_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_disconn(char *params)
{
    ble_disconn_t config = {.addr_type = 0, .remote_addr=0x123456123456};
    char mac_str[18] = {0};
    const char *keys[] = {"-t", "-m"};
    const char *fmts[] = {"%i", "%17s"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_STRING};
    void *values[] = {&config.addr_type, mac_str};
    uint8_t result = parse_params(params, keys, types, fmts, values, 2);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEDISCONN: addr_type=%d, mac=%s",
          config.addr_type, mac_str[0] ? mac_str : "none");

    if (mac_str[0] && bt_parse_mac_addr(mac_str, config.remote_addr.addr))
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_ble_disconn_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_data_len(char *params)
{
    ble_data_len_t config = {.conn_index = 1, .pkt_data_len = 0x0032};
    const char *keys[] = {"-c", "-l"};
    const char *fmts[] = {"%i", "%i"};
    const uint8_t types[] = {TYPE_UINT16, TYPE_UINT16};
    void *values[] = {&config.conn_index, &config.pkt_data_len};
    uint8_t result = parse_params(params, keys, types, fmts, values, 2);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEDATALEN: conn_index=%d, pkt_data_len=%u",
          config.conn_index, config.pkt_data_len);
    if (config.conn_index > 10 || config.pkt_data_len < LE_MIN_OCTETS || config.pkt_data_len > LE_MAX_OCTETS)
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_ble_data_len_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_sec_param(char *params)
{
    ble_sec_param_t config = {.auth_req = 1, .iocap = 1, .key_size = 16, .init_key = 1, .rsp_key = 1};
    const char *keys[] = {"-a", "-i", "-k", "-n", "-r"};
    const char *fmts[] = {"%i", "%i", "%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_UINT16, TYPE_UINT16, TYPE_UINT16, TYPE_UINT16, TYPE_UINT16};
    void *values[] = {&config.auth_req, &config.iocap, &config.key_size, &config.init_key, &config.rsp_key};
    uint8_t result = parse_params(params, keys, types, fmts, values, 5);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLESECPARAM: auth_req=%d, iocap=%d, key_size=%d, init_key=%d, rsp_key=%d",
          config.auth_req, config.iocap, config.key_size, config.init_key, config.rsp_key);
#ifdef BT_WIFI_COEX
    atcmd_ble_sec_param_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_enc(char *params)
{
    ble_enc_t config = {.conn_index = 1, .sec_act = 2};
    const char *keys[] = {"-c", "-s"};
    const char *fmts[] = {"%i", "%i"};
    const uint8_t types[] = {TYPE_UINT16, TYPE_UINT16};
    void *values[] = {&config.conn_index, &config.sec_act};
    uint8_t result = parse_params(params, keys, types, fmts, values, 2);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEENC: conn_index=%d, sec_act=%d",
          config.conn_index, config.sec_act);
    if (config.conn_index > 10 || config.sec_act > 4) {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_ble_enc_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_enc_dev(char *params)
{
    CLOGI("BLEENCDEV: no params");
#ifdef BT_WIFI_COEX
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_key_reply(char *params)
{
    ble_key_reply_t config = {.conn_index = 1};
    char key_str[KEY_LEN + 1] = "default_key";
    const char *keys[] = {"-c", "-k"};
    const char *fmts[] = {"%i", "%16s"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_STRING};
    void *values[] = {&config.conn_index, key_str};
    uint8_t result = parse_params(params, keys, types, fmts, values, 2);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEKEYREPLY: conn_index=%d, key=%s",
          config.conn_index, key_str);
    if (config.conn_index > 10) {
        return CLI_INVALID_PARAM;
    }
    memcpy(config.key.ltk, key_str, KEY_LEN);
#ifdef BT_WIFI_COEX
    atcmd_ble_key_reply_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_enc_clear(char *params)
{
    ble_enc_clear_t config = {.type = 1, .bd_addr=0x123456123456};
    char mac_str[18] = {0};
    const char *keys[] = {"-t", "-m"};
    const char *fmts[] = {"%i", "%17s"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_STRING};
    void *values[] = {&config.type, mac_str};
    uint8_t result = parse_params(params, keys, types, fmts, values, 2);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLEENCCLEAR: type=%d, mac=%s",
          config.type, mac_str[0] ? mac_str : "none");
    if (mac_str[0] && bt_parse_mac_addr(mac_str, config.bd_addr.addr)) {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_ble_enc_clear_send(&config);
    return CLI_SUCCESS;
#else
    ble_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_non_signal_tx(char *params)
{
    uint8_t channel = 1, data_len = 20, payload = 1, phy = 1, fhss = 0;
    const char *keys[] = {"-c", "-l", "-p", "-h", "-f"};
    const char *fmts[] = {"%i", "%i", "%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT8, TYPE_UINT8, TYPE_UINT8, TYPE_UINT8};
    void *values[] = {&channel, &data_len, &payload, &phy, &fhss};
    uint8_t result = parse_params(params, keys, types, fmts, values, 5);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLENONSIGNALTX: channel=%d, data_len=%d, payload=%d, phy=%d, fhss=%d",
          channel, data_len, payload, phy, fhss);
#ifdef BT_WIFI_COEX
    atcmd_ble_nonsignal_tx_send(channel, data_len, payload, phy, fhss);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_non_signal_rx(char *params)
{
    uint8_t channel = 1, phy = 1, mod_idx = 1, infinite_rx_mode = 0;
    const char *keys[] = {"-c", "-p", "-m", "-i"};
    const char *fmts[] = {"%i", "%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT8, TYPE_UINT8, TYPE_UINT8};
    void *values[] = {&channel, &phy, &mod_idx, &infinite_rx_mode};
    uint8_t result = parse_params(params, keys, types, fmts, values, 4);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BLENONSIGNALRX: channel=%d, phy=%d, mod_idx=%d, infinite_rx_mode=%d",
          channel, phy, mod_idx, infinite_rx_mode);
#ifdef BT_WIFI_COEX
    atcmd_ble_nonsignal_rx_send(channel, phy, mod_idx, infinite_rx_mode);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_non_signal_end(char *params)
{
    CLOGI("BLENONSIGNALEND: no params");
#ifdef BT_WIFI_COEX
    atcmd_ble_nonsignal_end_send();
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int bt_reset(char *params)
{
    int res = CLI_SUCCESS;
    #ifdef BT_WIFI_COEX
    lsip_reset();
    #endif

    return res;
}


/* Classic Bluetooth Commands */
int bt_inquiry(char *params)
{
    bt_inq_t config = {.lap.A = {0x33, 0x8B, 0x9E}, .lap = 0x9E8B33, .inq_len = 8, .nb_rsp = 0};
    const char *keys[] = {"-l", "-i", "-n"};
    const char *fmts[] = {"%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_UINT24, TYPE_UINT8, TYPE_UINT8};
    void *values[] = {&config.lap.A[0], &config.inq_len, &config.nb_rsp};
    uint8_t result = parse_params(params, keys, types, fmts, values, 3);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BTINQUIRY: lap=0x%02x:%02x:%02x, inq_len=%d, nb_rsp=%d",
          config.lap.A[0],config.lap.A[1],config.lap.A[2],config.inq_len, config.nb_rsp);
    //if (config.lap < 0x9E8B00 || config.lap > 0x9E8B3F || config.inq_len < 1 || config.inq_len > 0x30 || config.nb_rsp > 0xFF)
    if (config.inq_len < 1 || config.inq_len > 0x30 || config.nb_rsp > 0xFF)
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_bt_inquiry_send(&config);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int bt_scan(char *params)
{
    uint8_t enable = 1;
    const char *keys[] = {"-e"};
    const char *fmts[] = {"%i"};
    const uint8_t types[] = {TYPE_UINT8};
    void *values[] = {&enable};
    uint8_t result = parse_params(params, keys, types, fmts, values, 1);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BTSCAN: enable=%d", enable);
#ifdef BT_WIFI_COEX
    atcmd_bt_scan_send(enable);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int bt_dutmode(char *params)
{
    uint8_t enable = 1;
    const char *keys[] = {"-e"};
    const char *fmts[] = {"%i"};
    const uint8_t types[] = {TYPE_UINT8};
    void *values[] = {&enable};
    uint8_t result = parse_params(params, keys, types, fmts, values, 1);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BTDUTMODE: enable=%d", enable);
#ifdef BT_WIFI_COEX
    atcmd_bt_dutmode_send(enable);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int bt_conn(char *params)
{
    bt_conn_t config = {.bd_addr=0x123456123456, .pkt_type = 0xCC18, .page_scan_rep_mode = 1, .clk_off = 100, .switch_en = 1};
    char mac_str[18] = {0};
    const char *keys[] = {"-m", "-p", "-r", "-c", "-s"};
    const char *fmts[] = {"%17s", "%i", "%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_STRING, TYPE_UINT16, TYPE_UINT8, TYPE_UINT16, TYPE_UINT8};
    void *values[] = {mac_str, &config.pkt_type, &config.page_scan_rep_mode, &config.clk_off, &config.switch_en};
    uint8_t result = parse_params(params, keys, types, fmts, values, 5);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BTCONN: mac=%s, pkt_type=%d, page_scan_rep_mode=%d, clk_off=%d, switch_en=%d",
          mac_str[0] ? mac_str : "none", config.pkt_type, config.page_scan_rep_mode, config.clk_off, config.switch_en);
    if (mac_str[0] && bt_parse_mac_addr(mac_str, config.bd_addr.addr))
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_bt_conn_send(&config);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int bt_disconn(char *params)
{
    bt_disconn_t config = {.addr_type = 0, .remote_addr=0x123456123456};
    char mac_str[18] = {0};
    const char *keys[] = {"-t", "-m"};
    const char *fmts[] = {"%i", "%17s"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_STRING};
    void *values[] = {&config.addr_type, mac_str};
    uint8_t result = parse_params(params, keys, types, fmts, values, 2);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BTDISCONN: addr_type=%d, mac=%s",
          config.addr_type, mac_str[0] ? mac_str : "none");
    if (mac_str[0] && bt_parse_mac_addr(mac_str, config.remote_addr.addr))
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_bt_disconn_send(&config);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int bt_non_signal_tx(char *params)
{
    bt_non_signal_tx_t config = {.pkt_type = 0x0F, .pkt_len = 100, .pkt_per = 1, .pattern = 1, .tx_ch = 5, .tx_power = 0, .tx_value = 0};
    const char *keys[] = {"-t", "-l", "-p", "-r", "-c", "-w", "-v"};
    const char *fmts[] = {"%i", "%i", "%i", "%i", "%i", "%i", "%i"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT16, TYPE_UINT8, TYPE_UINT8, TYPE_UINT8, TYPE_UINT8, TYPE_UINT8};
    void *values[] = {&config.pkt_type, &config.pkt_len, &config.pkt_per, &config.pattern, &config.tx_ch, &config.tx_power, &config.tx_value};
    uint8_t result = parse_params(params, keys, types, fmts, values, 7);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("BTNONSIGNALTX: pkt_type=%d, pkt_len=%d, pkt_per=%u, pattern=%d, tx_ch=%d, tx_power=%d, tx_value=%d",
          config.pkt_type, config.pkt_len, config.pkt_per, config.pattern, config.tx_ch, config.tx_power, config.tx_value);
    if (config.pattern > 8 || config.tx_ch > 78)
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_bt_non_signal_tx_send(&config);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int bt_non_signal_rx(char *params)
{
    bt_non_signal_rx_t config = {.peer_bd_addr=0x123456123456, .rx_ch = 1, .pkt_type = 0x04, .infinite_mode = 0};
    char mac_str[18] = {0};
    const char *keys[] = {"-c", "-t", "-m", "-i"};
    const char *fmts[] = {"%i", "%i", "%17s", "%i"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT8, TYPE_STRING, TYPE_UINT8};
    void *values[] = {&config.rx_ch, &config.pkt_type, mac_str, &config.infinite_mode};
    uint8_t result = parse_params(params, keys, types, fmts, values, 4);
    if (result != CLI_SUCCESS) {
        return result;
    }
    CLOGI("BTNONSIGNALRX: rx_ch=%d, pkt_type=%d, mac=%s, infinite_mode=%d",
          config.rx_ch, config.pkt_type, mac_str[0] ? mac_str : "none", config.infinite_mode);
    if (mac_str[0] && bt_parse_mac_addr(mac_str, config.peer_bd_addr.addr))
    {
        return CLI_INVALID_PARAM;
    }
#ifdef BT_WIFI_COEX
    atcmd_bt_non_signal_rx_send(&config);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int bt_non_signal_disable(char *params)
{
        CLOGI("BTNONSIGNALDISABLE: no params");
#ifdef BT_WIFI_COEX
        atcmd_bt_non_signal_disable_send();
        return CLI_SUCCESS;
#else
        bt_not_support();
        return CLI_UNKNOWN_CMD;
#endif
}

int bt_non_signal_rx_get_data(char *params)
{
    CLOGI("BTNONSIGNALRXGETDATA: no params");
#ifdef BT_WIFI_COEX
    atcmd_bt_non_signal_rx_get_data_send();
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

/* Host Commands */
int ble_h_adv_start(char *params)
{
    uint8_t mode = 1;
    const char *keys[] = {"-m"};
    const char *fmts[] = {"%i"};
    const uint8_t types[] = {TYPE_UINT8};
    void *values[] = {&mode};
    uint8_t result = parse_params(params, keys, types, fmts, values, 1);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("ADVSTART: mode=%d", mode);
#ifdef BT_WIFI_COEX
    atcmd_hble_adv_start_send(mode);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_h_adv_stop(char *params)
{
    CLOGI("ADVSTOP: no params");
#ifdef BT_WIFI_COEX
    atcmd_hble_adv_stop_send();
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif
}

int ble_test_tone_start(char *params)
{
    uint8_t channel = 5, power = 5;
    const char *keys[] = {"-c", "-p"};
    const char *fmts[] = {"%i", "%i"};
    const uint8_t types[] = {TYPE_UINT8, TYPE_UINT8};
    void *values[] = {&channel, &power};
    uint8_t result = parse_params(params, keys, types, fmts, values, 2);
    if (result != CLI_SUCCESS)
    {
        return result;
    }
    CLOGI("TESTTONESTART: channel=%d, power=%d", channel, power);
#ifdef BT_WIFI_COEX
    atcmd_rf_test_tone_start_send(channel, power);
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

int ble_test_tone_stop(char *params)
{
    CLOGI("TESTTONESTOP: no params");
#ifdef BT_WIFI_COEX
    atcmd_rf_test_tone_stop_send();
    return CLI_SUCCESS;
#else
    bt_not_support();
    return CLI_UNKNOWN_CMD;
#endif

}

static int bt_cli_help(char *params)
{
    uint8_t i = 0;

    for (; cli_bt_commands[i].exec != NULL; i++)
    {
        CLOG(" - %s %s\n", cli_bt_commands[i].name, cli_bt_commands[i].params);
    }

    return CLI_SUCCESS;
}

/** There are 4 kinds of AT cmd
 *  (1) AT+<x>=?      (ATCMD_PARAM)  at cmd to get all configurable parameters range
 *  (2) AT+<x>?       (ATCMD_QUERY)  at cmd to get informations
 *  (3) AT+<x>=<...>  (ATCMD_EXEC)   at cmd with parameters
 *  (4) AT+<x>        (ATCMD_EXEC)   at cmd without parameters
**/
static const struct cli_cmd cli_bt_commands[] =
{
    // common
    {bt_cli_help,          "bt?",                   ""},
    ///bt
    {bt_reset,             "bt_reset",              ":reset bt"},
    // bt controller
    {ble_init,        "bleinit",              "bleinit [-i <init>]\r\n : BLE init, <init>: 0=disable, 1=enable, default 0\r\n"},
    {ble_name,        "blename",              "blename [-n <name>]\r\n : Set BLE name, <name>: string (max 31 chars), default empty\r\n"},
    {ble_scan_param,   "blescanparam",         "blescanparam [-t <scan_type>] [-f <scan_filt_policy>] [-i <scan_intv>] [-w <scan_window>]\r\n"
                                               " : Set BLE scan params, <scan_type>: 0=passive 1=active, <scan_filt_policy>: 0=duplicate 1=whitelist 2=extended, <scan_intv>: interval, <scan_window>: window, all default 0\r\n"
                          "                   <scan_type:0 passive scan 1: active scan>\r\n"
                         "                    <fiter_policy>:\r\n"
                         "                       0 BT_LE_SCAN_FILTER_DUPLICATE\r\n"
                         "                       1 BT_LE_SCAN_FILTER_WHITELIST\r\n"
                         "                       2 BT_LE_SCAN_FILTER_EXTENDED"},
    {ble_scan,        "blescan",               "blescan [-e <enable>] [-i <intv>] [-f <filter_type>] [-m <mac>]\r\n"
                                               " : BLE scan, <enable>: 0=continuous 1=disable, <intv>: interval (seconds), <filter_type>: 0/1, <mac>: xx:xx:xx:xx:xx:xx (if filter_type=1), defaults 0\r\n"
                                               "Response: +BLESCAN:<addr>,<event_type>,<rssi>,<name>\r\n"
                         "                   <enable>: 1 disable continuous scanning, 0: enable continuous scanning\r\n"
                         "                   <interval>: optional parameter, unit second\r\n"
                         "                       when disabling the scanning, this parameter should be omitted\r\n"
                         "                       when enabling the scanning, and <interval> is 0, it means that scanning is continuous\r\n"
                         "                       when enabling the scanning, and <interval> is NOT 0,for example, command, AT+BLESCAN=0,3,\r\n"
                         "                       it means that scanning should last for 3 seconds and then stop automatically"},
    {ble_scan_rsp_data, "blescanrspdata",       "blescanrspdata [-t <type>] [-l <data_len>] [-d <data>]\r\n"
                                                   " : Set BLE scan rsp data, <type>: type, <data_len>: length, <data>: HEX string (max 31 chars), defaults 0\r\n"},
    {ble_adv_param,    "bleadvparam",           "bleadvparam [-t <adv_type>] [-m <adv_mode>] [-i <adv_int_min>] [-x <adv_int_max>]\r\n"
                                                   " : Set BLE adv params, <adv_type>: 0=IND 1=SCAN_IND 2=NONCONN_IND, <adv_mode>: 0=GENER_DISC 1=NON_DISC 2=LIMIT_DISC, <adv_int_min/max>: 0x0020-0x4000, defaults 0\r\n"},
    {ble_adv_data,     "bleadvdata",           "bleadvdata [-t <adv_type>] [-l <data_len>] [-d <data>]\r\n"
                                                   " : Set BLE adv data, <adv_type>: 0-2, <data_len>: length, <data>: HEX string (max 31 chars), defaults 0\r\n"
                         "                   <adv_type>\r\n"
                         "                       0: ADV_TYPE_IND\r\n"
                         "                       1: ADV_TYPE_SCAN_IND\r\n"
                         "                       2: ADV_TYPE_NONCONN_IND\r\n"
                         "                   <adv_mode>\r\n"
                         "                       0: GENER_DISC_MODE\r\n"
                         "                       1: NON_DISC_MODE\r\n"
                         "                       2: LIMIT_DISC_MODE\r\n"
                         "                   <adv_int_min> minimum value of advertising interval, range: 0x0020 ~ 0x4000\r\n"
                         "                   <adv_int_max> maximum value of advertising interval, range: 0x0020 ~ 0x4000"},
    {ble_adv_start,    "bleadvstart",          "bleadvstart [-e <enable>]\r\n : Start BLE adv, <enable>: 0/1, default 1\r\n"},
    {ble_adv_stop,     "bleadvstop",           "bleadvstop [-e <enable>]\r\n : Stop BLE adv, <enable>: 0/1, default 0\r\n"},
    {ble_conn,        "bleconn",              "bleconn [-t <addr_type>] [-m <mac>] [-o <timeout>]\r\n"
                                                   " : BLE connect, <addr_type>: 0=PUBLIC 1=RAND 2=RPA_OR_PUBLIC 3=RPA_OR_RAND, <mac>: xx:xx:xx:xx:xx:xx, <timeout>: ms (hex), defaults 0\r\n"},
    {ble_conn_param,   "bleconnparam",         "bleconnparam [-c <conn_index>] [-i <min_interval>] [-x <max_interval>] [-l <con_latency>] [-t <timeout>]\r\n"
                                                   " : Set BLE conn params, <conn_index>: 0-10, <min/max_interval>: 0x0006-0x0C80, <con_latency>: 0x0000-0x01F3, <timeout>: 0x000A-0x0C80, defaults 0\r\n"},
    {ble_disconn,     "bledisconn",           "bledisconn [-t <addr_type>] [-m <mac>]\r\n"
                                                   " : BLE disconnect, <addr_type>: 0-3, <mac>: xx:xx:xx:xx:xx:xx, defaults 0\r\n"},
    {ble_data_len,     "bledatalen",           "bledatalen [-c <conn_index>] [-l <pkt_data_len>]\r\n"
                                                   " : Set BLE data len, <conn_index>: 0-10, <pkt_data_len>: 0x001B-0x00FB, defaults 0\r\n"},
    {ble_sec_param,    "blesecparam",          "blesecparam [-a <auth_req>] [-i <iocap>] [-k <key_size>] [-n <init_key>] [-r <rsp_key>]\r\n"
                                                   " : Set BLE sec params, <auth_req>: auth, <iocap>: capability, <key_size>: size, <init/rsp_key>: keys, defaults 0\r\n"
                                                   "Response: +BLESECPARAM:<auth_req>,<iocap>,<key_size>,<init_key>,<rsp_key>\r\n"},
    {ble_enc,         "bleenc",               "bleenc [-c <conn_index>] [-s <sec_act>]\r\n"
                                                   " : BLE encryption, <conn_index>: 0-10, <sec_act>: 1=NONE 2=ENCRYPT 3=NO_MITM 4=MITM, defaults 0\r\n"},
    {ble_key_reply,    "blekeyreply",          "blekeyreply [-c <conn_index>] [-k <key>]\r\n"
                                                   " : BLE key reply, <conn_index>: 0-10, <key>: string (max 16 chars), defaults 0\r\n"},
    {ble_enc_dev,      "bleencdev",            "bleencdev\r\n : BLE enc dev (query only)\r\n"},
    {ble_enc_clear,    "bleencclear",          "bleencclear [-t <type>] [-m <mac>]\r\n"
                                                   " : BLE enc clear, <type>: address type, <mac>: xx:xx:xx:xx:xx:xx (0=clear all), defaults 0\r\n"},
    {ble_non_signal_tx, "blenonsignaltx",       "blenonsignaltx [-c <channel>] [-l <data_len>] [-p <payload>] [-h <phy>] [-f fhss]\r\n"
                                                   " : BLE nonsignal tx, <channel>: 0x00-0x27, <data_len>: 0x00-0xFF, <payload>: 0x00-0x08, <phy>: 1=1M 2=2M 3=coded, defaults 0\r\n"
                         "                       <tx_channel>: tx channel,     range: 0x00~0x27\r\n"
                         "                       <data_len>:   tx data length, range: 0x00~0xFF\r\n"
                         "                       <pkt_payl>:\r\n"
                         "                           0x00 PRBS9 sequence\r\n"
                         "                           0x01 Repeated 11110000\r\n"
                         "                           0x02 Repeated 10101010\r\n"
                         "                           0x03 PRBS15 sequence\r\n"
                         "                           0x04 Repeated 11111111\r\n"
                         "                           0x05 Repeated 00000000\r\n"
                         "                           0x06 Repeated 00001111\r\n"
                         "                           0x07 Repeated 01010101\r\n"
                         "                           0x08 inifinite mode"},
    {ble_non_signal_rx, "blenonsignalrx",       "blenonsignalrx [-c <channel>] [-p <phy>] [-m <mod_idx>] [-i <infinite_rx_mode>]\r\n"
                                                 " : BLE nonsignal rx, <channel>: 0x00-0x27, <phy>: 1=1M 2=2M 3=coded, <mod_idx>: 0=standard 1=stable, <infinite_rx_mode>: 0/1, defaults 0\r\n"
                         "                       <rx_channel>: rx channel,   range: 0x00~0x27\r\n"
                         "                       <phy>: 1: 1M 2: 2M 3: coded\r\n"
                         "                       <mod_idx>: 0: standard 1: stable\r\n"
                         "                       <infinite_rx_mode>: 1: inifinate rx mode 0: normal rx mode"},
    {ble_non_signal_end, "blenonsignalend",     "blenonsignalend\r\n : End BLE nonsignal test, response: <nb_pkt_recv>\r\n"},
    {bt_inquiry,       "btinquiry",           "btinquiry [-l <lap>] [-i <inq_len>] [-n <nb_rsp>]\r\n"
                                                   " : BT inquiry, <lap>: 0x9E8B00-0x9E8B3F (default 0x9E8B33), <inq_len>: 0x01-0x30 (default 8), <nb_rsp>: 0x00-0xFF (default 0)\r\n"
                                                   "Response: +BTINQUIRY:<addr>,<class_of_dev>,<page_scan_req_mode>,<clk_offset>\r\n"},
    {bt_scan,          "btscan",              "btscan [-e <enable>]\r\n : BT scan, <enable>: 0=none 1=inquiry 2=page 3=both, default 0\r\n"},
    {bt_dutmode,       "btdutmode",           "btdutmode [-e <enable>]\r\n : BT DUT mode, <enable>: 0/1, default 0\r\n"},
    {bt_conn,          "btconn",              "btconn [-m <mac>] [-p <pkt_type>] [-r <page_scan_rep_mode>] [-c <clk_off>] [-s <switch_en>]\r\n"
                                                   " : BT connect, <mac>: xx:xx:xx:xx:xx:xx, <pkt_type>: packet type, <page_scan_rep_mode>: mode, <clk_off>: offset, <switch_en>: 0/1, defaults 0\r\n"},
    {bt_disconn,       "btdisconn",           "btdisconn [-t <addr_type>] [-m <mac>]\r\n"
                                                   " : BT disconnect, <addr_type>: type, <mac>: xx:xx:xx:xx:xx:xx, defaults 0\r\n"},
    {bt_non_signal_tx,   "btnonsignaltx",       "btnonsignaltx [-t <pkt_type>] [-l <pkt_len>] [-p <pkt_per>] [-r <pattern>] [-c <tx_ch>] [-w <tx_power>] [-v <tx_value>]\r\n"
                                                " : BT nonsignal tx, <pkt_type>: 0x03-0x1F, <pkt_len>: length, <pkt_per>: period (default 1), <pattern>: 0x00-0x08, <tx_ch>: 0-78, <tx_power>: 0/1, <tx_value>: value, defaults 0\r\n"
                                                "                 <pkt_type>\r\n"
                           "                     DM1:0x03\r\n"
                           "                     DM3:0x0A\r\n"
                           "                     DM5:0x0E\r\n"
                           "                     DH1:0x04\r\n"
                           "                     DH3:0x0B\r\n"
                           "                     DH5:0x0F\r\n"
                           "                     2-DH1:0x14\r\n"
                           "                     2-DH3:0x1A\r\n"
                           "                     2-DH5:0x1E\r\n"
                           "                     3-DH1:0x18\r\n"
                           "                     3-DH3:0x1B\r\n"
                           "                     3-DH5:0x1F\r\n"
                           "                 <packet_period>: interv between packets, default is 1\r\n"
                           "                 <pattern>:\r\n"
                           "                     0x00 PRBS9 sequence\r\n"
                           "                     0x01 Repeated 11110000\r\n"
                           "                     0x02 Repeated 10101010\r\n"
                           "                     0x03 PRBS15 sequence\r\n"
                           "                     0x04 Repeated 11111111\r\n"
                           "                     0x05 Repeated 00000000\r\n"
                           "                     0x06 Repeated 00001111\r\n"
                           "                     0x07 Repeated 01010101\r\n"
                           "                     0x08 inifinite mode\r\n"
                           "                <tx_ch>: tx channel, range: 0 ~ 78\r\n"
                           "                <tx_power>: 0 index 1 keyword, default is 0\r\n"
                           "                <tx_value>: index or  keyword, default is 0"},
    {bt_non_signal_rx,   "btnonsignalrx",      "btnonsignalrx [-c <rx_ch>] [-t <pkt_type>] [-m <mac>] [-i <infinite_mode>]\r\n"
                                               " : BT nonsignal rx, <rx_ch>: 0-78, <pkt_type>: 0x03-0x1F, <mac>: xx:xx:xx:xx:xx:xx, <infinite_mode>: 0/1, defaults 0\r\n"
                           "                <rx_channel>:       rx channel,   range: 0~78\r\n"
                           "                <pkt_type>:         as tx\r\n"
                           "                <peer_addr>:        remote bd adde\r\n"
                           "                <infinite_rx_mode>: 1: inifinate rx mode 0: normal rx mode"},
    {bt_non_signal_disable, "btnonsignaldisable",     "btnonsignaldisable\r\n : Disable BT nonsignal test\r\n"},
    {bt_non_signal_rx_get_data, "btnonsignalrxgetdata", "btnonsignalrxgetdata\r\n : Get BT nonsignal rx data, response: <total_packets>,<error_packets>,<total_bits>,<error_bits>\r\n"},
    // test tone
    {ble_test_tone_start,   "bletesttonestart",       "testtonestart [-c <channel>] [-p <power>]\r\n : RF test tone start, <channel>: channel, <power>: power, defaults 0\r\n"},
    {ble_test_tone_stop,    "bletesttonestop",        "testtonestop\r\n : RF stop test tone\r\n"},
    // bt host
    {ble_h_adv_start,    "blehadvstart",         "blehostadvstart [-m <mode>]\r\n : Host adv start, <mode>: mode, default 0\r\n"},
    {ble_h_adv_stop,     "blehadvstop",          "blehostadvstart\r\n : Host adv stop, response: <status>\r\n"},
    {NULL, "", ""}
};


uint32_t bt_cmd_handler(char* command, int len)
{
    uint32_t res;
    char *param;
    const struct cli_cmd *cmd;

    #if BT_WIFI_COEX
    hci_event_notify_reg(ls_event_post);
    #endif

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

    cmd = cli_bt_commands;
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
            CLI_LOG("Usage:\n%s %s\n",
                        cmd->name, cmd->params);
        }
    }
    else
    {
        res = CLI_UNKNOWN_CMD;
    }

    return res;
}

/// @} SHELL CMD BT
