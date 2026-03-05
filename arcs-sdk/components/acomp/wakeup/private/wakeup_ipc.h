/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AP -> CP  notify result*/

typedef enum{
    WAKEUP_IPC_NOTIFY_SUBCMD_PCM_DATA = 1,
    WAKEUP_IPC_NOTIFY_SUBCMD_WAKEUP_TIMEOUT = 2,
    WAKEUP_IPC_NOTIFY_SUBCMD_ANGLE = 3,
    WAKEUP_IPC_NOTIFY_SUBCMD_MODE_SWITCH = 4,
}wakeup_ipc_notify_subcmd_e;

typedef struct{
    wakeup_ipc_notify_subcmd_e cmd;
    
}__attribute__((packed)) wakeup_ipc_notify_subcmd_hdr_t;

typedef struct{
    wakeup_ipc_notify_subcmd_hdr_t hdr;
    uint32_t frame_idx;
    uint32_t len;
    uint8_t data[];
}__attribute__((packed,aligned(32))) wakeup_ipc_notify_subcmd_pcm_data_t;

typedef struct{
    wakeup_ipc_notify_subcmd_hdr_t hdr;    
}__attribute__((packed,aligned(32))) wakeup_ipc_notify_subcmd_wakeup_timeout_t;

typedef struct{
    wakeup_ipc_notify_subcmd_hdr_t hdr; 
    uint32_t len;   
    uint32_t angle_cnt;
    uint16_t angle_data[];
}__attribute__((packed,aligned(32))) wakeup_ipc_notify_subcmd_angle_t;

typedef struct{
    wakeup_ipc_notify_subcmd_hdr_t hdr; 
    uint32_t mode;
}__attribute__((packed,aligned(32))) wakeup_ipc_notify_subcmd_switch_mode_t;
/* CP -> AP  control subcmd */

typedef enum {
    WAKEUP_IPC_CONTROL_SUBCMD_PARAMETER_SET = 1,
    WAKEUP_IPC_CONTROL_SUDCMD_DEBUG_MODE_SET = 2,
    WAKEUP_IPC_CONTROL_SUBCMD_ALGO_MODE_SET = 3,
    WAKEUP_IPC_CONTROL_SUBCMD_THRESHOLD_SET = 4,
}wakeup_ipc_control_subcmd_e;

typedef struct {

    uint32_t debug_enable;
}__attribute__((packed)) wakeup_ipc_control_subcmd_parameter_set_t;

typedef struct {
    uint8_t enable;
}__attribute__((packed)) wakeup_ipc_control_subcmd_debug_mode_set_t;

typedef enum {
    WAKEUP_ALGO_MODE_WAKEUP = 0,  // 唤醒模式
    WAKEUP_ALGO_MODE_ESR = 1,     // ESR模式
}wakeup_algo_mode_e;

typedef struct {
    uint8_t mode;  // wakeup_algo_mode_e
}__attribute__((packed)) wakeup_ipc_control_subcmd_algo_mode_set_t;

typedef enum {
    WAKEUP_THRESHOLD_LEVEL_1 = 1,  // 门限等级1（最低）
    WAKEUP_THRESHOLD_LEVEL_2 = 2,  // 门限等级2
    WAKEUP_THRESHOLD_LEVEL_3 = 3,  // 门限等级3
    WAKEUP_THRESHOLD_LEVEL_4 = 4,  // 门限等级4
    WAKEUP_THRESHOLD_LEVEL_5 = 5,  // 门限等级5
    WAKEUP_THRESHOLD_LEVEL_6 = 6,  // 门限等级6（最高）
}wakeup_threshold_level_e;

typedef struct {
    uint8_t level;  // wakeup_threshold_level_e (1-6)
}__attribute__((packed)) wakeup_ipc_control_subcmd_threshold_set_t;

/* ipc end */



#ifdef __cplusplus
}
#endif
