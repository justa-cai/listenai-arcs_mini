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
#include "atcmd_bt_if.h"
#include "log_print.h"
#include "ls_event.h"
#include "ls_bt_type.h"
#include "bt_config.h"


#define RETURN_IF_ERROR(cond, err_code, fmt, ...) \
    do { \
        if (cond) { \
            atcmd_rspdata(fmt, err_code); \
            return ATCMD_ERROR; \
        } \
    }while(0)
struct out_bd_addr ble_scan_filter_bd_addr;

extern uint32_t lsip_get_em_base_addr(void);

int atoi_ex(const char *str)
{
    int result = 0;
    int base   = 10;
    const char *ptr = str;

    if (!str || !*str)
    {
        return 0;
    }
    if (ptr[0] == '0' &&((ptr[1] == 'x')||(ptr[1] == 'X')))
    {
        base = 16;
        ptr += 2;
    }

    while(*ptr)
    {
        int digit = 0;
        char c = *ptr;
        if (c >= '0' && c <= '9')
        {
            digit = c - '0';
        }
        else if(base == 16)
        {
            if(c >= 'a'&&c<='f')
            {
                digit = 10 + (c - 'a');
            }
            else if(c >= 'A'&&c<='F')
            {
                digit = 10 + (c - 'A');
            }
            else
            {
                break;
            }
        }

        result = result * base + digit;
        ptr++;
    }

    return result;

}

int atcmd_bt_parse_mac_addr(char *str, uint8_t *addr)
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


uint8_t atcmd_bt_not_support(void)
{
    CLOGD("atcmd_bt_not_support,open BT_WIFI_COEX\n");
}

void atcmd_ble_not_support(void)
{
    CLOGD("atcmd_ble_not_support,open BT_WIFI_COEX\n");
}
/// controller
int atcmd_bleinit(int type, char *params)
{
    ls_err_t ret;
    uint8_t init = 0;

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
        init = atoi(params);
        #if BT_WIFI_COEX
        atcmd_ble_init_send(init);
        #else
        atcmd_ble_not_support();
        #endif
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int atcmd_blename(int type, char *params)
{
    ls_err_t ret;
    uint8_t init = 0;
    uint8_t length = BD_NAME_SIZE;
    uint8_t local_name[BD_NAME_SIZE + 1];

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        params = atcmd_next_token(&params);
        CLOGD("atcmd_blename cmd send");

        if (!params)
        {
            atcmd_rspdata("CWAUTOCONN:%d", -ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        memcpy(local_name, params, length);
        #if BT_WIFI_COEX
        atcmd_blename_send(local_name);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_blescanparam(int type, char *params)
{
    int res;
    ls_err_t ret;
    ble_scan_params_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        //const char *tokens[] = {"scan_type", "scan_intv", "scan_window", "own_addr_type", "scan_filt_policy"};
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i%*[ ,]%i ", &config.scan_type, &config.scan_filt_policy, &config.scan_intv, &config.scan_window);
        RETURN_IF_ERROR(cnt<4, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("scan_type:%d", config.scan_type);
        CLOGI("scan_filt_policy:%d", config.scan_filt_policy);
        CLOGI("scan_intv:%d", config.scan_intv);
        CLOGI("scan_window:%d", config.scan_window);

        #if BT_WIFI_COEX
        atcmd_ble_scan_param_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_blescan(int type, char *params)
{
    int res;
    ls_err_t ret;
    ble_scan_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i% ", &config.enable, &config.intv, &config.filter_type);
        RETURN_IF_ERROR(cnt<3, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("enable     :%d", config.enable);
        CLOGI("intv       :%d", config.intv);
        CLOGI("filter_type:%d", config.filter_type);

        #if BT_WIFI_COEX
        if (config.filter_type == 1)
        {
            sscanf(params, "%*[^,],%*[^,],%*[^,], %2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x",
                &config.filter_param[0],&config.filter_param[1],&config.filter_param[2],&config.filter_param[3],&config.filter_param[4], &config.filter_param[5]);
            CLOGI("MAC %02x:%02x:%02x:%02x:%02x:%02x, evt_type:0x%x\n",config.filter_param[0], config.filter_param[1],config.filter_param[2],config.filter_param[3],config.filter_param[4],config.filter_param[5]);
            memcpy(&ble_scan_filter_bd_addr.addr[0], config.filter_param, BD_ADDR_LEN);
        }
        atcmd_ble_scan_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}


int ble_parse_scan_rspdata(char *params, ble_scan_rspdata_t *config)
{
    char *cur;
    char *next = params;
    int8_t token_idx = -1;

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
            case 0: //type
            {
                if (cur)
                {
                    ///
                    config->type = atoi_ex(cur);
                }
                break;
            }
            case 1: //data_len
            {
                if (cur)
                {
                    config->data_len = atoi_ex(cur);
                }
                break;
            }
            case 2: //scan rspdata
            {
                if (cur)
                {
                    memcpy(config->data.data, cur, SCAN_RSP_DATA_LEN);
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


int atcmd_blescanrspdata(int type, char *params)
{
    int res;
    ls_err_t ret;
    ble_scan_rspdata_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        res = ble_parse_scan_rspdata(params, &config);

        if (res)
        {
            atcmd_rspdata("blescanrspdata:%d", ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }
        #if BT_WIFI_COEX
        atcmd_ble_scan_rsp_data_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_bleadvparam(int type, char *params)
{
    int res = 0;
    ls_err_t ret;
    ble_adv_param_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i%*[ ,]%i ", &config.adv_type, &config.adv_mode, &config.adv_int_min, &config.adv_int_max);
        RETURN_IF_ERROR(cnt<4, ATCMD_ERR_PARAM_INVALID, "Invalid param count");
        CLOGI("adv_type   :%d", config.adv_type);
        CLOGI("adv_mode   :%d", config.adv_mode);
        CLOGI("adv_int_min:%d", config.adv_int_min);
        CLOGI("adv_int_max:%d", config.adv_int_max);
        // 0: ADV_TYPE_IND 1: ADV_TYPE_SCAN_IND 2: ADV_TYPE_NONCONN_IND
        if (config.adv_type > 2)
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        // 0: GENER_DISC_MODE 1: NON_DISC_MODE 2:LIMIT_DISC_MODE
        else if (config.adv_mode > 2)
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        else if ((config.adv_int_min < ADV_INTERVAL_MIN) || (config.adv_int_min > ADV_INTERVAL_MAX))
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        else if ((config.adv_int_max < config.adv_int_min) || (config.adv_int_max > ADV_INTERVAL_MAX))
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        if (res)
        {
            RETURN_IF_ERROR(1, ATCMD_ERR_PARAM_INVALID, "param invalid");
        }

        #if BT_WIFI_COEX
        atcmd_ble_adv_param_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int ble_parse_adv_data(char *params, ble_adv_data_t *config)
{
    char *cur;
    char *next = params;
    int8_t token_idx = -1;

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
            case 0: //adv type
            {
                if (cur)
                {
                    // 0: ADV_TYPE_IND 1: ADV_TYPE_SCAN_IND 2: ADV_TYPE_NONCONN_IND
                    config->adv_type = atoi_ex(cur);
                    if (config->adv_type > 2)
                    {
                        return ATCMD_ERR_PARAM_INVALID;
                    }
                }
                break;
            }
            case 1: //data_len
            {
                if (cur)
                {
                    config->data_len = atoi_ex(cur);
                }
                break;
            }
            case 2: //adv_data
            {
                if (cur)
                {
                    memcpy(config->data.data, cur,ADV_DATA_LEN);
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

int atcmd_bleadvdata(int type, char *params)
{
    int res;
    ls_err_t ret;
    ble_adv_data_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        res = ble_parse_adv_data(params, &config);

        if (res)
        {
            atcmd_rspdata("bleadvparam:%d", ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }
        #if BT_WIFI_COEX
        atcmd_ble_adv_data_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }


}

int atcmd_bleadvstart(int type, char *params)
{

    ble_adv_en_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        config.adv_en = 1;

        #if BT_WIFI_COEX
        atcmd_ble_adv_start_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_bleadvstop(int type, char *params)
{
    ble_adv_en_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        config.adv_en = 0;

        #if BT_WIFI_COEX
        atcmd_ble_adv_stop_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int atcmd_bleconn(int type, char *params)
{
    int res;
    ls_err_t ret;
    ble_conn_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*[ ,]%i ", &config.addr_type, &config.remote_addr.addr[0],&config.remote_addr.addr[1],
                                     &config.remote_addr.addr[2],&config.remote_addr.addr[3],&config.remote_addr.addr[4],&config.remote_addr.addr[5],&config.timeout);

        RETURN_IF_ERROR(cnt<8, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("addr_type:%d", config.addr_type);
        CLOGI("timeout  :%d", config.timeout);

        CLOGI("atcmd_bleconn MAC %02x:%02x:%02x:%02x:%02x:%02x\n",config.remote_addr.addr[0], config.remote_addr.addr[1],config.remote_addr.addr[2],config.remote_addr.addr[3],config.remote_addr.addr[4],config.remote_addr.addr[5]);

        #if BT_WIFI_COEX
        atcmd_ble_conn_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_bleconnparam(int type, char *params)
{
    int res = 0;
    ble_conn_param_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i%*[ ,]%i%*[ ,]%i ", &config.conn_index, &config.min_interval, &config.max_interval, &config.con_latency, &config.timeout);
        RETURN_IF_ERROR(cnt<5, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("conn_index  :%d", config.conn_index);
        CLOGI("min_interval:%d", config.min_interval);
        CLOGI("max_interval:%d", config.max_interval);
        CLOGI("con_latency :%d", config.con_latency);
        CLOGI("timeout     :%d", config.timeout);

        if (config.conn_index > 10)
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        else if ((config.min_interval<CON_INTERVAL_MIN)||(config.min_interval>CON_INTERVAL_MAX))
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        else if ((config.max_interval<config.min_interval)||(config.max_interval>CON_INTERVAL_MAX))
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        else if ((config.timeout<CON_SUP_TO_MIN) || (config.timeout>CON_SUP_TO_MAX))
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        if (res)
        {
            RETURN_IF_ERROR(1, ATCMD_ERR_PARAM_INVALID, "param invalid");
        }

        #if BT_WIFI_COEX
        atcmd_ble_conn_param_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_bledisconn(int type, char *params)
{
    int res;
    ble_disconn_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x ", &config.addr_type, &config.remote_addr.addr[0], &config.remote_addr.addr[1],
                                                         &config.remote_addr.addr[2], &config.remote_addr.addr[3],&config.remote_addr.addr[4],&config.remote_addr.addr[5]);
        RETURN_IF_ERROR(cnt<7, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("addr_type:%d", config.addr_type);
        CLOGI("MAC %02x:%02x:%02x:%02x:%02x:%02x\n",config.remote_addr.addr[0], config.remote_addr.addr[1],config.remote_addr.addr[2],config.remote_addr.addr[3],config.remote_addr.addr[4],config.remote_addr.addr[5]);

        #if BT_WIFI_COEX
        atcmd_ble_disconn_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_bledatalen(int type, char *params)
{
    int res = 0;
    ble_data_len_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%i ", &config.conn_index, &config.pkt_data_len);
        RETURN_IF_ERROR(cnt<2, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        if (config.conn_index >10)
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        else if ((config.pkt_data_len<LE_MIN_OCTETS) || (config.pkt_data_len>LE_MAX_OCTETS))
        {
            res = ATCMD_ERR_PARAM_INVALID;
        }
        if (res)
        {
            RETURN_IF_ERROR(1, ATCMD_ERR_PARAM_INVALID, "param invalid");
        }

        #if BT_WIFI_COEX
        atcmd_ble_data_len_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_blesecparam(int type, char *params)
{
    int res;
    ble_sec_param_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i%*[ ,]%i%*[ ,]%i ", &config.auth_req, &config.iocap, &config.key_size, &config.init_key, &config.rsp_key);
        RETURN_IF_ERROR(cnt<5, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        #if BT_WIFI_COEX
        atcmd_ble_sec_param_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_bleenc(int type, char *params)
{
    int res;
    ble_enc_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%i ", &config.conn_index, &config.sec_act);
        RETURN_IF_ERROR(cnt<2, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        if ((config.conn_index > 10) || (config.sec_act > 4))
        {
            RETURN_IF_ERROR(1, ATCMD_ERR_PARAM_INVALID, "Invalid param");
        }


        #if BT_WIFI_COEX
        atcmd_ble_enc_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int ble_parse_key_replay(char *params, ble_key_reply_t *config)
{
    char *cur;
    char *next = params;
    int8_t token_idx = -1;

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
            case 0: //conn_index
            {
                config->conn_index = atoi_ex(cur);
                if (config->conn_index > 10)
                {
                    return  ATCMD_ERR_PARAM_INVALID;
                }
                break;
            }
            case 1: //key
            {
                if (cur)
                {
                    //config->key = atoi_ex(cur);
                    memcpy(&config->key.ltk[0], cur, KEY_LEN);
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

int atcmd_blekeyreply(int type, char *params)
{
    int res;
    ble_key_reply_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        res = ble_parse_key_replay(params, &config);

        if (res)
        {
            atcmd_rspdata("bleenc:%d", ATCMD_ERR_UNSPECIF);
            return ATCMD_ERROR;
        }

        #if BT_WIFI_COEX
        atcmd_ble_key_reply_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}


int atcmd_bleencdev(int type, char *params)
{
    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
    }
    else //ATCMD_QUERY
    {
        /*#if BT_WIFI_COEX
        atcmd_ble_enc_dev_send();
        #else
        atcmd_ble_not_support();
        #endif*/

        return ATCMD_OK;
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_bleencclear(int type, char *params)
{
    int res;
    ble_enc_clear_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x ", &config.type, &config.bd_addr.addr[0], &config.bd_addr.addr[1],
                                                                 &config.bd_addr.addr[2], &config.bd_addr.addr[3],&config.bd_addr.addr[4],&config.bd_addr.addr[5]);
        RETURN_IF_ERROR(cnt<7, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        #if BT_WIFI_COEX
        atcmd_ble_enc_clear_send(&config);
        #else
        atcmd_ble_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_blenonsignaltx(int type, char *params)
{
    ls_err_t ret;
    uint8_t init = 0;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        uint8_t channel = 0, data_len = 0, payload = 0, phy = 0, fhss = 0;
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i%*[ ,]%i%*[ ,]%i ", &channel, &data_len, &payload, &phy, &fhss);
        RETURN_IF_ERROR(cnt<4, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("channel :%d", channel);
        CLOGI("data_len:%d", data_len);
        CLOGI("payload :%d", payload);
        CLOGI("phy     :%d", phy);
        CLOGI("fhss    :%d", fhss);

        #if BT_WIFI_COEX
        atcmd_ble_nonsignal_tx_send(channel, data_len, payload, phy, fhss);
        #else
        atcmd_bt_not_support();
        #endif
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}


int atcmd_blenonsignalrx(int type, char *params)
{
    ls_err_t ret;
    uint8_t init = 0;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        uint8_t channel = 0, phy = 0, mod_idx = 0, infinite_rx_mode = 0;
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i%*[ ,]%i ", &channel, &phy, &mod_idx, &infinite_rx_mode);
        RETURN_IF_ERROR(cnt<4, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("channel         :%d", channel);
        CLOGI("phy             :%d", phy);
        CLOGI("mod_idx         :%d", mod_idx);
        CLOGI("infinite_rx_mode:%d", infinite_rx_mode);

        #if BT_WIFI_COEX
        atcmd_ble_nonsignal_rx_send(channel, phy, mod_idx, infinite_rx_mode);
        #else
        atcmd_bt_not_support();
        #endif
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_blenonsignalend(int type, char *params)
{
    ls_err_t ret;
    uint8_t init = 0;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        #if BT_WIFI_COEX
        atcmd_ble_nonsignal_end_send();
        #else
        atcmd_bt_not_support();
        #endif
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int atcmd_btinquiry(int type, char *params)
{
    int res;
    bt_inq_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        uint32_t lap=0;
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i ", &lap, &config.inq_len, &config.nb_rsp);
        RETURN_IF_ERROR(cnt<3, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        config.lap.A[0] = lap&0xFF;
        config.lap.A[1] = (lap&0xFF00)>>8;
        config.lap.A[2] = (lap&0xFF0000)>>16;

        CLOGI("lap %02x:%02x:%02x", config.lap.A[0], config.lap.A[1], config.lap.A[2]);
        CLOGI("inq_len:%d", config.inq_len);
        CLOGI("nb_rsp :%d", config.nb_rsp);

        #if BT_WIFI_COEX
        atcmd_bt_inquiry_send(&config);
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_btscan(int type, char *params)
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
#if BT_WIFI_COEX
        atcmd_bt_scan_send(enable);
#else
        atcmd_bt_not_support();
#endif
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_btdutmode(int type, char *params)
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
    #if BT_WIFI_COEX
        atcmd_bt_dutmode_send(enable);
    #else
        atcmd_bt_not_support();
    #endif
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_btconn(int type, char *params)
{
    int res;
    bt_conn_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*[ ,]%i%*[ ,]%i%*[ ,]%i%*[ ,]%i ", &config.bd_addr.addr[0],&config.bd_addr.addr[1],&config.bd_addr.addr[2],
                              &config.bd_addr.addr[3],&config.bd_addr.addr[4],&config.bd_addr.addr[5],&config.pkt_type, &config.page_scan_rep_mode, &config.clk_off, &config.switch_en);
        RETURN_IF_ERROR(cnt<10, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("atcmd_btconn MAC %02x:%02x:%02x:%02x:%02x:%02x", config.bd_addr.addr[0], config.bd_addr.addr[1], config.bd_addr.addr[2],config.bd_addr.addr[3],config.bd_addr.addr[4],config.bd_addr.addr[5]);
        CLOGI("pkt_type          :%d", config.pkt_type);
        CLOGI("page_scan_rep_mode:%d", config.page_scan_rep_mode);
        CLOGI("clk_off           :%d", config.clk_off);
        CLOGI("switch_en         :%d", config.switch_en);

        #if BT_WIFI_COEX
        atcmd_bt_conn_send(&config);
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_btdisconn(int type, char *params)
{
    int res;
    bt_disconn_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x ", &config.addr_type, &config.remote_addr.addr[0], &config.remote_addr.addr[1],
                                                                 &config.remote_addr.addr[2], &config.remote_addr.addr[3],&config.remote_addr.addr[4],&config.remote_addr.addr[5]);
        RETURN_IF_ERROR(cnt<7, ATCMD_ERR_PARAM_INVALID, "Invalid param count");
        CLOGI("addr_type:%d", config.addr_type);
        CLOGI("atcmd_btdisconn MAC %02x:%02x:%02x:%02x:%02x:%02x", config.remote_addr.addr[0], config.remote_addr.addr[1], config.remote_addr.addr[2],config.remote_addr.addr[3],config.remote_addr.addr[4],config.remote_addr.addr[5]);

        #if BT_WIFI_COEX
        atcmd_bt_disconn_send(&config);
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_btnonsignaltx(int type, char *params)
{
    int res;
    bt_non_signal_tx_t config = {0};

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%i%*[ ,]%i%*[ ,]%i%*[ ,]%i%*[ ,]%i ", &config.pkt_type, &config.pkt_len, &config.pkt_per, &config.pattern, &config.tx_ch, &config.tx_power, &config.tx_value);
        RETURN_IF_ERROR(cnt<7, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        if ((config.pattern > 8) || (config.tx_ch > 78))
        {
            RETURN_IF_ERROR(1, ATCMD_ERR_PARAM_INVALID, "Invalid param");
        }

        CLOGI("pkt_type:%d", config.pkt_type);
        CLOGI("pkt_len :%d", config.pkt_len);
        CLOGI("pkt_per :%d", config.pkt_per);
        CLOGI("pattern :%d", config.pattern);
        CLOGI("tx_ch   :%d", config.tx_ch);
        CLOGI("tx_power:%d", config.tx_power);
        CLOGI("tx_value:%d", config.tx_value);

        #if BT_WIFI_COEX
        atcmd_bt_non_signal_tx_send(&config);
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_btnonsignalrx(int type, char *params)
{
    int res;
    bt_non_signal_rx_t config = {0};
    uint8_t rx_ch,  pkt_type, infinite_mode;

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%i%*[ ,]%i%*[ ,]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*[ ,]%i ", &rx_ch, &pkt_type, &config.peer_bd_addr.addr[0],&config.peer_bd_addr.addr[1],
                                                             &config.peer_bd_addr.addr[2],&config.peer_bd_addr.addr[3], &config.peer_bd_addr.addr[4],&config.peer_bd_addr.addr[5], &config.infinite_mode);

        RETURN_IF_ERROR(cnt<9, ATCMD_ERR_PARAM_INVALID, "Invalid param count");
        config.rx_ch = rx_ch;
        config.pkt_type = pkt_type;

        CLOGI("rx_ch        :%d", config.rx_ch);
        CLOGI("pkt_type     :%d", config.pkt_type);
        CLOGI("infinite_mode:%d", config.infinite_mode);

        CLOGI("MAC %02x:%02x:%02x:%02x:%02x:%02x", config.peer_bd_addr.addr[0], config.peer_bd_addr.addr[1], config.peer_bd_addr.addr[2],config.peer_bd_addr.addr[3],config.peer_bd_addr.addr[4],config.peer_bd_addr.addr[5]);
        #if BT_WIFI_COEX
        atcmd_bt_non_signal_rx_send(&config);
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int atcmd_btnonsignaldisable(int type, char *params)
{
    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        #if BT_WIFI_COEX
        atcmd_bt_non_signal_disable_send();
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
}

int atcmd_btnonsignalrxgetdata(int type, char *params)
{

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        #if BT_WIFI_COEX
        atcmd_bt_non_signal_rx_get_data_send();
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

/// host
int atcmd_hbleadvstart(int type, char *params)
{
    ls_err_t ret;
    uint8_t mode = 0;

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
        mode = atoi(params);
    #if BT_WIFI_COEX
        atcmd_hble_adv_start_send(mode);
    #else
        atcmd_bt_not_support();
    #endif
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}
int atcmd_hbleadvstop(int type, char *params)
{
    ls_err_t ret;
    uint8_t init = 0;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
    #if BT_WIFI_COEX
        atcmd_hble_adv_stop_send();
    #else
        atcmd_bt_not_support();
    #endif
        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}


#if (BT_WIFI_COEX)
#if (BT_EMB_PRESENT)
extern void ld_bd_addr_get(void* bd_addr);
#endif

#if (BLE_EMB_PRESENT)
extern void llm_get_local_pub_addr(uint8_t *addr);
#endif
extern void ble_gap_set_loc_pub_addr(uint8_t *addr);
#endif
int atcmd_bdaddr(int type, char *params)
{
    struct out_bd_addr lc_bd_addr;

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        int cnt = sscanf(params, "%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x%*1[:]%2x", &lc_bd_addr.addr[0],&lc_bd_addr.addr[1],
                         &lc_bd_addr.addr[2],&lc_bd_addr.addr[3], &lc_bd_addr.addr[4],&lc_bd_addr.addr[5]);

        RETURN_IF_ERROR(cnt<6, ATCMD_ERR_PARAM_INVALID, "Invalid param count");
        #if (BT_WIFI_COEX)
        ble_gap_set_loc_pub_addr(lc_bd_addr.addr);
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        struct out_bd_addr bd_addr;
        #if (BT_WIFI_COEX)
        #if (BT_EMB_PRESENT||BLE_EMB_PRESENT)
        #if(BT_EMB_PRESENT)
        ld_bd_addr_get(&bd_addr);

        atcmd_rspdata("BT MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                      bd_addr.addr[0], bd_addr.addr[1], bd_addr.addr[2],bd_addr.addr[3],bd_addr.addr[4],bd_addr.addr[5]);
        #endif
        #if(BLE_EMB_PRESENT)
        llm_get_local_pub_addr(bd_addr.addr);

        atcmd_rspdata("BLE MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                      bd_addr.addr[0], bd_addr.addr[1], bd_addr.addr[2],bd_addr.addr[3],bd_addr.addr[4],bd_addr.addr[5]);
        #endif
        #endif
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;

    }

}


int atcmd_testtonestart(int type, char *params)
{
    ls_err_t ret;
    uint8_t mode = 0;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        uint16_t channel = 0;
        uint16_t power = 0;
        int cnt = sscanf(params, "%i%*[ ,]%i ", &channel, &power);
        RETURN_IF_ERROR(cnt<2, ATCMD_ERR_PARAM_INVALID, "Invalid param count");

        CLOGI("channel:%d", channel);
        CLOGI("power  :%d", power);
        #if BT_WIFI_COEX
        atcmd_rf_test_tone_start_send(channel, power);
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int atcmd_testtonestop(int type, char *params)
{
    ls_err_t ret;
    uint8_t mode = 0;

    //CLOGI("%s %d %d %s\n", __func__, __LINE__, type, params);

    if (type == ATCMD_PARAM)
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        #if BT_WIFI_COEX
        atcmd_rf_test_tone_stop_send();
        #else
        atcmd_bt_not_support();
        #endif

        return ATCMD_OK;
    }
    else //ATCMD_QUERY
    {
        //CLOGI("%s %d\n",__func__, __LINE__);
        return ATCMD_UNKNOWN;
    }

}

int bt_event_cb(void *arg, event_module_t event_module,
                  int event_id, void *event_data)
{
    event_ble_non_signal_end_param_t         *ble_non_signal_end_param;
    event_bt_inq_result_param_t              *bt_inq_result_param;
    event_bt_inq_rssi_result_param_t         *bt_inq_rssi_result_param;
    event_bt_inq_eir_result_param_t          *bt_inq_eir_result_param;
    event_bt_non_signal_get_rx_data_param_t  *bt_get_rx_data_param;
    event_ble_scan_adv_report_param_t        *ble_scan_adv_report_param;
    event_ble_scan_ext_adv_report_param_t    *ble_scan_ext_adv_report_param;
    event_ble_scan_dir_adv_report_param_t    *ble_scan_dir_adv_report_param;

    struct out_bd_addr t_bd_addr = {0};

    switch (event_id)
    {
        case EVENT_BLE_INIT_DONE:
        break;
        case EVENT_BLE_CONNECTED:
        break;
        case EVENT_BLE_DISCONNECT:
        break;
        case EVENT_BLE_SCAN:
        break;
        case EVENT_BLE_SCAN_ADV_REPORT:
            ble_scan_adv_report_param = (event_ble_scan_adv_report_param_t*)event_data;
            memcpy(&t_bd_addr.addr[0], &ble_scan_adv_report_param->adv_rep[0].adv_addr.addr[0], BD_ADDR_LEN);
            if (!memcmp(&ble_scan_filter_bd_addr.addr[0], &t_bd_addr.addr[0], BD_ADDR_LEN))
            {
                CLOGI("ADV_REPORT_EVENT bd_addr %02x:%02x:%02x:%02x:%02x:%02x, evt_type:0x%x\n",t_bd_addr.addr[0], t_bd_addr.addr[1],t_bd_addr.addr[2],t_bd_addr.addr[3],t_bd_addr.addr[4],t_bd_addr.addr[5],ble_scan_adv_report_param->adv_rep[0].evt_type);
            }
        break;
        case EVENT_BLE_SCAN_EXT_ADV_REPORT:
            ble_scan_ext_adv_report_param = (event_ble_scan_ext_adv_report_param_t*)event_data;
            memcpy(&t_bd_addr.addr[0], &ble_scan_ext_adv_report_param->adv_rep[0].adv_addr.addr[0], BD_ADDR_LEN);
            if (!memcmp(&ble_scan_filter_bd_addr.addr[0], &t_bd_addr.addr[0], BD_ADDR_LEN))
            {
                CLOGI("ADV_EXT_REPORT_EVENT bd_addr %02x:%02x:%02x:%02x:%02x:%02x, evt_type:0x%x\n",t_bd_addr.addr[0], t_bd_addr.addr[1],t_bd_addr.addr[2],t_bd_addr.addr[3],t_bd_addr.addr[4],t_bd_addr.addr[5],ble_scan_ext_adv_report_param->adv_rep[0].evt_type);
            }
        break;
        case EVENT_BLE_SCAN_DIR_ADV_REPORT:
            ble_scan_dir_adv_report_param = (event_ble_scan_dir_adv_report_param_t*)event_data;
            memcpy(&t_bd_addr.addr[0], &ble_scan_dir_adv_report_param->adv_rep[0].addr.addr[0], BD_ADDR_LEN);
            if (!memcmp(&ble_scan_filter_bd_addr.addr[0], &t_bd_addr.addr[0], BD_ADDR_LEN))
            {
                CLOGI("ADV_DIR_REPORT_EVENT bd_addr %02x:%02x:%02x:%02x:%02x:%02x, evt_type:0x%x\n",t_bd_addr.addr[0], t_bd_addr.addr[1],t_bd_addr.addr[2],t_bd_addr.addr[3],t_bd_addr.addr[4],t_bd_addr.addr[5],ble_scan_dir_adv_report_param->adv_rep[0].evt_type);
            }
        break;
        case EVENT_BLE_DISCOVERY:
        break;
        case EVENT_BLE_NON_SIGNAL_END:
            ble_non_signal_end_param = (event_ble_non_signal_end_param_t*)event_data;
            ///put le end recv packet number in em, EM_BLE_WPAL_OFFSET = 0x0584
            // ble test mode only
            *(uint16_t*)(lsip_get_em_base_addr() + 0x0584) = ble_non_signal_end_param->nb_pkt_recv;
            CLOGI("nb_pkt_recv:%d\n", ble_non_signal_end_param->nb_pkt_recv);
        break;
        case EVENT_BT_CONNECTED:
        break;
        case EVENT_BT_DISCONNECT:
        break;
        case EVENT_BT_INQ_RESULT:
            bt_inq_result_param = (event_bt_inq_result_param_t*)event_data;
            memcpy(&t_bd_addr.addr[0], &bt_inq_result_param->bd_addr.addr[0], BD_ADDR_LEN);
            CLOGI("bd_addr %02x:%02x:%02x:%02x:%02x:%02x, clk_off:0x%x\n", event_module, event_id, t_bd_addr.addr[0], t_bd_addr.addr[1],t_bd_addr.addr[2],t_bd_addr.addr[3],t_bd_addr.addr[4],t_bd_addr.addr[5],bt_inq_result_param->clk_off);
        break;
        case EVENT_BT_INQ_RSSI_RESULT:
            bt_inq_rssi_result_param = (event_bt_inq_rssi_result_param_t*)event_data;
            memcpy(&t_bd_addr.addr[0], &bt_inq_rssi_result_param->bd_addr.addr[0], BD_ADDR_LEN);
            CLOGI("bd_addr %02x:%02x:%02x:%02x:%02x:%02x, clk_off:0x%x,rssi:%d\n", event_module, event_id, t_bd_addr.addr[0], t_bd_addr.addr[1],t_bd_addr.addr[2],t_bd_addr.addr[3],t_bd_addr.addr[4],t_bd_addr.addr[5],bt_inq_rssi_result_param->clk_off, bt_inq_rssi_result_param->rssi);
            //CLOGI("event <%d %d> bd_addr:0x%x, clk_off:0x%x, rssi:%d\n", event_module, event_id, bt_inq_rssi_result_param->bd_addr, bt_inq_rssi_result_param->clk_off, bt_inq_rssi_result_param->rssi);
        break;
        case EVENT_BT_INQ_EIR_RESULT:
            bt_inq_eir_result_param = (event_bt_inq_eir_result_param_t*)event_data;
            memcpy(&t_bd_addr.addr[0], &bt_inq_eir_result_param->bd_addr.addr[0], BD_ADDR_LEN);
            CLOGI("bd_addr %02x:%02x:%02x:%02x:%02x:%02x, clk_off:0x%x, rssi:%d, EIR\n", event_module, event_id, t_bd_addr.addr[0], t_bd_addr.addr[1],t_bd_addr.addr[2],t_bd_addr.addr[3],t_bd_addr.addr[4],t_bd_addr.addr[5],bt_inq_eir_result_param->clk_off, bt_inq_eir_result_param->rssi);
        break;
        case EVENT_BT_SCAN:
        break;
        case EVENT_BT_NON_SIGNAL_END:
        break;
        case EVENT_BT_NON_SIGNAL_RX_GET_DATA:
            bt_get_rx_data_param = (event_bt_non_signal_get_rx_data_param_t*)event_data;
            CLOGI("event <%d %d> total_packet:0x%x, error_packet:0x%x, total_bit:0x%x, error_bit:0x%x\n", event_module, event_id, bt_get_rx_data_param->total_packet, bt_get_rx_data_param->error_packet, bt_get_rx_data_param->total_bit, bt_get_rx_data_param->error_bit);
        break;
        default:
        break;
    }


    return LS_OK;
}

/** There are 4 kinds of AT cmd
*  (1) AT+<x>=?      (ATCMD_PARAM)  at cmd to get all configurable parameters range
*  (2) AT+<x>?       (ATCMD_QUERY)  at cmd to get informations
*  (3) AT+<x>=<...>  (ATCMD_EXEC)   at cmd with parameters
*  (4) AT+<x>        (ATCMD_EXEC)   at cmd without parameters
**/
const atcmd_item_t atcmd_bt_table[] =
{
    // bt controller
    {atcmd_bleinit,      "AT+BLEINIT",    "init ble env\r\n"
                       "AT+BLEINIT=<init>\r\n"},
    {atcmd_blename,      "AT+BLENAME",    "set ble device name\r\n"
                       "AT+BLENAME=<device_name>\r\n"},
    {atcmd_blescanparam, "AT+BLESCANPARAM",    "set ble scan para\r\n"
                       "AT+BLESCANPARAM=<scan_type>, <filter_policy>, <scan_interval>, <scan_window>\r\n"
                       "<scan_type:0 passive scan 1: active scan>\r\n"
                       "<fiter_policy>:\r\n"
                       "0 BT_LE_SCAN_FILTER_DUPLICATE\r\n"
                       "1 BT_LE_SCAN_FILTER_WHITELIST\r\n"
                       "2 BT_LE_SCAN_FILTER_EXTENDED\r\n"},
    {atcmd_blescan,      "AT+BLESCAN",    "ble scan\r\n"
                       "AT+BLESCAN=<enable>,<interval>,<filter_type>,<filter_param>, rsp is as below:\r\n"
                       "+BLESCAN: <addr>,<event_type>,<rssi>,<name>\r\n"
                       "<enable>: 1 disable continuous scanning, 0: enable continuous scanning\r\n"
                       "<interval>: optional parameter, unit second\r\n"
                       "when disabling the scanning, this parameter should be omitted\r\n"
                       "when enabling the scanning, and <interval> is 0, it means that scanning is continuous\r\n"
                       "when enabling the scanning, and <interval> is NOT 0,for example, command, AT+BLESCAN=0,3,\r\n"
                       "it means that scanning should last for 3 seconds and then stop automatically\r\n"},
    {atcmd_blescanrspdata,      "AT+BLESCANRSPDATA",    "set ble scan rsp data\r\n"
                              "AT+BLESCANRSPDATA=<type>,<data_len>,<scan_rsp_data>\r\n"
                              "<scan_rsp_data>: scan response data is a HEX string\r\n"
                              "exp: AT+BLESCANRSPDATA=0x1, 0x2, 1234\r\n"},
    {atcmd_bleadvparam,  "AT+BLEADVPARAM",    "set ble adv param\r\n"
                       "AT+BLEADVPARAM=<adv_type>,<adv_mode>,<adv_int_min>,<adv_int_max>\r\n"
                       "<adv_type>\r\n"
                       "0: ADV_TYPE_IND\r\n"
                       "1: ADV_TYPE_SCAN_IND\r\n"
                       "2: ADV_TYPE_NONCONN_IND\r\n"
                       "<adv_mode>\r\n"
                       "0: GENER_DISC_MODE\r\n"
                       "1: NON_DISC_MODE\r\n"
                       "2: LIMIT_DISC_MODE\r\n"
                       "<adv_int_min> minimum value of advertising interval, range: 0x0020 ~ 0x4000\r\n"
                       "<adv_int_max> maximum value of advertising interval, range: 0x0020 ~ 0x4000\r\n"},
    {atcmd_bleadvdata,   "AT+BLEADVDATA",    "set ble adv data\r\n"
                       "AT+BLEADVDATA=<type>,<data_len>,<adv_data>\r\n"
                       "<adv_data>: advertising data, this is a HEX string\r\n"
                       "eg: AT+BLEADVDATA = 0x1, 0x1, 3\r\n"},
    {atcmd_bleadvstart,  "AT+BLEADVSTART",    "ble adv start\r\n"
                       "AT+BLEADVSTART\r\n"},
    {atcmd_bleadvstop,   "AT+BLEADVSTOP",    "ble adv stop\r\n"
                       "AT+BLEADVSTOP\r\n"},
    {atcmd_bleconn,      "AT+BLECONN",    "ble connect\r\n"
                       "AT+BLECONN=<addr_type>,<remote_address>,<timeout>\r\n"
                       "<addr_type>:the address type of broadcaster\r\n"
                       "0: ADDR_PUBLIC /*Public BD address*/\r\n"
                       "1: ADDR_RAND /*Random BD address*/\r\n"
                       "2: ADDR_RPA_OR_PUBLIC\r\n"
                       "3: ADDR_RPA_OR_RAND\r\n"
                       "<timeout>: Connect time(ms), hex string\r\n"},
    {atcmd_bleconnparam, "AT+BLECONNPARAM",    "set ble con param\r\n"
                       "AT+BLECONNPARAM=<conn_index>,<min_interval>,<max_interval>,<latency>,<timeout>\r\n"
                       "<conn_index>:index of BLE connection, range[0~10]\r\n"
                       "<min_interval>: minimum value of connecting interval, range: 0x0006 ~ 0x0C80\r\n"
                       "<max_interval>: maximum value of connecting interval, range: 0x0006 ~ 0x0C80\r\n"
                       "<latency>: latency, range: 0x0000 ~ 0x01F3\r\n"
                       "<timeout>: timeout, range: 0x000A ~ 0x0C80\r\n"},
    {atcmd_bledisconn,   "AT+BLEDISCONN",    "ble connection disconnect\r\n"
                       "AT+BLEDISCONN=<addr_type>, <addr>\r\n"
                       "eg: AT+BLEDISCONN=0, 18B905DE97CA\r\n"},
    {atcmd_bledatalen,   "AT+BLEDAATLEN",    "set ble data length\r\n"
                       "AT+BLEDAATLEN=<conn_index>,<pkt_data_len>\r\n"
                       "<conn_index>: index of BLE connection, range: [0~10]\r\n"
                       "<pkt_data_len>: data packet's leghth, range: 0xx001B ~ 0x00FB\r\n"},
    {atcmd_blesecparam,  "AT+BLESECPARAM?",    "set ble sec param\r\n"
                       "rsp is: +BLESECPARAM: <auth_req>,<iocap>,<key_size>,<init_key>,<rsp_key>\r\n"},
    {atcmd_bleenc,       "AT+BLEENC",    "ble enc\r\n"
                       "AT+BLEENC=<conn_index>,<sec_act>\r\n"
                       "<conn_index>: index of BLE connection, range: [0~10]\r\n"
                       "<sec_act>: \r\n"
                       "1: SEC_NONE\r\n"
                       "2: SEC_ENCRYPT\r\n"
                       "3: SEC_ENCRYPT_NO_MIMT\r\n"
                       "4: SEC_ENCRYPT_MIMT\r\n"},
    {atcmd_blekeyreply,  "AT+BLEKEYREPLY",    "ble key reply\r\n"
                       "AT+BLEKEYREPLY=<conn_index>,<key>\r\n"
                       "<conn_index>: index of BLE connection, range: [0~10]\r\n"
                       "<key>: pairing key"},
    {atcmd_bleencdev,    "AT+BLEENCDEV?",    "ble enc dev\r\n"},
    {atcmd_bleencclear,  "AT+BLEENCCLEAR",  "ble enc clear\r\n"
                       "AT+BLEENCCLEAR=<type>,<address>\r\n"
                       "<type>: address type\r\n"
                       "<address>: if set to 0, clear all paired devices\r\n"},
    {atcmd_blenonsignaltx, "AT+BLENONSIGNALTX",    "ble non signal tx\r\n"
                         "AT+BLENONSIGNALTX=<tx_channel>,<data_len>,<pkt_payl>,<phy>,<fhss>\r\n"
                         "<tx_channel>: tx channel,   range: 0x00~0x27\r\n"
                         "<data_len>: tx data length, range: 0x00~0xFF\r\n"
                         "<pkt_payl>:\r\n"
                         "0x00 PRBS9 sequence\r\n"
                         "0x01 Repeated 11110000\r\n"
                         "0x02 Repeated 10101010\r\n"
                         "0x03 PRBS15 sequence\r\n"
                         "0x04 Repeated 11111111\r\n"
                         "0x05 Repeated 00000000\r\n"
                         "0x06 Repeated 00001111\r\n"
                         "0x07 Repeated 01010101\r\n"
                         "0x08 inifinite mode\r\n"
                         "<fhss>:\r\n"
                         "0x01 fhss hopping suppoorted\r\n"
                         "0x00 fhss hopping not supported\r\n"},
    {atcmd_blenonsignalrx, "AT+BLENONSIGNALRX",    "ble non signal rx\r\n"
                         "AT+BLENONSIGNALRX=<rx_channel>,<phy>,<mod_idx>,<infinite_rx_mode>\r\n"
                         "<rx_channel>: rx channel,   range: 0x00~0x27\r\n"
                         "<phy>: 0: 1M 1: 2M 2: coded\r\n"
                         "<mod_idx>: 0: standard 1: stable\r\n"
                         "<infinite_rx_mode>: 1: inifinate rx mode 0: normal rx mode\r\n"},
    {atcmd_blenonsignalend,"AT+BLENONSIGNALEND",    "ble non signal test end\r\n"
                         "rsp is: <nb_pkt_recv>"},
    {atcmd_btinquiry,      "AT+BTINQUIRY",    "bt inquiry\r\n"
                         "AT+BTINQUIRY=<lap>,<inq_len>,<nb_rsp>\r\n"
                         "rsp is: +BTINQUIRY: <addr>,<class_of_dev>,<page_scan_req_mode>,<clk_offset>\r\n"
                         "<lap>: 0x9E8B00 - 0x9E8B3F, normal is 0x9E8B33\r\n"
                         "<inq_len>: inquiry length, range: 0x01(1.28s) ~ 0x30(61.44s), normal is 8(10.24s)\r\n"
                         "<nb_rsp>\r\n"
                         "0x00: unlimited number of responses\r\n"
                         "0xXX: Maximum number of responses from the inquiry before inquiry is halted (0x01 to 0xFF)\r\n"},
    {atcmd_btscan,         "AT+BTSCAN",    "bt scan\r\n"
                         "AT+BTSCAN=<scan_en>\r\n"
                         "<scan_en>\r\n"
                         "0: no scan\r\n"
                         "1: inquiry scan enable\r\n"
                         "2: page scan enable\r\n"
                         "3: both scan enable\r\n"},
    {atcmd_btdutmode,      "AT+BTDUTMODE",    "enter bt dut mode\r\n"
                         "AT+BTDUTMODE=<enable>\r\n"
                         "<enable>: 1 enable 0 disable\r\n"},
    {atcmd_btconn,         "AT+BTCONN",    "bt connect\r\n"
                         "AT+BLECONN=<bd_addr>,<pkt_type>,<page_scan_rep_mode>,<clk_off>,<switch_en>\r\n"
                         "<bd_addr>: remote bd addr\r\n"
                         "<pkt_type>: alow packet type\r\n"
                         "<page_scan_rep_mode>: from inquiry result\r\n"
                         "<>clk_off>: from inquiry result\r\n"
                         "<switch_en>: 1 allow role switch 0 not allow role switch\r\n"},
    {atcmd_btdisconn,      "AT+BTDISCONN",    "bt connection disconnect\r\n"
                         "AT+BTDISCONN=<addr_type>, <addr>\r\n"
                         "eg: AT+BTDISCONN=0, 18B905DE97CA\r\n"},
    {atcmd_btnonsignaltx,  "AT+BTNONSIGNALTX",    "bt non signal tx\r\n"
                         "AT+BTNONSIGNALTX=<pkt_type>,<pkt_len>,<packet_period>,<pattern>,<tx_ch>,<tx_power>,<tx_value>\r\n"
                         "<pkt_type>\r\n"
                         "DM1:0x03\r\n"
                         "DM3:0x0A\r\n"
                         "DM5:0x0E\r\n"
                         "DH1:0x04\r\n"
                         "DH3:0x0B\r\n"
                         "DH5:0x0F\r\n"
                         "2-DH1:0x14\r\n"
                         "2-DH3:0x1A\r\n"
                         "2-DH5:0x1E\r\n"
                         "3-DH1:0x18\r\n"
                         "3-DH3:0x1B\r\n"
                         "3-DH5:0x1F\r\n"
                         "<packet_period>: interv between packets, default is 1\r\n"
                         "<pattern>:\r\n"
                         "0x00 PRBS9 sequence\r\n"
                         "0x01 Repeated 11110000\r\n"
                         "0x02 Repeated 10101010\r\n"
                         "0x03 PRBS15 sequence\r\n"
                         "0x04 Repeated 11111111\r\n"
                         "0x05 Repeated 00000000\r\n"
                         "0x06 Repeated 00001111\r\n"
                         "0x07 Repeated 01010101\r\n"
                         "0x08 inifinite mode\r\n"
                         "<tx_ch>: tx channel, range: 0 ~ 78\r\n"
                         "<tx_power>: 0 index 1 keyword, default is 0\r\n"
                         "<tx_value>: index or  keyword, default is 0\r\n"},
    {atcmd_btnonsignalrx,  "AT+BTNONSIGNALRX",    "bt non signal rx\r\n"
                         "AT+BTNONSIGNALRX=<rx_channel>,<pkt_type>,<peer_addr>,<infinite_rx_mode>\r\n"
                         "<rx_channel>: rx channel,   range: 0~78\r\n"
                         "<pkt_type>: as tx\r\n"
                         "<peer_addr>: remote bd adde\r\n"
                         "<infinite_rx_mode>: 1: inifinate rx mode 0: normal rx mode\r\n"},
    {atcmd_btnonsignaldisable,  "AT+BTNONSIGNALDISABLE",    "bt non signal disable\r\n"},
    {atcmd_btnonsignalrxgetdata,"AT+BTNONSIGNALRXGETDATA",  "bt non signal rx get data\r\n"
                              "rsp is:<total_packets>,<error_packets>,<total_bits>,<error_bits>\r\n"},
    // test tone
    {atcmd_testtonestart,  "AT+TESTTONESTART",    "rf tx test tone\r\n"},
    {atcmd_testtonestop,   "AT+TESTTONESTOP",    "rf stop tet tone\r\n"},
    // bt host
    {atcmd_hbleadvstart,   "AT+ADVSTART",    "host adv start \r\n"
                         "AT+ADVSTART=<mode>\r\n"},
    {atcmd_hbleadvstop,    "AT+ADVSTOP",     "host adv stop \r\n"
                         "rsp is: <status>"},
    {atcmd_bdaddr,         "AT+BDADDR",     "read or write bd addr \r\n"
                           "AT+BDADDR?, read bd addr\r\n"
                           "AT+BDADDR=<bd_addr>, write bd addr"},
};

void atcmd_bt_register(void)
{
    atcmd_entry_add_table(atcmd_bt_table, sizeof(atcmd_bt_table)/sizeof(atcmd_item_t));
}

void atcmd_bt_help(void)
{
    int i;
    int item_len;
    item_len = sizeof(atcmd_bt_table)/sizeof(atcmd_item_t);
    for (i = 0; i < item_len; i++)
      CLOGI("%s: %s\n", atcmd_bt_table[i].atcmd_entry.name, atcmd_bt_table[i].atcmd_entry.help);
}

