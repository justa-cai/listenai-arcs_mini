
#include <stdint.h>
#include <string.h>
#include "ic_message.h"
// #include "platform.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "flash_msg.h"
#include "lisa_log.h"

#define FLASH_MSG_QUEUE_CNT (1)

typedef union {
    struct {
        uint8_t opcode: 1; /* 0: request, 1:responese */
        uint8_t ext: 1;    /* 0: ok, 1:err for responese, 0: normal 1: immediately of request*/
        uint8_t cmd: 6;
        uint32_t arg; //
    } __attribute__((packed)) msg;
    uint8_t frame[10];
} flash_msg_t;

__attribute__((section(".cmn_sram.bss"))) static volatile uint8_t flash_flag = 0;
static volatile uint8_t *ap_pause_flag = NULL;
static QueueHandle_t flash_queue;
static int32_t flash_msg_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t *msg)
{
    flash_msg_t flash_msg;

    if (msg->len < sizeof(flash_msg_t)) {
        LOGE("flash_message_cb: msg length is too short");
        return -1;
    }

    memcpy(flash_msg.frame, (uint8_t *)msg->msg, 10);
    if (flash_msg.msg.cmd == CMD_FLASH_INIT) {
        xQueueSend(flash_queue, &flash_msg, portMAX_DELAY);
    }

    return 0;
}

#include <stdio.h>

__attribute__((optimize("O0"))) int flash_set_flash_flag(void)
{
    flash_flag = 0;
    *ap_pause_flag = 0;
    return 0;
}

__attribute__((optimize("O0"))) int flash_msg_send(uint8_t cmd)
{
    int err = 0;
    flash_msg_t flash_msg;

    flash_msg.msg.opcode = 0;
    flash_msg.msg.cmd = cmd;
    flash_msg.msg.arg = (uint32_t)&flash_flag;

    if (cmd == CMD_FLASH_INIT)
    {
        flash_flag = 0;
    } else {
        flash_flag = 1;
    }
    // LOGI("flash_msg_send enter cmd: %d", cmd);
    /* send msg to ap */
    while (!ic_message_remote_is_connected(IC_MESSAGE_ID_FLASH_MSG)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    while ((err = ic_message_msg_send_by_id(IC_MESSAGE_ID_FLASH_MSG, IC_MESSAGE_MSG_TYPE_EVT, (uint8_t *)&flash_msg,
                                            sizeof(flash_msg_t))) != 0) {

        LOGE("flash_msg_send failed: %d", err);
        vTaskDelay(10);
    }

    if (cmd == CMD_FLASH_INIT) {
        flash_msg_t resp;
        if (pdTRUE == xQueueReceive(flash_queue, &resp, pdMS_TO_TICKS(5000))) {
            if ((1 == resp.msg.opcode) && (cmd == resp.msg.cmd)) {

                LOGI("Shared var sync complete");
                ap_pause_flag = (uint8_t *)resp.msg.arg;
            } else {
                err = -1;
                LOGE("flash msg sync wrong cmd:%d sync:opcode[%d] cmd[%d]", cmd, resp.msg.opcode, resp.msg.cmd);
            }
        } else {
            err = -1;
            LOGE("flash msg sync timeout");
        }
    } else {
        // LOGI("ap_pause_flag: %d %p", *ap_pause_flag, ap_pause_flag);
        if (ap_pause_flag)
            while (0 == *ap_pause_flag){

            }
        else
            LOGE("ap_pause_flag is NULL");
    }

    return err;
}

// int

int flash_msg_init(void)
{
    int ret = 0;

    flash_queue = xQueueCreate(FLASH_MSG_QUEUE_CNT, sizeof(flash_msg_t));
    if (flash_queue == NULL) {
        LOGE("flash_msg_init failed, flash_queue is NULL");
        return -1;
    }
    ret = ic_message_register_by_id(IC_MESSAGE_ID_FLASH_MSG, flash_msg_cb, NULL);
    if (ret != 0) {
        LOGE("flash_msg_init failed, ic_message_register_by_id returned %d", ret);
        return ret;
    }
    return flash_msg_send(CMD_FLASH_INIT);
}
