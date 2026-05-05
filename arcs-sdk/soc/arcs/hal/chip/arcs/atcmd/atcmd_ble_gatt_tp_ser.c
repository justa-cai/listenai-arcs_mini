/**
****************************************************************************************
*
* @file tps2.c
*
* @brief BLE TP Service source
*
* Copyright (C) ListenAI 2020-2099
*
*
****************************************************************************************
*/

/*
 * MACROS
 ****************************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ble_gatt.h"
#include "ble_prf.h"
#include "log_print.h"
#include "ble_task.h"
#include "atcmd_ble_gatt_tp_ser.h"
#include "atcmd.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "rtos_al.h"
/*
 * DEFINES
 ****************************************************************************************
 */

/// TP BLE GATT UUIDs
#define BLE_GATT_TP_PRIMARY_SERVICE    (0xfd10)
#define BLE_GATT_TP_WC                 (0xfd11)
#define BLE_GATT_TP_NOTIFY             (0xfd12)
#define BLE_GATT_TP_WR                 (0xfd13)
#define BLE_GATT_TP_INDICATE           (0xfd14)
#define BLE_GATT_TP_READ               (0xfd15)
#define BLE_GATT_TP_CTRL               (0xfd16)
#define min(x,y)    ((x)>(y)?(y):(x))

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
/// TP Service state
enum
{
    LE_TP_IDEL,
    LE_TP_READY,
    LE_TP_WAIT_REBOOT,
};

/// TP Service Attributes Index
enum
{
    /// service
    TP_IDX_SVC,

    /// TP Write no response
    TP_IDX_TP_WC_CHAR,
    TP_IDX_TP_WC_VAL,

    /// TP Notify
    TP_IDX_TP_NOTIFY_CHAR,
    TP_IDX_TP_NOTIFY_VAL,
    TP_IDX_TP_NOTIFY_NTF_CFG,

    /// TP Write
    TP_IDX_TP_WR_CHAR,
    TP_IDX_TP_WR_VAL,

    /// TP Indicate
    TP_IDX_TP_INDICATE_CHAR,
    TP_IDX_TP_INDICATE_VAL,
    TP_IDX_TP_INDICATE_IND_CFG,

    /// TP Read
    TP_IDX_TP_READ_CHAR,
    TP_IDX_TP_READ_VAL,

    /// TP Control Point
    TP_IDX_TP_CTRL_CHAR,
    TP_IDX_TP_CTRL_VAL,

    TP_IDX_NB,
};


/// tp service environment variable
typedef struct tps_env
{
    /// service state
    uint8_t state;

    /// GATT user local identifier
    uint8_t user_lid;

    /// HIDS Start Handles
    uint16_t start_hdl;

    /// Notification configuration
    uint16_t ntf_cfg[BLE_CONNECTION_MAX];

    /// tp data buffer
    uint32_t *buff;

} tps_env_t;

typedef struct tp_ctrl_rsp
{
    uint32_t recv_bytes;
    uint32_t send_bytes;
    uint32_t recv_pa;
    uint32_t send_pa;
    uint8_t expand_len;
} tp_ctrl_rsp_t;

tp_ctrl_rsp_t tp_wc_ctrl_rsp = {0};
tp_ctrl_rsp_t tp_wr_ctrl_rsp = {0};
tp_ctrl_rsp_t tp_notify_ctrl_rsp = {0};
tp_ctrl_rsp_t tp_indicate_ctrl_rsp = {0};
tp_ctrl_rsp_t tp_read_ctrl_rsp = {0};
tp_ctrl_rsp_t *tp_ctrl_read_rsp = &tp_read_ctrl_rsp;
uint8_t g_tp_test_mode = 0;
void* g_tp_send_payload_buff_ptr = NULL;
uint16_t g_tx_mtu = 200;
static TickType_t g_tp_test_start_tick = 0;
static uint32_t g_tp_prev_bytes = 0;
uint8_t g_file_transfer_in_progress = 0;
uint8_t g_file_recv_buf[200];
/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// tp service database description
const ble_gatt_att16_desc_t tp_att_db[TP_IDX_NB] =
{
    /// TP service Declaration
    [TP_IDX_SVC]                                = {BLE_GATT_DECL_PRIMARY_SERVICE,       BLE_PROP(RD),                           0                                           },

    /// TP Write no response
    [TP_IDX_TP_WC_CHAR]                         = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    [TP_IDX_TP_WC_VAL]                          = {BLE_GATT_TP_WC,                      BLE_PROP(WC),                           TP_DATA_MAX_LEN                            },

    /// TP Notify
    [TP_IDX_TP_NOTIFY_CHAR]                     = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    [TP_IDX_TP_NOTIFY_VAL]                      = {BLE_GATT_TP_NOTIFY,                  BLE_PROP(N),                            TP_DATA_MAX_LEN                            },
    [TP_IDX_TP_NOTIFY_NTF_CFG]                  = {BLE_GATT_DESC_CLIENT_CHAR_CFG,       BLE_PROP(RD)|BLE_PROP(WR),              BLE_OPT(NO_OFFSET)                          },

    /// TP Write
    [TP_IDX_TP_WR_CHAR]                         = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    [TP_IDX_TP_WR_VAL]                          = {BLE_GATT_TP_WR,                      BLE_PROP(WR),                           TP_DATA_MAX_LEN                            },

    /// TP Indicate
    [TP_IDX_TP_INDICATE_CHAR]                   = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    [TP_IDX_TP_INDICATE_VAL]                    = {BLE_GATT_TP_INDICATE,                BLE_PROP(I),                            TP_DATA_MAX_LEN                            },
    [TP_IDX_TP_INDICATE_IND_CFG]                = {BLE_GATT_DESC_CLIENT_CHAR_CFG,       BLE_PROP(RD)|BLE_PROP(WR),              BLE_OPT(NO_OFFSET)                          },

    /// TP Read
    [TP_IDX_TP_READ_CHAR]                       = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    [TP_IDX_TP_READ_VAL]                        = {BLE_GATT_TP_READ,                    BLE_PROP(RD),                           TP_DATA_MAX_LEN                            },

    /// TP Control Point
    [TP_IDX_TP_CTRL_CHAR]                       = {BLE_GATT_DECL_CHARACTERISTIC,        BLE_PROP(RD),                           0                                           },
    [TP_IDX_TP_CTRL_VAL]                        = {BLE_GATT_TP_CTRL,                    BLE_PROP(WR)|BLE_PROP(RD),              TP_DATA_MAX_LEN                            },
};

typedef struct tp_throughput_record
{
    uint32_t tp_recv_wr_val_bytes;
    uint32_t tp_recv_wc_val_bytes;
    uint32_t tp_send_ntf_bytes;
    uint32_t tp_send_ind_bytes;
} tp_throughput_record_t;

static tp_throughput_record_t g_tp_throughput_record = {0};

/// tp service environment
static tps_env_t tps_env;

/* Throughput task definitions */
#define TPS_THROUGHPUT_TASK_STACK_SIZE    1024
#define TPS_THROUGHPUT_TASK_PRIO          1

static TaskHandle_t tps_throughput_task_handle = NULL;
#include "atcmd_ble_gap.h"
extern uint8_t g_ble_conn_status;
static void tps_throughput_task(void *param)
{
    (void)param;

    for (;;)
    {
        uint32_t cur_bytes = 0;
        if(g_ble_conn_status != BLE_CONN_STATUS_CONNECT &&
           g_ble_conn_status != BLE_CONN_STATUS_CONNECT_PAIRED) {
            /* Not connected: reset prev and sleep */
            g_tp_prev_bytes = 0;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        switch (g_tp_test_mode)
        {
            case TP_TEST_MODE_WC:
                cur_bytes = tp_wc_ctrl_rsp.recv_bytes;
                break;
            case TP_TEST_MODE_WR:
                cur_bytes = tp_wr_ctrl_rsp.recv_bytes;
                break;
            case TP_TEST_MODE_NOTIFY:
                cur_bytes = tp_notify_ctrl_rsp.send_bytes;
                break;
            case TP_TEST_MODE_INDICATE:
                cur_bytes = tp_indicate_ctrl_rsp.send_bytes;
                break;
            case TP_TEST_MODE_READ:
                /* For read mode, consider both sent and received bytes */
                cur_bytes = tp_read_ctrl_rsp.send_bytes + tp_read_ctrl_rsp.recv_bytes;
                break;
            default:
                /* Not in a test mode: reset prev and sleep */
                g_tp_prev_bytes = 0;
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
        }

        /* Calculate average rate from start time to now (B/s) */
        uint32_t avg_bps = 0;
        if (g_tp_test_start_tick != 0)
        {
            TickType_t ticks = xTaskGetTickCount() - g_tp_test_start_tick;
            uint32_t elapsed_s = 0;
#ifdef configTICK_RATE_HZ
            elapsed_s = (uint32_t)ticks / (uint32_t)configTICK_RATE_HZ;
#else
            elapsed_s = (uint32_t)ticks / 1000u; /* fallback, unlikely */
#endif
            if (elapsed_s == 0) elapsed_s = 1; /* avoid div0 for very short durations */
            avg_bps = cur_bytes / elapsed_s;
        }

        /* Calculate bytes in the last 1 second (delta) using global prev */
        uint32_t delta = 0;
        if (cur_bytes >= g_tp_prev_bytes)
            delta = cur_bytes - g_tp_prev_bytes;
        else
            delta = cur_bytes; /* wrapped or reset */

        /* update global prev for next interval */
        g_tp_prev_bytes = cur_bytes;

        /* Only print when a test is running and a start tick is set */
        if ((g_tp_test_mode != 0) && (g_tp_test_start_tick != 0)) {
            /* Convert to KB with one decimal place (x10 integer to avoid float)
             * Use rounding: (val*10 + 512) / 1024  -> nearest 0.1 KB
             */
            uint32_t last_kb_x10 = (delta * 10 + 512) / 1024u;
            uint32_t avg_kb_x10 = (avg_bps * 10 + 512) / 1024u;
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ALWAYS,
                       "TP Throughput - mode=%d total=%u bytes, last=%u.%u KB/s, avg=%u.%u KB/s",
                       g_tp_test_mode, cur_bytes,
                       last_kb_x10 / 10, last_kb_x10 % 10,
                       avg_kb_x10 / 10, avg_kb_x10 % 10);

            /* AT response line for external parser (last and avg as X.Y) */
            char atbuf[64];
            snprintf(atbuf, sizeof(atbuf), "+TPTP:%d,%u,%u.%u,%u.%u",
                     g_tp_test_mode, cur_bytes,
                     last_kb_x10 / 10, last_kb_x10 % 10,
                     avg_kb_x10 / 10, avg_kb_x10 % 10);
            atcmd_rspinfor("%s\r\n", atbuf);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* Should never reach here */
    vTaskDelete(NULL);
}

static int tps_start_throughput_task(void)
{
    if (tps_throughput_task_handle == NULL)
    {
        if (xTaskCreate(tps_throughput_task,
                        ((const char*)"tps_throughput_task"),
                        TPS_THROUGHPUT_TASK_STACK_SIZE,
                        NULL,
                        TPS_THROUGHPUT_TASK_PRIO,
                        &tps_throughput_task_handle) != pdPASS)
        {
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ERROR,
                       "ERROR: Create tps throughput task failed.");
            return -1;
        }
    }
    return 0;
}

/* Print final statistics for a given test mode */
static void tps_print_final_stats(uint8_t mode)
{
    uint32_t cur_bytes = 0;
    uint32_t duration_s = 0;
    uint32_t avg_bps = 0;
    uint32_t last_bytes = 0;

    switch (mode)
    {
        case TP_TEST_MODE_WC:
            cur_bytes = tp_wc_ctrl_rsp.recv_bytes;
            break;
        case TP_TEST_MODE_WR:
            cur_bytes = tp_wr_ctrl_rsp.recv_bytes;
            break;
        case TP_TEST_MODE_NOTIFY:
            cur_bytes = tp_notify_ctrl_rsp.send_bytes;
            break;
        case TP_TEST_MODE_INDICATE:
            cur_bytes = tp_indicate_ctrl_rsp.send_bytes;
            break;
        case TP_TEST_MODE_READ:
            cur_bytes = tp_read_ctrl_rsp.send_bytes + tp_read_ctrl_rsp.recv_bytes;
            break;
        default:
            return;
    }

    if (g_tp_test_start_tick != 0)
    {
        TickType_t ticks = xTaskGetTickCount() - g_tp_test_start_tick;
#ifdef configTICK_RATE_HZ
        duration_s = (uint32_t)ticks / (uint32_t)configTICK_RATE_HZ;
#else
        duration_s = (uint32_t)ticks / 1000u;
#endif
        if (duration_s == 0) duration_s = 1;
        avg_bps = cur_bytes / duration_s;
    }
    /* last_bytes: bytes in the last measured interval (from global prev) */
    if (cur_bytes >= g_tp_prev_bytes)
        last_bytes = cur_bytes - g_tp_prev_bytes;
    else
        last_bytes = cur_bytes;

    /* Convert to KB with one decimal place (x10 integer to avoid float)
     * rounding: (val*10 + 512) / 1024
     */
    uint32_t last_kb_x10 = (last_bytes * 10 + 512) / 1024u;
    uint32_t avg_kb_x10 = (avg_bps * 10 + 512) / 1024u;

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ALWAYS,
               "TP Final - mode=%d total=%u bytes, duration=%u s, avg=%u.%u KB/s, last=%u.%u KB/s",
               mode, cur_bytes, duration_s,
               avg_kb_x10 / 10, avg_kb_x10 % 10,
               last_kb_x10 / 10, last_kb_x10 % 10);

    /* AT response with total (bytes), last and avg in KB with one decimal */
    char atbuf[64];
    snprintf(atbuf, sizeof(atbuf), "+TPTP:%d,%u,%u.%u,%u.%u",
             mode, cur_bytes,
             last_kb_x10 / 10, last_kb_x10 % 10,
             avg_kb_x10 / 10, avg_kb_x10 % 10);
    atcmd_rspinfor("%s\r\n", atbuf);
}

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void tps_send_notify(uint8_t conidx, uint16_t len, uint8_t* data)
{
    if((tps_env.ntf_cfg[conidx] & BLE_PRF_CLI_START_NTF) == 0)
    {
        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_WARNING, "tps_send_notify: notify not enabled for conidx=%d", conidx);
        return;
    }
    // send notify
    ble_gatt_srv_event_send(conidx, tps_env.user_lid, 0,
                            BLE_GATT_NOTIFY, (tps_env.start_hdl + TP_IDX_TP_NOTIFY_VAL), data, len);
}

void tps_send_indicate(uint8_t conidx, uint16_t len, uint8_t* data)
{
    if((tps_env.ntf_cfg[conidx] & BLE_PRF_CLI_START_IND) == 0)
    {
        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_WARNING, "tps_send_indicate: indicate not enabled for conidx=%d", conidx);
        return;
    }
    // send indicate
    ble_gatt_srv_event_send(conidx, tps_env.user_lid, 0,
                            BLE_GATT_INDICATE, (tps_env.start_hdl + TP_IDX_TP_INDICATE_VAL), data, len);
}

static void tps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tps_cb_event_sent: conidx=%d, status=%d", conidx, status);
    if (g_tp_test_mode == TP_TEST_MODE_NOTIFY) {
        tp_notify_ctrl_rsp.send_bytes += g_tx_mtu;
        tp_notify_ctrl_rsp.send_pa ++;
        tps_send_notify(conidx, g_tx_mtu, g_tp_send_payload_buff_ptr); // 继续发送下一个通知
    } else if (g_tp_test_mode == TP_TEST_MODE_INDICATE) {
        tp_indicate_ctrl_rsp.send_bytes += g_tx_mtu;
        tp_indicate_ctrl_rsp.send_pa ++;
        tps_send_indicate(conidx, g_tx_mtu, g_tp_send_payload_buff_ptr); // 继续发送下一个指示
    }
}

void tp2_process_command(void* cmd_buff, uint16_t length)
{
// 控制指令协议构建函数
// 协议格式（小端序）：
// 命令类型（1字节）：0x01=启动, 0x02=停止, 0x03=读取统计
// 测试模式（1字节）：0x01=write_cmd, 0x02=write_req, 0x03=notify, 0x04=indicate, 0x05=read
// 负载长度（2字节）：用于write模式，表示每次写入的字节数
// 负载模式（1字节）：用于write模式，表示负载填充模式
// 扩展长度（1字节）：扩展数据长度（0-255）
    uint8_t  cmd_type;
    uint8_t  test_mode;
    uint16_t payload_len;
    uint8_t  payload_mode;
    uint8_t  expand_len;

    if(length < 5)
    {
        AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ERROR, "tp2_process_command: invalid length=%d", length);
        rtos_free(cmd_buff);
        return;
    }

    uint8_t* buff = (uint8_t*)cmd_buff;
    cmd_type = buff[0];
    test_mode = buff[1];
    payload_len = ble_co_read16p(&buff[2]);
    payload_mode = buff[4];
    expand_len = buff[5];

    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tp2_process_command: cmd_type=%d, test_mode=%d, payload_len=%d, payload_mode=%d, expand_len=%d",
                cmd_type, test_mode, payload_len, payload_mode, expand_len);
    switch (test_mode)
    {
        case TP_TEST_MODE_WC:
            
            if (cmd_type == TP_CMD_TYPE_START) {
                g_tp_test_mode = TP_TEST_MODE_WC;
                tp_wc_ctrl_rsp.recv_bytes = 0;
                tp_wc_ctrl_rsp.recv_pa = 0;
                g_tp_test_start_tick = xTaskGetTickCount();
                g_tp_prev_bytes = 0;
            } else if (cmd_type == TP_CMD_TYPE_STOP) {
                /* print final stats then stop */
                tps_print_final_stats(TP_TEST_MODE_WC);
                g_tp_test_mode = 0;
                tp_ctrl_read_rsp = &tp_wc_ctrl_rsp;
                g_tp_test_start_tick = 0;
                g_tp_prev_bytes = 0;
            }
            break;

        case TP_TEST_MODE_WR:
            
            if (cmd_type == TP_CMD_TYPE_START) {
                g_tp_test_mode = TP_TEST_MODE_WR;
                tp_wr_ctrl_rsp.recv_bytes = 0;
                tp_wr_ctrl_rsp.recv_pa = 0;
                g_tp_test_start_tick = xTaskGetTickCount();
                g_tp_prev_bytes = 0;
            } else if (cmd_type == TP_CMD_TYPE_STOP) {
                tps_print_final_stats(TP_TEST_MODE_WR);
                g_tp_test_mode = 0;
                tp_ctrl_read_rsp = &tp_wr_ctrl_rsp;
                g_tp_test_start_tick = 0;
                g_tp_prev_bytes = 0;
            }
            break;

        case TP_TEST_MODE_NOTIFY:
            
            if (cmd_type == TP_CMD_TYPE_START) {
                g_tp_test_mode = TP_TEST_MODE_NOTIFY;
                tp_notify_ctrl_rsp.send_bytes = 0;
                tp_notify_ctrl_rsp.send_pa = 0;
                g_tp_test_start_tick = xTaskGetTickCount();
                g_tp_prev_bytes = 0;
                
                g_tp_send_payload_buff_ptr = rtos_malloc(g_tx_mtu);
                memset(g_tp_send_payload_buff_ptr, 0xAB, g_tx_mtu);
                
            } else if (cmd_type == TP_CMD_TYPE_STOP) {
                tps_print_final_stats(TP_TEST_MODE_NOTIFY);
                g_tp_test_mode = 0;
                tp_ctrl_read_rsp = &tp_notify_ctrl_rsp;
                rtos_free(g_tp_send_payload_buff_ptr);
                g_tp_send_payload_buff_ptr = NULL;
                g_tp_test_start_tick = 0;
                g_tp_prev_bytes = 0;
            }
            break;

        case TP_TEST_MODE_INDICATE:

            if (cmd_type == TP_CMD_TYPE_START) {
                g_tp_test_mode = TP_TEST_MODE_INDICATE;
                tp_indicate_ctrl_rsp.send_bytes = 0;
                tp_indicate_ctrl_rsp.send_pa = 0;
                g_tp_test_start_tick = xTaskGetTickCount();
                g_tp_prev_bytes = 0;
                g_tp_send_payload_buff_ptr = rtos_malloc(g_tx_mtu);
                memset(g_tp_send_payload_buff_ptr, 0xAB, g_tx_mtu);
            } else if (cmd_type == TP_CMD_TYPE_STOP) {
                tps_print_final_stats(TP_TEST_MODE_INDICATE);
                g_tp_test_mode = 0;
                tp_ctrl_read_rsp = &tp_indicate_ctrl_rsp;
                rtos_free(g_tp_send_payload_buff_ptr);
                g_tp_send_payload_buff_ptr = NULL;
                g_tp_test_start_tick = 0;
                g_tp_prev_bytes = 0;
            }
            break;

        case TP_TEST_MODE_READ:
            
            if (cmd_type == TP_CMD_TYPE_START) {
                g_tp_test_mode = TP_TEST_MODE_READ;
                tp_read_ctrl_rsp.send_bytes = 0;
                tp_read_ctrl_rsp.send_pa = 0;
                tp_read_ctrl_rsp.recv_bytes = 0;
                tp_read_ctrl_rsp.recv_pa = 0;
                g_tp_test_start_tick = xTaskGetTickCount();
                g_tp_prev_bytes = 0;
            } else if (cmd_type == TP_CMD_TYPE_STOP) {
                tps_print_final_stats(TP_TEST_MODE_READ);
                g_tp_test_mode = 0;
                tp_ctrl_read_rsp = &tp_read_ctrl_rsp;
                g_tp_test_start_tick = 0;
                g_tp_prev_bytes = 0;
            }
            break;

        default:
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_ERROR, "tp2_process_command: invalid test_mode=%d", test_mode);
            break;
    }
    rtos_free(cmd_buff);

}
/**
 ****************************************************************************************
 * @brief This function is called during a write procedure to modify attribute handle.
 *
 *        @see gatt_srv_att_val_set_cfm shall be called to accept or reject attribute
 *        update.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Value offset
 * @param[in] p_data        Pointer to buffer that contains data to write starting from offset
 ****************************************************************************************
 */
// uint8_t test_data[20] = {11};
static void tps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, void* p_data)
{
    uint16_t  status      = BLE_GAP_ERR_NO_ERROR;
    uint8_t att_idx       = hdl - tps_env.start_hdl;

    uint16_t length = ble_co_buf_data_len(p_data);
    uint8_t* buff = ble_co_buf_data(p_data);
    void* tp_cmd_buff;
    // uint8_t  result = TP_SUCCESS;
    // ls_tp_cmd_t cmd;

    // AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tps_cb_att_val_set: conidx=%d, hdl=%d, att_idx=%d, length=%d", conidx, hdl, att_idx, length);
    switch(att_idx)
    {
        case TP_IDX_TP_NOTIFY_NTF_CFG:
        {
            uint16_t cfg = ble_co_read16p(buff);
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "notify cfg=%d", cfg);
            if(BLE_PRF_CLI_START_NTF == cfg) {
                tps_env.ntf_cfg[conidx] = tps_env.ntf_cfg[conidx] | BLE_PRF_CLI_START_NTF;
                if(g_tp_test_mode == TP_TEST_MODE_NOTIFY) {
                    tps_send_notify(0, g_tx_mtu, g_tp_send_payload_buff_ptr); // 发送首个通知以启动通知流程
                }
                
            } else {
                tps_env.ntf_cfg[conidx] = tps_env.ntf_cfg[conidx] & (~BLE_PRF_CLI_START_NTF);
            }

        }
            break;

        case TP_IDX_TP_WC_VAL:
            if(g_file_transfer_in_progress) {
                AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "TP_IDX_TP_WC_VAL: length=%d", length);
                memcpy(&g_file_recv_buf[0], buff, length);
                tps_send_notify(conidx, length, &g_file_recv_buf[0]);
            } else {
                tp_wc_ctrl_rsp.recv_bytes += length;
                tp_wc_ctrl_rsp.recv_pa += 1;
            }
            break;

        case TP_IDX_TP_WR_VAL:
            tp_wr_ctrl_rsp.recv_bytes += length;
            tp_wr_ctrl_rsp.recv_pa += 1;
            break;

        case TP_IDX_TP_INDICATE_IND_CFG:
            {
                uint16_t cfg = ble_co_read16p(buff);
                AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "indicate cfg=%d", cfg);
                if(BLE_PRF_CLI_START_IND == cfg) {
                    tps_env.ntf_cfg[conidx] = tps_env.ntf_cfg[conidx] | BLE_PRF_CLI_START_IND;
                    if(g_tp_test_mode == TP_TEST_MODE_INDICATE) {
                        tps_send_indicate(0, g_tx_mtu, g_tp_send_payload_buff_ptr); // 发送首个指示以启动指示流程
                    }   
                } else {
                    tps_env.ntf_cfg[conidx] = tps_env.ntf_cfg[conidx] & (~BLE_PRF_CLI_START_IND);
                }
            }
            break;
        case TP_IDX_TP_CTRL_VAL:
            // AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tps_cb_att_val_set: TP_IDX_TP_CTRL_VAL");
            // process control point command
            tp_cmd_buff = rtos_malloc(length);
            memcpy(tp_cmd_buff, buff, length);
            tp2_process_command(tp_cmd_buff, length);
            break;
        default:
            break;
    }
    ble_gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
    
}

/**
 ****************************************************************************************
 * @brief This function is called when peer want to read local attribute database value.
 *
 *        @see gatt_srv_att_read_get_cfm shall be called to provide attribute value
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Data offset
 * @param[in] max_length    Maximum data length to return
 ****************************************************************************************
 */
static void tps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset, uint16_t max_length)
{
    uint16_t  status      = BLE_GAP_ERR_NO_ERROR;
    uint32_t value;
    uint16_t length       = 0;
    uint8_t att_idx       = hdl - tps_env.start_hdl;
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tps_cb_att_read_get: conidx=%d, hdl=%d, att_idx=%d", conidx, hdl, att_idx);
    switch(att_idx)
    {
        case TP_IDX_TP_NOTIFY_NTF_CFG:
            value = tps_env.ntf_cfg[conidx] & BLE_PRF_CLI_START_NTF;
            length = sizeof(uint16_t);
            status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, sizeof(uint16_t), length, (uint8_t*)&value);
            break;
    
        case TP_IDX_TP_READ_VAL:
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tps_cb_att_read_get: TP_IDX_TP_READ_VAL");
            value = 0x09;
            length = sizeof(uint8_t);
            status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, sizeof(uint8_t), length, (uint8_t*)&value);
            break;
        
        case TP_IDX_TP_INDICATE_IND_CFG:

            value = tps_env.ntf_cfg[conidx] & BLE_PRF_CLI_START_IND;
            length = sizeof(uint16_t);
            status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, sizeof(uint16_t), length, (uint8_t*)&value);
            break;

        case TP_IDX_TP_CTRL_VAL:
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tps_cb_att_read_get: TP_IDX_TP_CTRL_VAL");
            tp_ctrl_rsp_t* tp_ctrl_rsp = tp_ctrl_read_rsp;
            length = sizeof(tp_ctrl_rsp_t);
            //打印结构体内容
            AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tp_ctrl_rsp: recv_bytes=%d, send_bytes=%d, recv_pa=%d, send_pa=%d, expand_len=%d",
                        tp_ctrl_rsp->recv_bytes,
                        tp_ctrl_rsp->send_bytes,
                        tp_ctrl_rsp->recv_pa,
                        tp_ctrl_rsp->send_pa,
                        tp_ctrl_rsp->expand_len);

            status = ble_gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, sizeof(uint8_t), length, (uint8_t*)tp_ctrl_rsp);
            break;

        default:
            status = BLE_PRF_ERR_INVALID_PARAM;
            break;
    }
}

void tps_cb_att_event_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t dummy, uint16_t hdl,
                              uint16_t max_length)
{
    // no implementation
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tps_cb_att_event_get not implement");
}

void tps_cb_att_info_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl)
{
    // no implementation
    AT_DBG_MSG(AT_LOG_FLAG_BLE, AT_LOG_LEVEL_DEBUG, "tps_cb_att_info_get not implement");
}

/// Service callback hander from GATT
static const ble_gatt_srv_cb_t tps_cb =
{
    .cb_event_sent    = tps_cb_event_sent,
    .cb_att_read_get  = tps_cb_att_read_get,
    .cb_att_event_get = tps_cb_att_event_get,
    .cb_att_info_get  = tps_cb_att_info_get,
    .cb_att_val_set   = tps_cb_att_val_set,
};

/*
 * GLOBAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */
uint16_t tps_init(uint8_t sec_lvl, uint8_t user_prio)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;

    uint8_t user_lid = BLE_GATT_INVALID_USER_LID;
    uint16_t start_hdl = 0;

    memset(&tps_env, 0, sizeof(tps_env));

    do
    {
        /// register TPS user
        status = ble_gatt_user_register(TP_DATA_MAX_LEN, user_prio, &tps_cb, &user_lid);
        if(status != BLE_GAP_ERR_NO_ERROR) break;

        // Add TP service
        status = ble_gatt_db_svc16_add(user_lid, sec_lvl, BLE_GATT_TP_PRIMARY_SERVICE, TP_IDX_NB,
                                   NULL, &(tp_att_db[0]), TP_IDX_NB, &start_hdl);
        if(status != BLE_GAP_ERR_NO_ERROR) break;

        tps_env.start_hdl = start_hdl;
        tps_env.user_lid = user_lid;
        tps_env.state = LE_TP_IDEL;
        tps_env.ntf_cfg[0] = BLE_PRF_CLI_STOP_NTFIND;
        /* start throughput logging task */
        tps_start_throughput_task();

    }while(0);

    if((status != BLE_GAP_ERR_NO_ERROR) && (user_lid != BLE_GATT_INVALID_USER_LID))
    {
        ble_gatt_user_unregister(user_lid);
    }

    return (status);
}




