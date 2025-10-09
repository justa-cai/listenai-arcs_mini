#include <stdint.h>
#include <string.h>
#include "ic_message.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "plat_os.h"
#include "log_print.h"
#include "cache.h"
#include "lite_dac.h"

#define PSRAM_BASE          (0x28000000)

#define DAC_MSG_QUEUE_CNT   (1)
#define DAC_MSG_TIMEOUT     (5000)
#define ADAC_QUE_BUF_SIZE   (1280)

enum{
    DAC_MSG_INIT = 0,
    DAC_MSG_DEINIT,
    DAC_MSG_CTRL,
    DAC_MSG_GET_BUF,
    DAC_MSG_WRITE,
    DAC_MSG_GET_SAMPLE,
    DAC_MSG_SET_PA,
    DAC_MSG_EQ_CFG,
    DAC_MSG_EQ_SW,
    DAC_MSG_SET_VOL,
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
}dac_msg_t;

static struct {
    int8_t a_gain;
    int8_t d_gain;
    uint32_t uarg;
    int32_t channel;    //音频声道
    uint32_t rate;       //采样率
    int32_t bit;        //sample的位宽
}dac_proxy_ctrl;

static struct {
    uint32_t uarg;
    uint32_t addr;       //采样率
}dac_eq_cfg;

static os_semaphore_t dac_sem = NULL; 
static QueueHandle_t dac_queue;

static int32_t dac_msg_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t *msg)
{
    dac_msg_t dac_msg;
    memcpy(dac_msg.frame, (uint8_t *)msg->msg, 10);
    if(xQueueSend(dac_queue, &dac_msg, DAC_MSG_TIMEOUT)!=pdPASS){
        CLOGE("dac_msg_cb timeout");
    }
    // CLOGI("[%s]cmd:%d", __FUNCTION__, dac_msg.msg.cmd);
    return 0;
}

static int dac_msg_sync(int msg_cmd, uint32_t arg){
	int ret = 0, cnt = 0;
    dac_msg_t dac_msg;

    dac_msg.msg.opcode = 0;
    dac_msg.msg.cmd = msg_cmd;
    dac_msg.msg.arg = arg;

    if(os_semaphore_get(&dac_sem, pdMS_TO_TICKS(DAC_MSG_TIMEOUT))!=OS_EOK){
        CLOGE("get dac_sem timeout");
        return -1;
    }
	while((ret=ic_message_msg_send_by_id(IC_MESSAGE_ID_DAC, IC_MESSAGE_MSG_TYPE_EVT,
        (uint8_t *)&dac_msg, sizeof(dac_msg_t)))!=0){

        CLOGE("dac_msg_sync send failed: %d", ret);
        if(cnt ++ >10){
            ret = -1;
            CLOGE("dac_msg_sync send retry:%d", cnt);
            os_semaphore_put(&dac_sem);
            return -1;
        }
        vTaskDelay(2);
    }
    if(pdTRUE == xQueueReceive(dac_queue, &dac_msg, pdMS_TO_TICKS(DAC_MSG_TIMEOUT))){
        if((1 != dac_msg.msg.opcode) ||
           (msg_cmd != dac_msg.msg.cmd)){

            CLOGE("dac_msg_sync error opcode:%d cmd:%d-%d", dac_msg.msg.opcode, dac_msg.msg.cmd, msg_cmd);
            ret = -1;
        }else{
            ret = dac_msg.msg.arg;
        }
    }else{
        CLOGE("dac_msg_sync failed");
        ret = -1;
    }

    os_semaphore_put(&dac_sem);
    return ret;
}

int lite_dac_left_sample(void){

    return dac_msg_sync(DAC_MSG_GET_SAMPLE, 0);
}

int lite_dac_write(void *src, int size, TickType_t xTicksToWait){
	int ret = 0, cnt = 0;
    dac_msg_t dac_msg;

    dac_msg.msg.opcode = 0;
    dac_msg.msg.cmd = DAC_MSG_WRITE;
    dac_msg.msg.len = size;
    dac_msg.msg.arg = (uint32_t)src;

   if(os_semaphore_get(&dac_sem, pdMS_TO_TICKS(DAC_MSG_TIMEOUT))!=OS_EOK){
        CLOGE("get dac_sem timeout");
        return -1;
    }
    HAL_FlushDCache_by_Addr((uint32_t *)src, size<<1);
    while((ret=ic_message_msg_send_by_id(IC_MESSAGE_ID_DAC, IC_MESSAGE_MSG_TYPE_EVT,
        (uint8_t *)&dac_msg, sizeof(dac_msg_t)))!=0){

        CLOGE("dac_msg_sync send failed: %d", ret);
        if(cnt ++ >10){
            ret = -1;
            CLOGE("dac_msg_sync send retry:%d", cnt);
            break;
        }
        vTaskDelay(2);
    }
    os_semaphore_put(&dac_sem);
    return ret;
}

int lite_dac_get_buf(uint8_t **buf, TickType_t xTicksToWait){
    
    int32_t ret = dac_msg_sync(DAC_MSG_GET_BUF, 0);

    return  ret>=0 ? (*buf=(uint8_t *)ret, ADAC_QUE_BUF_SIZE) : (*buf=NULL,0);
}

int lite_dac_ctrl(uint32_t uarg, void *parg){

    uint32_t arg = 0;
    dac_proxy_ctrl.uarg = uarg;
    if(ADAC_CTRL_AUD_CFG == uarg){
        dac_proxy_ctrl.channel = ((dac_aud_t*)parg)->channel;
        dac_proxy_ctrl.rate = ((dac_aud_t*)parg)->rate;
        dac_proxy_ctrl.bit = ((dac_aud_t*)parg)->bit;
    }else if(ADAC_CTRL_VOLUME == uarg){
        dac_proxy_ctrl.a_gain = ((dac_gain_t*)parg)->a_gain;
        dac_proxy_ctrl.d_gain = ((dac_gain_t*)parg)->d_gain;
    }
    arg = (uint32_t)&dac_proxy_ctrl;
    return dac_msg_sync(DAC_MSG_CTRL, arg);
}

int lite_dac_eq_config(uint32_t uarg, void *parg){

    uint32_t arg = 0;
    dac_eq_cfg.uarg = uarg;
    dac_eq_cfg.addr = (uint32_t)parg;
    arg = (uint32_t)&dac_eq_cfg;
    return dac_msg_sync(DAC_MSG_EQ_CFG, arg);
}

int lite_dac_eq_swtich(int sw){
    return dac_msg_sync(DAC_MSG_EQ_SW, sw);
}

__attribute__((section (".cmn_sram.bss"))) static volatile float vol_mul = 0;
int lite_dac_set_vol(float mul){

    static uint8_t first = 1;
    vol_mul = mul;

    if (!first) {
        return 0;
    }

    first = 0;

    return dac_msg_sync(DAC_MSG_SET_VOL, (uint32_t)&vol_mul);
}

int lite_dac_proxy_pa_pulse_set(int pulse){
    return dac_msg_sync(DAC_MSG_SET_PA, pulse);
}

int lite_dac_init(void){

    return dac_msg_sync(DAC_MSG_INIT, 0);
}

int lite_dac_deinit(void){

    return dac_msg_sync(DAC_MSG_DEINIT, 0);
}

int lite_dac_msg_init(void){

    os_semaphore_create_binary(&dac_sem, "adc");
    os_semaphore_put(&dac_sem);
    dac_queue = xQueueCreate(DAC_MSG_QUEUE_CNT, sizeof(dac_msg_t));
    return ic_message_register_by_id(IC_MESSAGE_ID_DAC, dac_msg_cb, NULL);
}

