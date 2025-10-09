#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "log_print.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "ic_message.h"
#include <assert.h>
#include "lis_algo.h"
#include "lis_trans.h"
#include "lis_tts.h"
#include "lis_ocr.h"
#include "sysutils.h"
#include "cache.h"

#define LOG_HEXDUMP(data, len, line_len) \
    do { \
        for (int i = 0; i < len; i++) { \
            logDbg("%02x ", data[i]); \
            if ((i + 1) % line_len == 0) { \
                logDbg("\n"); \
            } \
        } \
        logDbg("\n"); \
    } while (0)

#define IC_MSG_QUEUE_SIZE 16

typedef enum {
    e_algo_status_idle = 0x0,
    e_xtts_synth_begin = 0x01,
    e_xtts_synth_doing = 0x02,
    e_xtts_synth_end = 0x03,
    e_xtts_play_begin = 0x04,
    e_xtts_play_playing = 0x05,
    e_xtts_play_end = 0x06,
    e_xtts_synth_stop = 0x07,
    e_trans_begin = 0x08,
    e_trans_stop = 0x09,
    e_trans_end = 0x0A,
    e_trans_result = 0x0B,
    e_ocr_scan_begin = 0x0C,
    e_ocr_scan_end = 0x0D,
    e_roi_start,
    e_roi_stop,
} algo_status_cp2ap_e;
    
#define CLIENT_MESSAGE_TASK_STACKSIZE  (4 * 1024)
#define CLIENT_MESSAGE_TASK_PRIORITY  (1)
static QueueHandle_t rx_queue;
#define TYPE_CNT 4
static EventGroupHandle_t func_evt[TYPE_CNT];
static int algo_ic_msg_id[TYPE_CNT] = {IC_MESSAGE_ID_TRANS, IC_MESSAGE_ID_TTS, IC_MESSAGE_ID_OCR, IC_MESSAGE_ID_COMMON};// id must match algo_func_type_e
__psram_data__ static func_ack_t func_ack[TYPE_CNT] = {0};
__psram_data__ static int g_algo_status[TYPE_CNT] = {0};

int lsf_cp2ap_func(uint32_t type, uint8_t *send_data, uint32_t send_size, uint8_t *rx, uint32_t rx_size, TickType_t xTicksToWait)
{
    func_descriptors_t *func = (func_descriptors_t*)send_data; // just for debug
    // if(func) LOGD("name:%s, func:%d, enter", ((const char *[]){"trans", "xtts", "ocr"})[type], func->func);
    if((TYPE_TRANS == type && func->func == FUNC_TRANS_STATUS_E) ||
        (TYPE_TTS == type && func->func == FUNC_XTTS_STATUS_E) ||
        (TYPE_OCR == type && func->func == OCR_IOCTL_GET && func->ocr.arg == OCR_STATUS)
    ) {
        // 算法状态获取接口使用AP通知的方式，减少核间通讯频次
        func_ack_t *ack = (func_ack_t *)rx;
        ack->status = g_algo_status[type];
        return 1;
    }
    HAL_FlushDCache_by_Addr((uint32_t *)func, sizeof(func_descriptors_t));
    xEventGroupClearBits(func_evt[type], func->func);
    int ret = ic_message_msg_send_by_id(algo_ic_msg_id[type], IC_MESSAGE_MSG_TYPE_CMD, &send_data, sizeof(uint8_t*));
    ASSERT(ret == 0, "send_data failed, ret=%d", ret);
    //wait ack
    xEventGroupWaitBits(func_evt[type], func->func, true, false, portMAX_DELAY);
    memcpy(rx, &func_ack[type], sizeof(func_ack_t));
    // if(func) LOGD("name:%s, func:%d, exit\n", ((const char *[]){"trans", "xtts", "ocr"})[type], func->func);
    return 1;
}

static int32_t translation_message_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t* msg){

    uint32_t data = *(uint32_t*)msg->msg;

    func_ack_t *func = (func_ack_t*)data;
    if(func == NULL) return 0;
    HAL_InvalidateDCache_by_Addr((uint32_t *)func, sizeof(func_ack_t));
    // LOGD("func_ack.func:%p, func:%d, status:%x", func, func->func, func->status);
    // LOGD("trans ack:%p, func:%s, status:%x", func,
    //     ((const char *[]){"NULL", "start", "status", "result", "stop"})[func->func], func->status);
    switch (func->func) {
        case FUNC_TRANS_STATUS_E: {
            if(func->status == e_trans_begin) g_algo_status[TYPE_TRANS] = LIS_TRANS_STATE_ING;
            if(func->status == e_trans_stop) g_algo_status[TYPE_TRANS] = LIS_TRANS_STATE_EARLY_OVER;
            if(func->status == e_trans_end) g_algo_status[TYPE_TRANS] = LIS_TRANS_STATE_OVER;
            if(func->status == e_trans_result) g_algo_status[TYPE_TRANS] = LIS_TRANS_STATE_RESULT;
            LOGD("trans ack status:%x", g_algo_status[TYPE_TRANS]);
            break;
        }
        case FUNC_TRANS_START_E:
        case FUNC_TRANS_STOP_E:{
            memcpy(&func_ack[TYPE_TRANS], func, sizeof(func_ack_t));
            break;
        }
        case FUNC_TRANS_RESULT_E: {
            LOGD("trans ack result:%s", func->result);
            memcpy(&func_ack[TYPE_TRANS], func, sizeof(func_ack_t));
            break;
        }
        default:
            LOGD("Unknown func:%d", func->func);
            break;
    }
    xEventGroupSetBits(func_evt[TYPE_TRANS], func->func);
}

static int32_t ocr_message_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t* msg)
{
    uint32_t data = *(uint32_t*)msg->msg;

    func_ack_t *func = (func_ack_t*)data;
    if(func == NULL) return 0;
    HAL_InvalidateDCache_by_Addr((uint32_t *)func, sizeof(func_ack_t));
    // LOGD("ocr ack:%p, func:%s, status:%x", func,
    //     ((const char *[]){"NULL", "start", "status", "set", "get", "stop", "result"})[func->func], func->status);
    switch (func->func) {
        case OCR_IOCTL_STATUS: {
            g_algo_status[TYPE_OCR] = func->status;
            LOGD("ocr ack status:%x", g_algo_status[TYPE_OCR]);
            break;
        }
        case OCR_IOCTL_START:
        case OCR_IOCTL_SET:
        case OCR_IOCTL_GET:
        case OCR_IOCTL_SYS_START_UP:
        case OCR_IOCTL_STOP: {
            memcpy(&func_ack[TYPE_OCR], func, sizeof(func_ack_t));
            break;
        }
        case OCR_IOCTL_RESULT: {
            memcpy(&func_ack[TYPE_OCR], func, sizeof(func_ack_t));
            LOGD("ocr ack result:%s", func->result);
            break;
        }
        case OCR_IOCTL_RESULT_NOTIFY: {
            LOGD("ocr notify result:%s", func->result);
            extern void lis_ocr_result_callback(void *rslt);
            lis_ocr_result_callback(func->result);
            break;
        }
        default:
            LOGD("Unknown func:%d", func->func);
            break;
    }
    xEventGroupSetBits(func_evt[TYPE_OCR], func->func);
}

static int32_t xtts_message_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t* msg)
{
    uint32_t data = *(uint32_t*)msg->msg;

    func_ack_t *func = (func_ack_t*)data;
    if(func == NULL) return 0;
    HAL_InvalidateDCache_by_Addr((uint32_t *)func, sizeof(func_ack_t));
    // LOGD("xtts ack:%p, func:%s, status:%x", func,
    //     ((const char *[]){"NULL", "start", "status", "stop"})[func->func], func->status);
    switch (func->func) {
        case FUNC_XTTS_STATUS_E: {
            // LOGD("xtts get");
            switch (func->status) {
            case e_xtts_synth_begin:
            case e_xtts_synth_doing:
            case e_xtts_play_begin:
            case e_xtts_play_playing:
                g_algo_status[TYPE_TTS] = LIS_TTS_STATE_ING;
                break;
            case e_xtts_synth_end:
                g_algo_status[TYPE_TTS] = LIS_TTS_STATE_TTS_END;
                break;
            case e_xtts_play_end:
                g_algo_status[TYPE_TTS] = LIS_TTS_STATE_OVER;
                break;
            case e_xtts_synth_stop:
                g_algo_status[TYPE_TTS] = LIS_TTS_STATE_EARLY_OVER;
                break;
            default:
                g_algo_status[TYPE_TTS] = LIS_TTS_STATE_ERR;
                LOGD("xtts unkown st:%d", func->status);
                break;
            }
            LOGD("xtts ack status:%x", g_algo_status[TYPE_TTS]);
            break;
        }
        case FUNC_XTTS_START_E:
        case FUNC_XTTS_STOP_E:
        case FUNC_XTTS_SET_PARAM_E:
        {
            memcpy(&func_ack[TYPE_TTS], func, sizeof(func_ack_t));
            break;
        }
        default:
            LOGD("Unknown func:%d", func->func);
            break;
    }
    xEventGroupSetBits(func_evt[TYPE_TTS], func->func);
}

void algo_lsf_ocr_init(void)
{
    uint32_t send_data = 0;
    ic_message_register_by_id(IC_MESSAGE_ID_OCR, ocr_message_cb, NULL);
    while(!ic_message_remote_is_connected(IC_MESSAGE_ID_OCR)) {
        vTaskDelay(1);
    }
}

void algo_lsf_xtts_init(void)
{
    uint32_t send_data = 0;
    ic_message_register_by_id(IC_MESSAGE_ID_TTS, xtts_message_cb, NULL);
    do {
        int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_TTS, IC_MESSAGE_MSG_TYPE_CMD, &send_data, sizeof(send_data));
        if (ret == 0) {
            break;
        }
    } while (1);

    extern int xtts_app_task(void);
    xtts_app_task();
}

void algo_lsf_trans_init(void)
{
    uint32_t send_data = 0;
    ic_message_register_by_id(IC_MESSAGE_ID_TRANS, translation_message_cb, NULL);

    do {
        int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_TRANS, IC_MESSAGE_MSG_TYPE_CMD, &send_data, sizeof(send_data));
        if (ret == 0) {
            break;
        }
    } while (1);
}

void algo_lsf_client_start(void)
{
    rx_queue = xQueueCreate(IC_MSG_QUEUE_SIZE, sizeof(uint32_t));
    for(int i = 0; i < TYPE_CNT; i++) {
        func_evt[i] = xEventGroupCreate();
    }
}

