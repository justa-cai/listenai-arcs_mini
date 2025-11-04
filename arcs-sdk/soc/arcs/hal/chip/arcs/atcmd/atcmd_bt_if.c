/*
 * bt_at_if.c
 *
 *  bt stack interface functions
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition
#include "bt_config.h"
#include "log_print.h"
#include "nvs.h"

//#include "plf.h"

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"
#include "bt_api.h"
#include "atcmd_bt_if.h"
//#include "bt_stack_if.h"
//#include "bt_classic_if.h"
//#include "bt_ble_if.h"

#include "bt_os_task.h"

/*
 * MACROS
 ****************************************************************************************
 */


/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */


/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

#if (BT_STACK_PRESENT)
extern void bt_classic_scan_enable(uint8_t enable);

#endif

#if BLE_HOST_PRESENT
extern uint8_t app_ble_adv_start(uint8_t adv_id, uint8_t adv_type);
extern uint8_t app_ble_adv_stop(uint8_t adv_id);
extern void ble_gap_disconnect(uint8_t conidx, uint8_t reason);
#endif

#if (BT_EMB_PRESENT)
extern bool lm_dut_mode_en_set(uint8_t enable);
extern int hci_wr_scan_en_cmd_handler(void *param, uint16_t opcode);

extern int hci_nonsig_get_rx_data_cmd_lc_handler(void const *param,  uint16_t opcode);
extern int hci_nonsig_tx_disable_cmd_lc_handler(void const *param,  uint16_t opcode);
extern int hci_nonsig_rx_enable_cmd_lc_handler(void *param,  uint16_t opcode);
extern int hci_nonsig_tx_enable_cmd_lc_handler(void *param,  uint16_t opcode);
extern int hci_create_con_cmd_handler(void *param, uint16_t opcode);
extern int hci_inq_cmd_handler(void *param, uint16_t opcode);
extern uint8_t lm_get_link_id(struct bd_addr *p_bd_addr);
#endif

#if (BLE_EMB_PRESENT)
extern uint8_t lld_test_start(void* params);
extern uint8_t lld_test_stop(void);

extern uint8_t llm_get_link_id(struct bd_addr *p_bd_addr);
extern int hci_le_set_data_len_cmd_handler(uint8_t link_id, void *param, uint16_t opcode);
extern int hci_disconnect_cmd_handler(uint8_t link_id, void *param, uint16_t opcode);
extern int hci_le_con_upd_cmd_handler(uint8_t link_id, void *param, uint16_t opcode);
extern int hci_le_create_con_cmd_handler(void *param, uint16_t opcode);
extern int hci_le_set_adv_en_cmd_handler(void *param, uint16_t opcode);
extern int hci_le_set_adv_data_cmd_handler(void *param, uint16_t opcode);
extern int hci_le_set_adv_param_cmd_handler(void *param, uint16_t opcode);
extern int hci_le_set_scan_en_cmd_handler(void *param, uint16_t opcode);
extern int hci_le_set_scan_param_cmd_handler(void *param, uint16_t opcode);
extern int hci_le_set_scan_rsp_data_cmd_handler(void *param, uint16_t opcode);
#endif

extern void SysTick_Open(uint64_t interval);
extern void SysTick_Close(void);

#define  PARAM_ID_DEVICE_NAME                 (0x02)

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief initialize platform
 *
 *
 ****************************************************************************************
 */
bt_at_cmd_t *atcmd_msg_alloc( btos_event_t *ev, uint32_t size)
{
    ev->msg_body = btos_malloc(sizeof(btos_msg_t)+size);
    ev->msg_body->msg_id = BT_OS_AT_SEND_EVT;
    ev->msg_body->param_len = size;
    return (bt_at_cmd_t *)ev->msg_body->param;
}

uint8_t atcmd_ble_init_send(uint8_t init)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_init send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(uint8_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_COMMON_BLE_INIT;
    at_cmd->data_len = sizeof(uint8_t);
    at_cmd->data[0] = init;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

void atcmd_ble_init_handler(uint8_t init)
{
    CLOGD("at_ble_init_handler,init:%d",init);
    //bt_stack_if_init(init);
}

uint8_t atcmd_blename_send(uint8_t *name)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_name send");
    at_cmd = atcmd_msg_alloc(&ev, strlen(name)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_COMMON_BLE_NAME;
    at_cmd->data_len = strlen(name);
    memcpy(at_cmd->data, name, strlen(name));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

void atcmd_ble_name_handler(uint8_t *name)
{
    uint8_t length = BD_NAME_SIZE;
    CLOGD("at_ble_name_handler");
    #if CFG_NVS
    nvds_put(PARAM_ID_DEVICE_NAME, &length, name);
    #endif
}

uint8_t atcmd_ble_scan_param_send(ble_scan_params_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_scan_param send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_scan_params_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_SCAN_PARAM;
    at_cmd->data_len = sizeof(ble_scan_params_t);

    memcpy(at_cmd->data, params, sizeof(ble_scan_params_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_scan_param_handler(ble_scan_params_t *params)
{ 
    uint8_t status = CO_ERROR_NO_ERROR;

    hci_le_set_scan_param_cmd_handler(params, 0x200B);

    return status;
}

uint8_t atcmd_ble_scan_send(ble_scan_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_scan send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_scan_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_SCAN;
    at_cmd->data_len = sizeof(ble_scan_t);
    memcpy(at_cmd->data, params, sizeof(ble_scan_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

void blescan_disable()
{
    ble_scan_en_t en_config = {0};
    en_config.filter_duplic = 1;
    #if(BLE_EMB_PRESENT)
    hci_le_set_scan_en_cmd_handler(&en_config, 0x200C);
    #else
    atcmd_ble_not_support();
    #endif
}

void blescan_enable()
{
    ble_scan_en_t en_config = {0};
    en_config.scan_en = 1;
    en_config.filter_duplic = 1;
    #if(BLE_EMB_PRESENT)
    hci_le_set_scan_en_cmd_handler(&en_config, 0x200C);
    #else
    atcmd_ble_not_support();
    #endif
}

void SysTick_Handler(void)
{
    blescan_disable();

    SysTick_Close();
}


uint8_t atcmd_ble_scan_handler(ble_scan_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    ///disable conus scanning
    if (params->enable == 1)
    {
        blescan_disable();
    }
    ///enable conus scanning
    else
    {
        /// conus scanning
        if (params->intv == 0)
        {
            blescan_enable();
        }
        else
        {
            blescan_enable();
            SysTick_Open((params->intv) * 1000000);
        }
    }
    return status;
}


uint8_t atcmd_ble_scan_rsp_data_send(ble_scan_rspdata_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_scan_rsp_data send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_scan_rspdata_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_SCAN_RSP_DATA;
    at_cmd->data_len = sizeof(ble_scan_rspdata_t);
    memcpy(at_cmd->data, params, sizeof(ble_scan_rspdata_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_scan_rsp_data_handler(ble_scan_rspdata_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;

    hci_le_set_scan_rsp_data_cmd_handler(params, 0x2009);
    
    return  status;
}

uint8_t atcmd_ble_adv_param_send(ble_adv_param_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_adv_param send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_adv_param_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_ADV_PARAM;
    at_cmd->data_len = sizeof(ble_adv_param_t);
    memcpy(at_cmd->data, params, sizeof(ble_adv_param_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_adv_param_handler(ble_adv_param_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    struct hci_out_le_set_adv_param_cmd cmd = {0};

    cmd.adv_intv_min = params->adv_int_min;
    cmd.adv_intv_max = params->adv_int_max;
    cmd.adv_type     = params->adv_type;
    cmd.adv_chnl_map = 0xFF;

    hci_le_set_adv_param_cmd_handler(&cmd, 0x2006);

    return status;
}

uint8_t atcmd_ble_adv_data_send(ble_adv_data_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_adv_data send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_adv_data_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_ADV_DATA;
    at_cmd->data_len = sizeof(ble_adv_data_t);
    memcpy(at_cmd->data, params, sizeof(ble_adv_data_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_adv_data_handler(ble_adv_data_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;

    hci_le_set_adv_data_cmd_handler(params, 0x2008);

    return status;
}


uint8_t atcmd_ble_adv_start_send(ble_adv_en_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_adv_start send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_adv_en_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_ADV_START;
    at_cmd->data_len = sizeof(ble_adv_en_t);
    memcpy(at_cmd->data, params, sizeof(ble_adv_en_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_adv_start_handler(ble_adv_en_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;

    hci_le_set_adv_en_cmd_handler(params, 0x200A);

    return status;
}

uint8_t atcmd_ble_adv_stop_send(ble_adv_en_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_adv_stop send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_adv_en_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_ADV_STOP;
    at_cmd->data_len = sizeof(ble_adv_en_t);
    memcpy(at_cmd->data, params, sizeof(ble_adv_en_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_adv_stop_handler(ble_adv_en_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;

    hci_le_set_adv_en_cmd_handler(params, 0x200A);

    return status;
}


uint8_t atcmd_ble_conn_send(ble_conn_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_conn send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_conn_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_CONN;
    at_cmd->data_len = sizeof(ble_conn_t);
    memcpy(at_cmd->data, params, sizeof(ble_conn_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_conn_handler(ble_conn_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    struct hci_out_le_create_con_cmd cmd = {0};

    cmd.scan_intv   = SCAN_INTERVAL_DFT;
    cmd.scan_window = SCAN_WINDOW_DFT;
    //cmd.init_filt_policy = 0;
    cmd.peer_addr_type   = params->addr_type;
    memcpy(cmd.peer_addr.addr, params->remote_addr.addr, BD_ADDR_LEN);
    cmd.con_intv_min   = 0x50;
    cmd.con_intv_max   = 0x50;
    //cmd.con_latency    = 0;
    cmd.superv_to      = params->timeout;

    hci_le_create_con_cmd_handler(&cmd, 0x200D);

    return status;
}

uint8_t atcmd_ble_conn_param_send(ble_conn_param_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_conn_param send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_conn_param_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_CONN_PARAM;
    at_cmd->data_len = sizeof(ble_conn_param_t);
    memcpy(at_cmd->data, params, sizeof(ble_conn_param_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_conn_param_handler(ble_conn_param_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;

    hci_le_con_upd_cmd_handler(params->conn_index, params, 0x2013);

    return status;
}


uint8_t atcmd_ble_disconn_send(ble_disconn_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_disconn send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_disconn_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_DISCONN;
    at_cmd->data_len = sizeof(ble_disconn_t);
    memcpy(at_cmd->data, params, sizeof(ble_disconn_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_disconn_handler(ble_disconn_t *params)
{
    hci_disconnect_cmd_t cmd = {0};
    uint8_t link_id = 0;
    uint8_t status = CO_ERROR_NO_ERROR;

    link_id = llm_get_link_id((struct bd_addr *)&(params->remote_addr));
    if (link_id != 0xFF)
    {
        cmd.conhdl = link_id;
        #if BLE_HOST_PRESENT
        ble_gap_disconnect(cmd.conhdl, cmd.reason);
        #else
        hci_disconnect_cmd_handler(link_id, &cmd, 0x0406);
        #endif
    }
    else
    {
        CLOGD("Invalid bd addr");
    }

    return status;
}

uint8_t atcmd_ble_data_len_send(ble_data_len_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_data_len send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_data_len_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BLE_DATA_LEN;
    at_cmd->data_len = sizeof(ble_data_len_t);
    memcpy(at_cmd->data, params, sizeof(ble_data_len_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_data_len_handler(ble_data_len_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    params->tx_time = BLE_MAX_TIME;

    hci_le_set_data_len_cmd_handler(params->conn_index, params, 0x2022);

    return status;
}

uint8_t atcmd_ble_sec_param_send(ble_sec_param_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_sec_param send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_sec_param_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_HOST_BLE_SEC_PARAM;
    at_cmd->data_len = sizeof(ble_sec_param_t);
    memcpy(at_cmd->data, params, sizeof(ble_sec_param_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_sec_param_handler(ble_sec_param_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    return status;
}

uint8_t atcmd_ble_enc_send(ble_enc_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_enc send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_enc_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_HOST_BLE_ENC;
    at_cmd->data_len = sizeof(ble_enc_t);
    memcpy(at_cmd->data, params, sizeof(ble_enc_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_enc_handler(ble_enc_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    return status;
}

uint8_t atcmd_ble_key_reply_send(ble_key_reply_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_key_reply send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_key_reply_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_HOST_BLE_KEY_REPLY;
    at_cmd->data_len = sizeof(ble_key_reply_t);
    memcpy(at_cmd->data, params, sizeof(ble_key_reply_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_key_reply_handler(ble_key_reply_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    return status;
}

uint8_t atcmd_ble_enc_clear_send(ble_enc_clear_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("at_ble_enc_clear send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_enc_clear_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_HOST_BLE_ENC_CLEAR;
    at_cmd->data_len = sizeof(ble_enc_clear_t);
    memcpy(at_cmd->data, params, sizeof(ble_enc_clear_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_enc_clear_handler(ble_enc_clear_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    return status;
}

uint8_t atcmd_ble_nonsignal_tx_send(uint8_t channel, uint8_t data_len, uint8_t payload, uint8_t phy, uint8_t fhss)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("atcmd_ble_nonsignal_tx_send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_test_params_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_TEST_BLE_NONSIGNAL_TX;
    at_cmd->data_len = sizeof(ble_test_params_t);
    ble_test_params_t *params = (ble_test_params_t *)at_cmd->data;

    params->channel = channel;
    params->data_len = data_len;
    params->payload = payload;
    params->phy = phy;
    params->fhss = fhss;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_ble_nonsignal_tx_handler(ble_test_params_t *params)
{
    uint8_t status = 0;
    if (  (params->channel > TEST_FREQ_MAX)
       || (params->payload > PAYL_INIFINITE)
       || (params->phy < TEST_PHY_MIN)
       || (params->phy > TX_TEST_PHY_MAX)
       || (params->fhss > 1))
    {
        status = 0x12;//CO_ERROR_INVALID_HCI_PARAM;
    }
    else
    {
        params->type = 1;
        params->cte_len = NO_CTE;
        params->tx_pwr_lvl = MAX_TX_PWR_LVL;
        #if (BLE_EMB_PRESENT)
        status = lld_test_start(params);
        #else
        CLOGD("Not support");
        #endif
    }
    
    CLOGD("atcmd_ble_nonsignal_tx_handler,sta:0x%x, c:%d,d:%d,pay:%d,phy:%d,fhss:%d",status, params->channel, params->data_len, params->payload, params->phy, params->fhss);
    return status;
}

uint8_t atcmd_ble_nonsignal_rx_send(uint8_t channel, uint8_t phy, uint8_t mod_idx, uint8_t infinite_rx_mode)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("atcmd_ble_nonsignal_rx_send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(ble_test_params_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_TEST_BLE_NONSIGNAL_RX;
    at_cmd->data_len = sizeof(ble_test_params_t);
    ble_test_params_t *params = (ble_test_params_t *)at_cmd->data;

    params->channel = channel;
    params->phy = phy;
    //use payload transport mod_idx!!!.
    params->payload = mod_idx;
    params->infinite_rx_mode = infinite_rx_mode;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

void atcmd_ble_nonsignal_rx_handler(ble_test_params_t *params)
{
    uint8_t status = 0;
    uint8_t mod_idx = params->payload;

    if ((params->channel > TEST_FREQ_MAX)
        || (params->phy < TEST_PHY_MIN)
        || (params->phy > RX_TEST_PHY_MAX)
        || (mod_idx > 0x01))
    {
        status = 0x12;//CO_ERROR_INVALID_HCI_PARAM;
    }
    else
    {
        params->type = 0;
        params->cte_len = NO_CTE;
        status = lld_test_start(params);
    }
    CLOGD("atcmd_ble_nonsignal_rx_handler,sta:0x%x,c:%d,phy:%d,mod_idx:%d,rx_mode:%d",status, params->channel, params->phy, mod_idx, params->infinite_rx_mode);
}

uint8_t atcmd_ble_nonsignal_end_send(void)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("ble_nonsignal_end send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_TEST_BLE_NONSIGNAL_END;
    at_cmd->data_len = 0;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

void atcmd_ble_nonsignal_end_handler(void)
{
    uint8_t status = lld_test_stop();

    CLOGD("atcmd_ble_nonsignal_end_handler,sta:0x%x",status);
}

uint8_t atcmd_bt_scan_send(uint8_t enable)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("bt_scan send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(uint8_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BT_SCAN;
    at_cmd->data_len = sizeof(uint8_t);
    at_cmd->data[0] = enable;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_bt_scan_handler(uint8_t enable)
{
    CLOGD("atcmd_bt_scan_handler,enable:%d",enable);
    uint8_t status = CO_ERROR_NO_ERROR;
    ble_test_scan_en_cmd_t scan;

    #if (BT_STACK_PRESENT)
    bt_classic_scan_enable(enable);
    #else
    CLOGD("Not support");
    #endif
    //scan.scan_en = enable;
    //hci_wr_scan_en_cmd_handler(&scan,0x0C1A);

    return status;
}

uint8_t atcmd_bt_inquiry_send(bt_inq_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;
    at_cmd = atcmd_msg_alloc(&ev, sizeof(bt_inq_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BT_INQUIRY;
    at_cmd->data_len = sizeof(bt_inq_t);

    memcpy(at_cmd->data, params, sizeof(bt_inq_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_bt_inquiry_handler(bt_inq_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    #if (BT_EMB_PRESENT)
    hci_inq_cmd_handler(params, 0x0401);
    #else
    CLOGD("Not support");
    #endif

    return status;
}

uint8_t atcmd_bt_conn_send(bt_conn_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("bt_conn send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(bt_conn_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BT_CONN;
    at_cmd->data_len = sizeof(bt_conn_t);
    memcpy(at_cmd->data, params, sizeof(bt_conn_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_bt_conn_handler(bt_conn_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    #if (BT_EMB_PRESENT)
    hci_create_con_cmd_handler(params, 0x0405);
    #else
    CLOGI("cmd not supported");
    #endif

    return status;
}


uint8_t atcmd_bt_disconn_send(bt_disconn_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("bt_disconn send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(bt_disconn_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_CONTROLLER_BT_DISCONN;
    at_cmd->data_len = sizeof(bt_disconn_t);
    memcpy(at_cmd->data, params, sizeof(bt_disconn_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint8_t atcmd_bt_disconn_handler(bt_disconn_t *params)
{
    uint8_t link_id = 0;
    hci_disconnect_cmd_t cmd = {0};
    uint8_t status = CO_ERROR_NO_ERROR;

    #if (BT_EMB_PRESENT)
    link_id = lm_get_link_id((struct bd_addr *)&(params->remote_addr));
    if (link_id != 0xFF)
    {
        cmd.conhdl = link_id + 0x80;
        hci_disconnect_cmd_handler(link_id, &cmd, 0x0406);
    }
    else
    {
         CLOGI("Invalid bd_addr");
    }
    #else
    CLOGI("cmd not supported");
    #endif
    return status;
}



uint8_t atcmd_bt_non_signal_tx_send(bt_non_signal_tx_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("bt_non_signal_tx send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(bt_non_signal_tx_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_TEST_BT_NONSIGNAL_TX;
    at_cmd->data_len = sizeof(bt_non_signal_tx_t);
    memcpy(at_cmd->data, params, sizeof(bt_non_signal_tx_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}


uint8_t atcmd_bt_non_signal_tx_handler(bt_non_signal_tx_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    #if (BT_EMB_PRESENT)
    hci_nonsig_tx_enable_cmd_lc_handler(params, 0XFC70);
    #else
    CLOGI("cmd not supported");
    #endif

    return status;
}

uint8_t atcmd_bt_non_signal_rx_send(bt_non_signal_rx_t *params)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("bt_non_signal_rx send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(bt_non_signal_rx_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_TEST_BT_NONSIGNAL_RX;
    at_cmd->data_len = sizeof(bt_non_signal_rx_t);
    memcpy(at_cmd->data, params, sizeof(bt_non_signal_rx_t));

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}


uint8_t atcmd_bt_non_signal_rx_handler(bt_non_signal_rx_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    #if (BT_EMB_PRESENT)
    hci_nonsig_rx_enable_cmd_lc_handler(params, 0XFC72);;
    #else
    CLOGI("cmd not supported");
    #endif

    return status;
}


uint8_t atcmd_bt_non_signal_disable_send(void)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("bt_non_signal_disable send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(uint8_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_TEST_BT_NONSIGNAL_DIS;
    at_cmd->data_len = sizeof(uint8_t);

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}


uint8_t atcmd_bt_non_signal_disable_handler()
{
    uint8_t status = CO_ERROR_NO_ERROR;
    #if (BT_EMB_PRESENT)
    hci_nonsig_tx_disable_cmd_lc_handler(NULL, 0XFC71);
    #else
    CLOGI("cmd not supported");
    #endif
    return status;
}


uint8_t atcmd_bt_non_signal_rx_get_data_send(void)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("bt_non_signal_rx_get_data send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(uint8_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_TEST_BT_NONSIGNAL_RX_GET_DATA;
    at_cmd->data_len = sizeof(uint8_t);

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}


uint8_t atcmd_bt_non_signal_rx_get_data_handler()
{
    uint8_t status = CO_ERROR_NO_ERROR;
    #if (BT_EMB_PRESENT)
    hci_nonsig_get_rx_data_cmd_lc_handler(NULL, 0XFC73);
    #else
    CLOGI("cmd not supported");
    #endif

    return status;
}

uint8_t atcmd_bt_dutmode_send(uint8_t enable)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("bt_dutmode send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(uint8_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_TEST_BT_DUT_MODE;
    at_cmd->data_len = sizeof(uint8_t);
    at_cmd->data[0] = enable;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint32_t atcmd_bt_dutmode_handler(uint8_t enable)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    CLOGD("atcmd_bt_dutmode_handler,enable:%d",enable);
#if BT_EMB_PRESENT
    lm_dut_mode_en_set(enable);
#else
    CLOGD("Not support");
#endif
    return status;
}


/// host
uint8_t atcmd_hble_adv_start_send(uint8_t modes)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("hble_adv_start send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(uint8_t)+sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_HOST_BLE_ADV_START;
    at_cmd->data_len = sizeof(uint8_t);
    at_cmd->data[0] = modes;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

void atcmd_hble_adv_start_handler(uint8_t modes)
{
    CLOGD("atcmd_hble_adv_start_handler,modes:%d",modes);
    #if (BLE_HOST_PRESENT)
    app_ble_adv_start(GAP_ADV_ID_0, modes);
    #endif

}

uint8_t atcmd_hble_adv_stop_send(void)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("hble_adv_stop send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_HOST_BLE_ADV_STOP;
    at_cmd->data_len = 0;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

void atcmd_hble_adv_stop_handler(void)
{
    #if (BLE_HOST_PRESENT)
    app_ble_adv_stop(GAP_ADV_ID_0);
    #endif

    CLOGD("atcmd_hble_adv_stop_handler");
}

uint8_t atcmd_rf_test_tone_start_send(uint16_t channel, uint8_t power)
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("atcmd_rf_test_tone_start_send, channel:%d, power:%d", channel , power);
    at_cmd = (bt_at_cmd_t *)atcmd_msg_alloc(&ev, sizeof(bt_at_cmd_t)+sizeof(rf_test_tone_start_cmd_t));

    at_cmd->at_id = BT_AT_RF_TEST_TONE_START_CMD;
    at_cmd->data_len = sizeof(rf_test_tone_start_cmd_t);
    rf_test_tone_start_cmd_t *params = (rf_test_tone_start_cmd_t *)at_cmd->data;
    params->channel = channel;
    params->power = power;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint32_t atcmd_rf_test_tone_start_handler(rf_test_tone_start_cmd_t *params)
{
    uint8_t status = CO_ERROR_NO_ERROR;
    CLOGD("atcmd_rf_test_tone_start_handler, channel:%d, power:%d", params->channel, params->power);

    rf_start_test_tone(params->channel, params->power);

    return status;
}

uint8_t atcmd_rf_test_tone_stop_send()
{
    btos_event_t ev;
    bt_at_cmd_t *at_cmd;

    CLOGD("atcmd_rf_test_tone_stop_send");
    at_cmd = atcmd_msg_alloc(&ev, sizeof(bt_at_cmd_t));

    at_cmd->at_id = BT_AT_RF_TEST_TONE_STOP_CMD;
    at_cmd->data_len = 0;

    return btos_send_event(OS_TASK_ID_BT, &ev, BTOS_TASK_MAX_DELAY);
}

uint32_t atcmd_rf_test_tone_stop_handler()
{
    uint8_t status = CO_ERROR_NO_ERROR;
    CLOGD("atcmd_rf_test_tone_stop_handler");

    rf_stop_test_tone();

    return status;
}

void bt_at_cmd_msg_handle(bt_at_cmd_t* msg)
{
    bt_at_cmd_t *at_cmd = msg;
    
    CLOGD("bt_at_cmd_msg_handle,id:0x%x", at_cmd->at_id);
    if(at_cmd)
    {
        switch(at_cmd->at_id)
        {
            /// controller
            case BT_AT_COMMON_BLE_INIT :
            {
                atcmd_ble_init_handler(at_cmd->data[0]);
            }break;
            case BT_AT_COMMON_BLE_NAME:
            {
                atcmd_ble_name_handler(at_cmd->data);
            }break;
            case BT_AT_CONTROLLER_BLE_SCAN_PARAM:
            {
                ble_scan_params_t *params = (ble_scan_params_t *)at_cmd->data;
                atcmd_ble_scan_param_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_SCAN:
            {
                ble_scan_t *params = (ble_scan_t *)at_cmd->data;
                atcmd_ble_scan_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_SCAN_RSP_DATA:
            {
                ble_scan_rspdata_t *params = (ble_scan_rspdata_t *)at_cmd->data;
                atcmd_ble_scan_rsp_data_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_ADV_PARAM:
            {
                ble_adv_param_t *params = (ble_adv_param_t *)at_cmd->data;
                atcmd_ble_adv_param_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_ADV_DATA:
            {
                ble_adv_data_t *params = (ble_adv_data_t *)at_cmd->data;
                atcmd_ble_adv_data_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_ADV_START:
            {
                ble_adv_en_t *params = (ble_adv_en_t *)at_cmd->data;
                atcmd_ble_adv_start_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_ADV_STOP:
            {
                ble_adv_en_t *params = (ble_adv_en_t *)at_cmd->data;
                atcmd_ble_adv_stop_handler(params);
            }
            case BT_AT_CONTROLLER_BLE_CONN:
            {
                ble_conn_t *params = (ble_conn_t *)at_cmd->data;
                atcmd_ble_conn_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_CONN_PARAM:
            {
                ble_conn_param_t *params = (ble_conn_param_t *)at_cmd->data;
                atcmd_ble_conn_param_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_DISCONN:
            {
                ble_disconn_t *params = (ble_disconn_t *)at_cmd->data;
                atcmd_ble_disconn_handler(params);
            }break;
            case BT_AT_CONTROLLER_BLE_DATA_LEN:
            {
                ble_data_len_t *params = (ble_data_len_t *)at_cmd->data;
                atcmd_ble_data_len_handler(params);
            }break;
            case BT_AT_TEST_BLE_NONSIGNAL_TX:
            {
                ble_test_params_t *params = (ble_test_params_t *)at_cmd->data;
                atcmd_ble_nonsignal_tx_handler(params);
            }break;
            case BT_AT_TEST_BLE_NONSIGNAL_RX:
            {
                ble_test_params_t *params = (ble_test_params_t *)at_cmd->data;
                atcmd_ble_nonsignal_rx_handler(params);
            }break;
            case BT_AT_TEST_BLE_NONSIGNAL_END:
            {
                atcmd_ble_nonsignal_end_handler();
            }break;
            case BT_AT_CONTROLLER_BT_INQUIRY:
            {
                bt_inq_t *params = (bt_inq_t *)at_cmd->data;
                atcmd_bt_inquiry_handler(params);
            }break;
            case BT_AT_CONTROLLER_BT_SCAN:
            {
                atcmd_bt_scan_handler(at_cmd->data[0]);
            }break;
            case BT_AT_CONTROLLER_BT_CONN:
            {
                bt_conn_t *params = (bt_conn_t *)at_cmd->data;
                atcmd_bt_conn_handler(params);
            }break;
            case BT_AT_CONTROLLER_BT_DISCONN:
            {
                bt_disconn_t *params = (bt_disconn_t *)at_cmd->data;
                atcmd_bt_disconn_handler(params);
            }break;
            case BT_AT_TEST_BT_DUT_MODE:
            {
                atcmd_bt_dutmode_handler(at_cmd->data[0]);
            }break;
            case BT_AT_TEST_BT_NONSIGNAL_TX:
            {
                bt_non_signal_tx_t *params = (bt_non_signal_tx_t *)at_cmd->data;
                atcmd_bt_non_signal_tx_handler(params);
            }break;
            case BT_AT_TEST_BT_NONSIGNAL_RX:
            {
                bt_non_signal_rx_t *params = (bt_non_signal_rx_t *)at_cmd->data;
                atcmd_bt_non_signal_rx_handler(params);
            }break;
            case BT_AT_TEST_BT_NONSIGNAL_DIS:
            {
                atcmd_bt_non_signal_disable_handler();
            }break;
            case BT_AT_TEST_BT_NONSIGNAL_RX_GET_DATA:
            {
                atcmd_bt_non_signal_rx_get_data_handler();
            }break;
            // test tone
            case BT_AT_RF_TEST_TONE_START_CMD:
            {
                rf_test_tone_start_cmd_t *params = (rf_test_tone_start_cmd_t *)at_cmd->data;
                atcmd_rf_test_tone_start_handler(params);
            }break;
            case BT_AT_RF_TEST_TONE_STOP_CMD:
            {
                atcmd_rf_test_tone_stop_handler();
            }break;

            /// host
            case BT_AT_HOST_BLE_ADV_START:
            {
                atcmd_hble_adv_start_handler(at_cmd->data[0]);
            }break;
            case BT_AT_HOST_BLE_ADV_STOP:
            {
                atcmd_hble_adv_stop_handler();
            }break;
            case BT_AT_HOST_BLE_SEC_PARAM:
            {
                ble_sec_param_t *params = (ble_sec_param_t *)at_cmd->data;
                atcmd_ble_sec_param_handler(params);
            }break;
            case BT_AT_HOST_BLE_ENC:
            {
                ble_enc_t *params = (ble_enc_t *)at_cmd->data;
                atcmd_ble_enc_handler(params);
            }break;
            case BT_AT_HOST_BLE_KEY_REPLY:
            {
                ble_key_reply_t *params = (ble_key_reply_t *)at_cmd->data;
                atcmd_ble_key_reply_handler(params);
            }break;
            case BT_AT_HOST_BLE_ENC_CLEAR:
            {
                ble_enc_clear_t *params = (ble_enc_clear_t *)at_cmd->data;
                atcmd_ble_enc_clear_handler(params);
            }break;
            default:
            {
                CLOGD("bt at cm if err,id:%d", at_cmd->at_id);
            }break;
        }
    }
    else
    {
        CLOGD("bt at cm if err,cmd:%x", at_cmd);
    }
}

