/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "lnn_real_ap"

#include <arcs_ap.h>
#include <cache.h>
#include <Driver_MBX.h>
#include <lisa_log.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "lnn_resnet18_real_ipc.h"
#include "resnet18_real_ap.h"

#define REQUEST_POLL_INTERVAL_MS  (50U)
#define AP_HEARTBEAT_INTERVAL_MS  (2000U)
#define STATUS_NOTIFY_QUEUE_DEPTH (4U)

typedef struct {
    uint32_t seq;
    uint32_t state;
} ap_status_notify_t;

typedef enum {
    AP_PHASE_BOOT = 0,
    AP_PHASE_MODEL_INIT,
    AP_PHASE_READY,
    AP_PHASE_WAIT,
    AP_PHASE_INVALIDATE,
    AP_PHASE_ACCEPT,
    AP_PHASE_BUSY,
    AP_PHASE_FORWARD,
    AP_PHASE_RESULT,
    AP_PHASE_ERROR,
} ap_phase_t;

static volatile uint32_t ap_phase = AP_PHASE_BOOT;
static volatile uint32_t ap_loop_count;
static volatile uint32_t ap_mbox_msg_count;
static volatile uint32_t ap_last_notified;
static volatile uint32_t ap_last_msg_seq;
static volatile uint32_t ap_last_msg_state;
static volatile uint32_t ap_last_seq;
static volatile uint32_t ap_last_state = LNN_RESNET18_REAL_STATE_BOOTING;
static volatile uint32_t ap_last_input_bytes;
static QueueHandle_t status_notify_queue;
static TaskHandle_t inference_task_handle;
static lnn_resnet18_real_msg_t mbox_rx_msg __attribute__((aligned(4)));

static const char *ap_phase_name(uint32_t phase)
{
    switch ((ap_phase_t)phase) {
    case AP_PHASE_BOOT:
        return "boot";
    case AP_PHASE_MODEL_INIT:
        return "model_init";
    case AP_PHASE_READY:
        return "ready";
    case AP_PHASE_WAIT:
        return "wait";
    case AP_PHASE_INVALIDATE:
        return "invalidate";
    case AP_PHASE_ACCEPT:
        return "accept";
    case AP_PHASE_BUSY:
        return "busy";
    case AP_PHASE_FORWARD:
        return "forward";
    case AP_PHASE_RESULT:
        return "result";
    case AP_PHASE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

static void ipc_cache_flush(volatile lnn_resnet18_real_ipc_t *ipc)
{
    HAL_FlushDCache_by_Addr((uint32_t *)(uintptr_t)ipc, sizeof(*ipc));
    HAL_FlushDCache();
    __asm__ volatile("fence iorw, iorw" : : : "memory");
}

static void ipc_cache_invalidate(volatile lnn_resnet18_real_ipc_t *ipc)
{
    __asm__ volatile("fence iorw, iorw" : : : "memory");
    HAL_InvalidateDCache_by_Addr((uint32_t *)(uintptr_t)ipc, sizeof(*ipc));
}

static void ipc_set_message(volatile lnn_resnet18_real_ipc_t *ipc, const char *message)
{
    char *dst = (char *)ipc->message;

    if (message == NULL) {
        dst[0] = '\0';
        return;
    }

    (void)snprintf(dst, LNN_RESNET18_REAL_MESSAGE_MAX, "%s", message);
}

static int8_t lnn_mbox_event_handler(uint32_t event, uint32_t param)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (event == CSK_MBX_EVENT_RECEIVE_COMPLETE) {
        const lnn_resnet18_real_msg_t msg = mbox_rx_msg;

        if (param == LNN_RESNET18_REAL_MBOX_CH && msg.magic == LNN_RESNET18_REAL_IPC_MAGIC &&
            msg.type == LNN_RESNET18_REAL_MSG_REQUEST) {
            ap_mbox_msg_count++;
            ap_last_notified = 1U;
            ap_last_msg_seq = msg.seq;
            ap_last_msg_state = msg.state;
            if (inference_task_handle != NULL) {
                vTaskNotifyGiveFromISR(inference_task_handle, &higher_priority_task_woken);
            }
        }
        (void)MBX_Receive(NULL, &mbox_rx_msg, LNN_RESNET18_REAL_MBOX_CH);
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
    return 0;
}

static int lnn_mbox_init(void)
{
    int32_t ret = MBX_Initialize(NULL, lnn_mbox_event_handler);
    if (ret != CSK_DRIVER_OK) {
        LOGE("init mailbox failed: %d", ret);
        return -1;
    }

    ret = MBX_Receive(NULL, &mbox_rx_msg, LNN_RESNET18_REAL_MBOX_CH);
    if (ret != CSK_DRIVER_OK) {
        LOGE("arm mailbox receive failed: %d", ret);
        return -1;
    }

    return 0;
}

static void ap_gpio_irq_disable_for_cp_touch(void)
{
    /*
     * GPIOA/GPIOB are owned by CP touch in this sample. AP must not enable the
     * local GPIO IRQ vectors; CP may enable GPIO REG_INTREN and handle peripheral
     * status. Do not clear REG_INTRSTATUS here.
     */
    disable_IRQ(IRQ_GPIOA_VECTOR);
    disable_IRQ(IRQ_GPIOB_VECTOR);
    clear_IRQ(IRQ_GPIOA_VECTOR);
    clear_IRQ(IRQ_GPIOB_VECTOR);
}

static void notify_cp_direct(uint32_t seq, uint32_t state)
{
    lnn_resnet18_real_msg_t msg = {
        .magic = LNN_RESNET18_REAL_IPC_MAGIC,
        .seq = seq,
        .type = LNN_RESNET18_REAL_MSG_STATUS,
        .state = state,
    };
    int32_t ret = CSK_DRIVER_OK;

    for (uint32_t i = 0; i < 10U; i++) {
        ret = MBX_Send(NULL, &msg, LNN_RESNET18_REAL_MBOX_CH);
        if (ret == CSK_DRIVER_OK) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    LOGW("notify CP by mailbox failed: %d", ret);
}

static void notify_cp(uint32_t seq, uint32_t state)
{
    if (status_notify_queue == NULL) {
        return;
    }

    ap_status_notify_t notify = {
        .seq = seq,
        .state = state,
    };

    (void)xQueueSend(status_notify_queue, &notify, 0);
}

static void status_notify_task(void *arg)
{
    (void)arg;
    ap_status_notify_t notify;

    while (1) {
        if (xQueueReceive(status_notify_queue, &notify, portMAX_DELAY) == pdPASS) {
            notify_cp_direct(notify.seq, notify.state);
        }
    }
}

static void boot_cp_core(void)
{
    LOGI("boot CP from 0x%08x", (uint32_t)CONFIG_THINKER_RESNET18_REAL_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = CONFIG_THINKER_RESNET18_REAL_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
}

static void heartbeat_task(void *arg)
{
    (void)arg;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(AP_HEARTBEAT_INTERVAL_MS));
        LOGI("heartbeat phase=%s loops=%u mbox_msgs=%u msg=%u/%u notified=%u seq=%u state=%u input_bytes=%u",
             ap_phase_name(ap_phase), ap_loop_count, ap_mbox_msg_count, ap_last_msg_seq, ap_last_msg_state,
             ap_last_notified, ap_last_seq, ap_last_state, ap_last_input_bytes);
    }
}

static void inference_task(void *arg)
{
    (void)arg;

    volatile lnn_resnet18_real_ipc_t *ipc = lnn_resnet18_real_ipc_get();
    uint32_t last_seen_seq = UINT32_MAX;
    uint32_t last_seen_state = UINT32_MAX;

    ap_phase = AP_PHASE_MODEL_INIT;
    if (resnet18_real_init() != 0) {
        LOGE("AP model init failed");
        ipc->state = LNN_RESNET18_REAL_STATE_ERROR;
        ipc->result_score = 0;
        ipc->result_index = -1;
        ipc_set_message(ipc, "AP model init failed");
        ipc_cache_flush(ipc);
        ap_phase = AP_PHASE_ERROR;
        ap_last_state = ipc->state;
        notify_cp(ipc->seq, ipc->state);
        vTaskDelete(NULL);
    }

    ipc->state = LNN_RESNET18_REAL_STATE_IDLE;
    ipc_set_message(ipc, "AP ready");
    ipc_cache_flush(ipc);
    ap_phase = AP_PHASE_READY;
    ap_last_state = ipc->state;
    ap_last_input_bytes = ipc->input_bytes;
    notify_cp(ipc->seq, ipc->state);
    LOGI("AP ready for request, ipc_addr=0x%08x", (uint32_t)(uintptr_t)ipc);
    log_flush();

    while (1) {
        ap_phase = AP_PHASE_WAIT;
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(REQUEST_POLL_INTERVAL_MS));

        ap_loop_count++;
        ap_phase = AP_PHASE_INVALIDATE;
        ipc_cache_invalidate(ipc);
        ap_last_seq = ipc->seq;
        ap_last_state = ipc->state;
        ap_last_input_bytes = ipc->input_bytes;

        if (ipc->magic != LNN_RESNET18_REAL_IPC_MAGIC || ipc->version != LNN_RESNET18_REAL_IPC_VERSION) {
            continue;
        }

        if (ipc->seq != last_seen_seq || ipc->state != last_seen_state) {
            last_seen_seq = ipc->seq;
            last_seen_state = ipc->state;
        }

        if (ipc->state != LNN_RESNET18_REAL_STATE_REQUEST) {
            continue;
        }

        uint32_t seq = ipc->seq;
        if (ipc->input_bytes != LNN_RESNET18_REAL_INPUT_BYTES) {
            ipc->result_label[0] = '\0';
            ipc->label_len = 0;
            ipc->result_score = 0;
            ipc->result_index = -1;
            ipc->state = LNN_RESNET18_REAL_STATE_ERROR;
            ipc_set_message(ipc, "bad input size");
            ipc_cache_flush(ipc);
            ap_phase = AP_PHASE_ERROR;
            ap_last_state = ipc->state;
            notify_cp(seq, ipc->state);
            continue;
        }

        ap_phase = AP_PHASE_ACCEPT;
        ipc->state = LNN_RESNET18_REAL_STATE_BUSY;
        ipc_set_message(ipc, "AP running inference");
        ipc_cache_flush(ipc);
        ap_phase = AP_PHASE_BUSY;
        ap_last_state = ipc->state;
        notify_cp(seq, ipc->state);
        LOGI("request accepted seq=%u input_bytes=%u", seq, ipc->input_bytes);

        const char *label = NULL;
        int8_t score = 0;
        int32_t index = -1;
        ap_phase = AP_PHASE_FORWARD;
        int ret = resnet18_real_run_tensor((const int8_t *)ipc->input, &label, &score, &index);

        if (ret == 0 && label != NULL) {
            char *dst = (char *)ipc->result_label;
            (void)snprintf(dst, LNN_RESNET18_REAL_LABEL_MAX, "%s", label);
            ipc->label_len = (uint32_t)strlen(dst);
            ipc->result_score = score;
            ipc->result_index = index;
            ipc->state = LNN_RESNET18_REAL_STATE_DONE;
            ipc_set_message(ipc, "done");
            ap_phase = AP_PHASE_RESULT;
            ap_last_state = ipc->state;
        } else {
            ipc->result_label[0] = '\0';
            ipc->label_len = 0;
            ipc->result_score = 0;
            ipc->result_index = -1;
            ipc->state = LNN_RESNET18_REAL_STATE_ERROR;
            ipc_set_message(ipc, "inference failed");
            ap_phase = AP_PHASE_ERROR;
            ap_last_state = ipc->state;
        }

        ipc_cache_flush(ipc);
        notify_cp(seq, ipc->state);
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    volatile lnn_resnet18_real_ipc_t *ipc = lnn_resnet18_real_ipc_get();

    memset((void *)ipc, 0, sizeof(*ipc));
    ipc->magic = LNN_RESNET18_REAL_IPC_MAGIC;
    ipc->version = LNN_RESNET18_REAL_IPC_VERSION;
    ipc->struct_size = sizeof(*ipc);
    ipc->state = LNN_RESNET18_REAL_STATE_BOOTING;
    ipc->input_width = LNN_RESNET18_REAL_INPUT_WIDTH;
    ipc->input_height = LNN_RESNET18_REAL_INPUT_HEIGHT;
    ipc->input_bytes = LNN_RESNET18_REAL_INPUT_BYTES;
    ipc_set_message(ipc, "AP booting");
    ipc_cache_flush(ipc);

    ap_gpio_irq_disable_for_cp_touch();

    boot_cp_core();

    if (lnn_mbox_init() != 0) {
        return -1;
    }

    status_notify_queue = xQueueCreate(STATUS_NOTIFY_QUEUE_DEPTH, sizeof(ap_status_notify_t));
    if (status_notify_queue == NULL) {
        LOGE("create status notify queue failed");
        return -1;
    }

    if (xTaskCreate(status_notify_task, "ap_notify", 2048, NULL, configMAX_PRIORITIES - 4, NULL) != pdPASS) {
        LOGE("create status_notify_task failed");
        return -1;
    }

    if (xTaskCreate(inference_task, "resnet18_ap", 8192, NULL, configMAX_PRIORITIES - 2, &inference_task_handle) !=
        pdPASS) {
        LOGE("create inference_task failed");
        return -1;
    }

    return 0;
}
