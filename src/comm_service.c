#include <string.h>
#include "cache.h"
#include "comm_service.h"
#include "ic_message.h"
#include "lisa_log.h"
#include "lisa_thread.h"

#include "FreeRTOS.h"
#include "semphr.h"

#define TAG "comm"

typedef struct
{
    uint32_t func;
    uint32_t status;
    char result[1024];
} __attribute__((aligned(32))) func_ack_t;

SemaphoreHandle_t ivw_ctrl_sem = NULL;

__attribute__((weak)) void handle_algo_esr(char *info, uint32_t len)
{
	LISA_LOGI(TAG, "algo esr: %s", info);
}

__attribute__((weak)) void handle_algo_esr_timeout()
{
	LISA_LOGI(TAG, "algo esr timeout");
}

static int32_t ivw_message_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t* msg){

    uint32_t data = *(uint32_t*)msg->msg;

    func_ack_t *func = (func_ack_t *)data;
    if (func) {
        HAL_InvalidateDCache_by_Addr((uint32_t *)func, sizeof(func_ack_t));

        LISA_LOGV(TAG, "[%p] func: %d, status: %x", func, func->func, func->status);
        switch (func->func) {
            case FUNC_IVW_START_E: {
                // LISA_LOGD(TAG, "ivw start");
            } break;
            case FUNC_IVW_STATUS_E: {
                // LISA_LOGI(TAG, "ivw status");
            } break;
            case FUNC_IVW_TIMEOUT_E:{
                handle_algo_esr_timeout();
            } break;
            case FUNC_IVW_RESULT_E: {
                handle_algo_esr(func->result, strlen(func->result) + 1);
            }  break;
            case FUNC_IVW_CTRL_CMD_E: {
                LISA_LOGI(TAG, "ivw ctrl cmd");
                xSemaphoreGive(ivw_ctrl_sem);
            } break;
            default: {
                LISA_LOGD(TAG, "unknown func: %d", func->func);
            } break;
        }
    }

    return 0;
}

void comm_service_init()
{
    int32_t data = 0, send_count = 0;
    ic_message_register_by_id(IC_MESSAGE_ID_IVW, ivw_message_cb, NULL);
    do {
        send_count++;
        int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_IVW, IC_MESSAGE_MSG_TYPE_CMD, &data, sizeof(data));
        if (ret ==0) {
            break;
        } else {
            LISA_LOGV(TAG, "[%d] check ap ivw msg", send_count);
        }
    } while(1);

    ivw_ctrl_sem = xSemaphoreCreateBinary();
    lisa_thread_mdelay(500);
}

void lis_ivw_idle(void)
{
    func_descriptors_t ivw_ctrl = {
        .func = FUNC_IVW_CTRL_CMD_E,
        .ivw.cmd = e_algo_idle_set,
    };
    uint32_t send_msg =(uint32_t)&ivw_ctrl;

    LOGI("iwv_ctrl: %p", &ivw_ctrl);
    HAL_FlushDCache_by_Addr((uint32_t *)&ivw_ctrl, sizeof(func_descriptors_t));
    int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_IVW, IC_MESSAGE_MSG_TYPE_CMD, &send_msg, sizeof(uint8_t*));
    if (ret != 0) {
        LISA_LOGE(TAG, "send ivw idle cmd failed");
    }
    xSemaphoreTake(ivw_ctrl_sem, pdMS_TO_TICKS(10000));
}

void lis_ivw_run(void)
{
    func_descriptors_t ivw_ctrl = {
        .func = FUNC_IVW_CTRL_CMD_E,
        .ivw.cmd = e_algo_run_set,
    };
    uint32_t send_msg =(uint32_t)&ivw_ctrl;

    HAL_FlushDCache_by_Addr((uint32_t *)&ivw_ctrl, sizeof(func_descriptors_t));
    int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_IVW, IC_MESSAGE_MSG_TYPE_CMD, &send_msg, sizeof(uint8_t*));
    if (ret != 0) {
        LISA_LOGE(TAG, "send ivw run cmd failed");
        return ;
    }
    xSemaphoreTake(ivw_ctrl_sem, pdMS_TO_TICKS(10000));
}


void lis_ivw_gain_set(int8_t al, int8_t ar, int8_t dl, int8_t dr)
{
    func_descriptors_t ctrl = {
        .func = FUNC_IVW_CTRL_CMD_E,
        .ivw.cmd = e_algo_gain_set,
        .ivw.data = {
            al,
            ar,
            dl,
            dr,
        },
    };
    uint32_t send_msg =(uint32_t)&ctrl;
    HAL_FlushDCache_by_Addr((uint32_t *)&ctrl, sizeof(ctrl));
    int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_IVW, IC_MESSAGE_MSG_TYPE_CMD, &send_msg, sizeof(uint8_t*));
    if (ret != 0) {
        LISA_LOGE(TAG, "send ivw gain cmd failed");
    }
    xSemaphoreTake(ivw_ctrl_sem, pdMS_TO_TICKS(10000));
}
