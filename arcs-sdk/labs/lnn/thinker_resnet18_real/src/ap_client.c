/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "lnn_ap_client"

#include <lisa_log.h>
#include <stdint.h>
#include <stdio.h>

#include "lnn_resnet18_real_cp.h"

#define RGB565_RED(value)   (uint8_t)((((value) >> 11) & 0x1FU) << 3)
#define RGB565_GREEN(value) (uint8_t)((((value) >> 5) & 0x3FU) << 2)
#define RGB565_BLUE(value)  (uint8_t)(((value) & 0x1FU) << 3)

static const double cifar100_train_mean[] = {0.5070751592371323, 0.48654887331495095, 0.4409178433670343};
static const double cifar100_train_std[] = {0.2673342858792401, 0.2564384629170883, 0.27615047132568404};

static int8_t quantize_input(uint8_t value, uint32_t channel)
{
    double normalized = ((double)value / 255.0 - cifar100_train_mean[channel]) / cifar100_train_std[channel];
    double scaled = normalized * 64.0;
    int32_t q = (int32_t)(scaled >= 0.0 ? (scaled + 0.5) : (scaled - 0.5));

    if (q > 127) {
        q = 127;
    } else if (q < -128) {
        q = -128;
    }

    return (int8_t)q;
}

static uint16_t rgb565_window_get_pixel(const rgb565_window_t *window, uint32_t x, uint32_t y)
{
    return window->pixels[(window->y + y) * window->stride + window->x + x];
}

static void preprocess_rgb565_to_tensor(const rgb565_window_t *src, int8_t *dst)
{
    for (uint32_t y = 0; y < LNN_RESNET18_REAL_INPUT_HEIGHT; y++) {
        for (uint32_t x = 0; x < LNN_RESNET18_REAL_INPUT_WIDTH; x++) {
            uint32_t src_x = x * src->width / LNN_RESNET18_REAL_INPUT_WIDTH;
            uint32_t src_y = y * src->height / LNN_RESNET18_REAL_INPUT_HEIGHT;
            uint32_t offset = y * LNN_RESNET18_REAL_INPUT_WIDTH + x;
            uint16_t rgb565 = rgb565_window_get_pixel(src, src_x, src_y);

            dst[0 * LNN_RESNET18_REAL_INPUT_WIDTH * LNN_RESNET18_REAL_INPUT_HEIGHT + offset] =
                quantize_input(RGB565_RED(rgb565), 0);
            dst[1 * LNN_RESNET18_REAL_INPUT_WIDTH * LNN_RESNET18_REAL_INPUT_HEIGHT + offset] =
                quantize_input(RGB565_GREEN(rgb565), 1);
            dst[2 * LNN_RESNET18_REAL_INPUT_WIDTH * LNN_RESNET18_REAL_INPUT_HEIGHT + offset] =
                quantize_input(RGB565_BLUE(rgb565), 2);
        }
    }
}

bool lnn_ap_ready_for_request(void)
{
    volatile lnn_resnet18_real_ipc_t *ipc = lnn_resnet18_real_ipc_get();
    bool ready;

    lnn_ipc_lock();
    lnn_ipc_cache_invalidate(ipc);
    ready = lnn_ipc_ready_for_request(ipc);
    lnn_ipc_unlock();

    return ready;
}

int lnn_ap_submit_window_for_inference(const rgb565_window_t *window)
{
    volatile lnn_resnet18_real_ipc_t *ipc = lnn_resnet18_real_ipc_get();
    uint32_t prev_state;
    uint32_t seq;
    uint32_t readback_seq;
    uint32_t readback_state;

    if (window == NULL || window->pixels == NULL || window->width == 0U || window->height == 0U) {
        return -1;
    }

    lnn_ipc_lock();
    lnn_ipc_cache_invalidate(ipc);
    if (!lnn_ipc_ready_for_request(ipc)) {
        LOGW("AP not ready: magic=0x%08x state=%u message=%s", ipc->magic, ipc->state, (const char *)ipc->message);
        lnn_ipc_unlock();
        return -1;
    }
    prev_state = ipc->state;

    int8_t *input = (int8_t *)ipc->input;
    preprocess_rgb565_to_tensor(window, input);

    ipc->seq++;
    ipc->state = LNN_RESNET18_REAL_STATE_REQUEST;
    ipc->input_width = LNN_RESNET18_REAL_INPUT_WIDTH;
    ipc->input_height = LNN_RESNET18_REAL_INPUT_HEIGHT;
    ipc->input_bytes = LNN_RESNET18_REAL_INPUT_BYTES;
    ipc->result_label[0] = '\0';
    ipc->label_len = 0;
    ipc->result_score = 0;
    ipc->result_index = -1;
    (void)snprintf((char *)ipc->message, LNN_RESNET18_REAL_MESSAGE_MAX, "CP request seq=%u", ipc->seq);

    seq = ipc->seq;
    lnn_ipc_cache_flush(ipc);
    lnn_ipc_cache_invalidate(ipc);
    readback_seq = ipc->seq;
    readback_state = ipc->state;
    lnn_ipc_unlock();

    int notify_ret = lnn_ipc_notify_ap(seq);
    LOGI("submit frame seq=%u prev_state=%u readback=%u/%u window=%ux%u+%u+%u ipc_addr=0x%08x notify=%s:%d", seq,
         prev_state, readback_seq, readback_state, window->width, window->height, window->x, window->y,
         (uint32_t)(uintptr_t)ipc, REQUEST_NOTIFY_MODE, notify_ret);
    return 0;
}

void lnn_ap_read_status(lnn_ap_status_t *status)
{
    volatile lnn_resnet18_real_ipc_t *ipc = lnn_resnet18_real_ipc_get();

    if (status == NULL) {
        return;
    }

    lnn_ipc_lock();
    lnn_ipc_cache_invalidate(ipc);
    status->magic = ipc->magic;
    status->seq = ipc->seq;
    status->state = ipc->state;
    status->result_score = ipc->result_score;
    (void)snprintf(status->result_text, sizeof(status->result_text), "%s", (const char *)ipc->result_label);
    (void)snprintf(status->message, sizeof(status->message), "%s", (const char *)ipc->message);
    lnn_ipc_unlock();
}

void lnn_ap_mark_status_dirty(void)
{
    lnn_ipc_mark_status_dirty();
}

bool lnn_ap_take_status_dirty(void)
{
    return lnn_ipc_take_status_dirty();
}
