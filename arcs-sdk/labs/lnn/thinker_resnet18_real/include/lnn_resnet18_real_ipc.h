/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#define LNN_RESNET18_REAL_IPC_MAGIC       (0x4C4E4E52U) /* LNNR */
#define LNN_RESNET18_REAL_IPC_VERSION     (1U)
#define LNN_RESNET18_REAL_MSG_REQUEST     (1U)
#define LNN_RESNET18_REAL_MSG_STATUS      (2U)
#define LNN_RESNET18_REAL_MBOX_CH         (6U)

#define LNN_RESNET18_REAL_INPUT_CHANNELS  (3U)
#define LNN_RESNET18_REAL_INPUT_WIDTH     (192U)
#define LNN_RESNET18_REAL_INPUT_HEIGHT    (192U)
#define LNN_RESNET18_REAL_INPUT_BYTES     \
    (LNN_RESNET18_REAL_INPUT_CHANNELS * LNN_RESNET18_REAL_INPUT_WIDTH * LNN_RESNET18_REAL_INPUT_HEIGHT)

#define LNN_RESNET18_REAL_LABEL_MAX       (32U)
#define LNN_RESNET18_REAL_MESSAGE_MAX     (64U)

typedef enum {
    LNN_RESNET18_REAL_STATE_BOOTING = 0,
    LNN_RESNET18_REAL_STATE_IDLE,
    LNN_RESNET18_REAL_STATE_REQUEST,
    LNN_RESNET18_REAL_STATE_BUSY,
    LNN_RESNET18_REAL_STATE_DONE,
    LNN_RESNET18_REAL_STATE_ERROR,
} lnn_resnet18_real_state_t;

typedef struct __attribute__((aligned(64))) {
    volatile uint32_t magic;
    volatile uint32_t version;
    volatile uint32_t struct_size;
    volatile uint32_t seq;
    volatile uint32_t state;
    volatile uint32_t input_width;
    volatile uint32_t input_height;
    volatile uint32_t input_bytes;
    volatile int32_t result_score;
    volatile int32_t result_index;
    volatile uint32_t label_len;
    char result_label[LNN_RESNET18_REAL_LABEL_MAX];
    char message[LNN_RESNET18_REAL_MESSAGE_MAX];
    /* YOLO 输入张量 110KB 放不进 12KB 的 IPC RAM：
     * 张量本体在 CP 侧共享 PSRAM（.lnn.input 段），此处只交接地址。
     * CP 写入后 flush D-cache；AP 侧 DCache 常关，直接读。 */
    volatile uint32_t input_addr;
    volatile uint32_t reserved[15]; /* 补齐到 64 字节对齐 */
} lnn_resnet18_real_ipc_t;

typedef struct __attribute__((aligned(4))) {
    uint32_t magic;
    uint32_t seq;
    uint32_t type;
    uint32_t state;
} lnn_resnet18_real_msg_t;

extern volatile lnn_resnet18_real_ipc_t lnn_resnet18_real_ipc_block;

static inline volatile lnn_resnet18_real_ipc_t *lnn_resnet18_real_ipc_get(void)
{
    return &lnn_resnet18_real_ipc_block;
}
