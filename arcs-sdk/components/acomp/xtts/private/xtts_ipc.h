/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CP -> AP control subcmds */

typedef enum {
    XTTS_IPC_CONTROL_SUBCMD_SYNTH_TEXT = 1,
    XTTS_IPC_CONTROL_SUBCMD_STOP      = 2,
    XTTS_IPC_CONTROL_SUBCMD_SET_SPEED  = 3,
    XTTS_IPC_CONTROL_SUBCMD_SET_VOLUME = 4,
    XTTS_IPC_CONTROL_SUBCMD_SET_ROLE   = 5,
} xtts_ipc_control_subcmd_e;

typedef struct {
    uint32_t len;
    uint8_t text[];
} __attribute__((packed)) xtts_ipc_control_subcmd_synth_text_t;

typedef struct {
    int32_t speed;
} __attribute__((packed)) xtts_ipc_control_subcmd_set_speed_t;

typedef struct {
    int32_t volume;
} __attribute__((packed)) xtts_ipc_control_subcmd_set_volume_t;

typedef struct {
    int32_t role;
} __attribute__((packed)) xtts_ipc_control_subcmd_set_role_t;

/* AP -> CP notify subcmds */

typedef enum {
    XTTS_IPC_NOTIFY_SUBCMD_SYNTH_STATUS = 1,
} xtts_ipc_notify_subcmd_e;

typedef struct {
    xtts_ipc_notify_subcmd_e cmd;
} __attribute__((packed)) xtts_ipc_notify_subcmd_hdr_t;

typedef struct {
    xtts_ipc_notify_subcmd_hdr_t hdr;
    uint32_t status;
} __attribute__((packed, aligned(32))) xtts_ipc_notify_subcmd_status_t;

#ifdef __cplusplus
}
#endif
