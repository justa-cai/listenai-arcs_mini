/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "mbox_client"

#include <cache.h>
#include <Driver_MBX.h>
#include <lisa_log.h>
#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "lnn_resnet18_real_cp.h"

static SemaphoreHandle_t ipc_mutex;
static volatile bool ap_status_dirty = true;
static lnn_resnet18_real_msg_t mbox_rx_msg __attribute__((aligned(4)));

void lnn_ipc_cache_flush(volatile lnn_resnet18_real_ipc_t *ipc)
{
    HAL_FlushDCache_by_Addr((uint32_t *)(uintptr_t)ipc, sizeof(*ipc));
    HAL_FlushDCache();
    __asm__ volatile("fence iorw, iorw" : : : "memory");
}

void lnn_ipc_cache_invalidate(volatile lnn_resnet18_real_ipc_t *ipc)
{
    __asm__ volatile("fence iorw, iorw" : : : "memory");
    HAL_InvalidateDCache_by_Addr((uint32_t *)(uintptr_t)ipc, sizeof(*ipc));
}

void lnn_ipc_lock(void)
{
    if (ipc_mutex != NULL) {
        (void)xSemaphoreTake(ipc_mutex, portMAX_DELAY);
    }
}

void lnn_ipc_unlock(void)
{
    if (ipc_mutex != NULL) {
        (void)xSemaphoreGive(ipc_mutex);
    }
}

bool lnn_ipc_ready_for_request(volatile lnn_resnet18_real_ipc_t *ipc)
{
    return ipc->magic == LNN_RESNET18_REAL_IPC_MAGIC &&
           (ipc->state == LNN_RESNET18_REAL_STATE_IDLE || ipc->state == LNN_RESNET18_REAL_STATE_DONE);
}

void lnn_ipc_mark_status_dirty(void)
{
    ap_status_dirty = true;
}

bool lnn_ipc_take_status_dirty(void)
{
    bool dirty = ap_status_dirty;

    ap_status_dirty = false;
    return dirty;
}

int lnn_ipc_mutex_init(void)
{
    ipc_mutex = xSemaphoreCreateMutex();
    if (ipc_mutex == NULL) {
        LOGE("create IPC mutex failed");
        return -1;
    }

    return 0;
}

static int8_t lnn_mbox_event_handler(uint32_t event, uint32_t param)
{
    if (event == CSK_MBX_EVENT_RECEIVE_COMPLETE) {
        const lnn_resnet18_real_msg_t msg = mbox_rx_msg;

        if (param == LNN_RESNET18_REAL_MBOX_CH && msg.magic == LNN_RESNET18_REAL_IPC_MAGIC &&
            msg.type == LNN_RESNET18_REAL_MSG_STATUS) {
            lnn_ipc_mark_status_dirty();
        }
        (void)MBX_Receive(NULL, &mbox_rx_msg, LNN_RESNET18_REAL_MBOX_CH);
    }

    return 0;
}

int lnn_mbox_init(void)
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

int lnn_ipc_notify_ap(uint32_t seq)
{
    lnn_resnet18_real_msg_t msg = {
        .magic = LNN_RESNET18_REAL_IPC_MAGIC,
        .seq = seq,
        .type = LNN_RESNET18_REAL_MSG_REQUEST,
        .state = LNN_RESNET18_REAL_STATE_REQUEST,
    };
    int32_t ret = CSK_DRIVER_OK;

    for (uint32_t i = 0; i < 20U; i++) {
        ret = MBX_Send(NULL, &msg, LNN_RESNET18_REAL_MBOX_CH);
        if (ret == CSK_DRIVER_OK) {
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    LOGW("notify AP by mailbox failed: %d", ret);
    return -1;
}
