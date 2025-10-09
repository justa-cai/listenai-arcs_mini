#include <stdint.h>
#include <string.h>
#include "ic_message.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "plat_os.h"
#include "log_print.h"
#include "cache.h"
#include "lite_adc.h"

#define PSRAM_BASE          (0x28000000)
#define ADC_MSG_QUEUE_CNT   (1)
#define ADC_MSG_TIMEOUT     (500)

enum{
    ADC_MSG_INIT = 0,
    ADC_MSG_DEINIT,
    ADC_MSG_CTRL,
    ADC_MSG_READ,
    ADC_MSG_GET_SAMPLE,
};

typedef union{
    struct{
        uint8_t opcode : 1; /* 0: request, 1:responese */
        uint8_t ext : 1; /* 0: ok, 1:err for responese, 0: normal 1: immediately of request*/
        uint8_t cmd : 6;
        uint32_t len;
        uint32_t arg;  //
    }__attribute__((packed)) msg;
    uint8_t frame[10];
}adc_msg_t;

struct{
    uint32_t uarg;
    union {
        struct {
            int al, ar, dl, dr;
        }gain;
        uint32_t hpf[3];
    }para;
}adc_ctrl_para;

static os_semaphore_t adc_sem = NULL;
static os_semaphore_t adc_ctrl_sem = NULL;
static QueueHandle_t adc_queue;


static int32_t adc_msg_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t *msg)
{
    adc_msg_t adc_msg;
    memcpy(adc_msg.frame, (uint8_t *)msg->msg, 10);
    if(xQueueSend(adc_queue, &adc_msg, ADC_MSG_TIMEOUT)!=pdPASS){
        CLOGE("adc_msg_cb timeout");
    }
    // CLOGI("[%s]cmd:%d", __FUNCTION__, dac_msg.msg.cmd);
    return 0;
}

static int adc_msg_sync(int msg_cmd, uint32_t arg){
	int ret = 0, cnt = 0;
    adc_msg_t adc_msg;

    adc_msg.msg.opcode = 0;
    adc_msg.msg.cmd = msg_cmd;
    adc_msg.msg.arg = arg;

    if(os_semaphore_get(&adc_sem, pdMS_TO_TICKS(ADC_MSG_TIMEOUT))!=OS_EOK){
        CLOGE("get adc_sem timeout");
        return -1;
    }
	while((ret=ic_message_msg_send_by_id(IC_MESSAGE_ID_ADC, IC_MESSAGE_MSG_TYPE_EVT,
        (uint8_t *)&adc_msg, sizeof(adc_msg_t)))!=0){

        CLOGE("adc_msg_sync send failed: %d", ret);
        if(cnt ++ >10){
            ret = -1;
            CLOGE("adc_msg_sync send retry:%d", cnt);
            os_semaphore_put(&adc_sem);
            return -1;
        }
        vTaskDelay(2);
    }
    if(pdTRUE == xQueueReceive(adc_queue, &adc_msg, pdMS_TO_TICKS(ADC_MSG_TIMEOUT))){
        if((1 != adc_msg.msg.opcode) ||
           (msg_cmd != adc_msg.msg.cmd)){

            CLOGE("adc_msg_sync error opcode:%d cmd:%d-%d", adc_msg.msg.opcode, adc_msg.msg.cmd, msg_cmd);
            ret = -1;
        }else{
            ret = adc_msg.msg.arg;
        }
    }else{
        CLOGE("adc_msg_sync failed");
        ret = -1;
    }

    os_semaphore_put(&adc_sem);
    return ret;
}

int lite_adc_ctrl(uint32_t uarg, void *parg){

    if(os_semaphore_get(&adc_ctrl_sem, pdMS_TO_TICKS(ADC_MSG_TIMEOUT))!=OS_EOK){
        CLOGE("get adc_ctrl_sem timeout");
        return -1;
    }

    adc_ctrl_para.uarg = uarg;
    if(MAPI_AADC_CTRL_SET_GAIN == uarg){
        adc_ctrl_para.para.gain = *(typeof(adc_ctrl_para.para.gain) *)parg;
    }else if(MAPI_AADC_CTRL_SET_HPF == uarg){
        adc_ctrl_para.para.hpf[0] = ((uint32_t *)parg)[0];
        adc_ctrl_para.para.hpf[1] = ((uint32_t *)parg)[1];
        adc_ctrl_para.para.hpf[2] = ((uint32_t *)parg)[2];
    }
    int ret = adc_msg_sync(ADC_MSG_CTRL, (uint32_t)&adc_ctrl_para);

    os_semaphore_put(&adc_ctrl_sem);
    return ret;
}

int lite_adc_read(void *dst, int size, TickType_t msec){
    
    adc_msg_t adc_msg = {.msg.opcode = 0, .msg.cmd = ADC_MSG_READ};
    if(os_semaphore_get(&adc_sem, pdMS_TO_TICKS(ADC_MSG_TIMEOUT))!=OS_EOK){
        CLOGE("get dac_sem timeout");
        return -1;
    }
    int ret = ret=ic_message_msg_send_by_id(IC_MESSAGE_ID_ADC, IC_MESSAGE_MSG_TYPE_EVT,
                                            (uint8_t *)&adc_msg, sizeof(adc_msg_t));
    
    if(pdTRUE == xQueueReceive(adc_queue, &adc_msg, pdMS_TO_TICKS(ADC_MSG_TIMEOUT))){
        if((1 != adc_msg.msg.opcode) ||
           (ADC_MSG_READ != adc_msg.msg.cmd)){

            CLOGE("adc_msg_sync error opcode:%d cmd:%d-%d", adc_msg.msg.opcode, adc_msg.msg.cmd, ADC_MSG_READ);
            ret = -1;
        }else{
            ret = adc_msg.msg.len;
            if((uint32_t)adc_msg.msg.arg>=PSRAM_BASE){
                HAL_InvalidateDCache_by_Addr((uint32_t *)adc_msg.msg.arg, ret<<1);
                memcpy(dst, &adc_msg.msg.arg, sizeof(void *));
            }
        }
    }else{
        CLOGE("adc_msg_sync failed");
        ret = -1;
    }

    os_semaphore_put(&adc_sem);
    return ret;
}

int lite_adc_available(void){
    return adc_msg_sync(ADC_MSG_GET_SAMPLE, 0);
}

int lite_adc_init(void){
    return adc_msg_sync(ADC_MSG_INIT, 0);
}

int lite_adc_deinit(void){
    return adc_msg_sync(ADC_MSG_DEINIT, 0);    
}

int lite_adc_msg_init(void){

    os_semaphore_create_binary(&adc_sem, "adc");
    os_semaphore_put(&adc_sem);
    os_semaphore_create_binary(&adc_ctrl_sem, "adc_ctrl");
    os_semaphore_put(&adc_ctrl_sem);
    adc_queue = xQueueCreate(ADC_MSG_QUEUE_CNT, sizeof(adc_msg_t));
    return ic_message_register_by_id(IC_MESSAGE_ID_ADC, adc_msg_cb, NULL);
}
