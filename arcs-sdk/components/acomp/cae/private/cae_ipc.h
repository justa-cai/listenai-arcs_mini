/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AP -> CP  notify subcmd - 简化版，仅保留音频和角度 */

#define CAE_INDEX_AES     (1)
#define CAE_INDEX_JSON    (2)

typedef enum {
    CAE_IPC_NOTIFY_SUBCMD_PCM_DATA = 1,
    CAE_IPC_NOTIFY_SUBCMD_ANGLE    = 3,
    CAE_IPC_NOTIFY_SUBCMD_ERROR    = 5,
} cae_ipc_notify_subcmd_e;

typedef struct {
    cae_ipc_notify_subcmd_e cmd;
} __attribute__((packed)) cae_ipc_notify_subcmd_hdr_t;

typedef struct {
    cae_ipc_notify_subcmd_hdr_t hdr;
    uint32_t frame_idx;
    uint32_t len;
    uint8_t data[];
} __attribute__((packed, aligned(32))) cae_ipc_notify_subcmd_pcm_data_t;

typedef struct {
    cae_ipc_notify_subcmd_hdr_t hdr;
    uint32_t len;
    uint32_t angle_cnt;
    uint16_t angle_data[];
} __attribute__((packed, aligned(32))) cae_ipc_notify_subcmd_angle_t;

typedef struct {
    cae_ipc_notify_subcmd_hdr_t hdr;
    int32_t err_code;
    uint8_t err_msg[];
} __attribute__((packed, aligned(32))) cae_ipc_notify_subcmd_error_t;

/* CP -> AP  control subcmd - 简化版，仅保留波束设置 */

typedef enum {
    CAE_IPC_CONTROL_SUBCMD_BEAM_SET = 1,
} cae_ipc_control_subcmd_e;

typedef struct {
    uint8_t beam_index;  /* 波束索引 (0-2) */
} __attribute__((packed)) cae_ipc_control_subcmd_beam_set_t;

/* ipc end */

#ifdef __cplusplus
}
#endif
