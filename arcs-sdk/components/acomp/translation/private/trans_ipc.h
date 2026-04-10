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
    TRANS_IPC_CONTROL_SUBCMD_TRANSLATE    = 1,
    TRANS_IPC_CONTROL_SUBCMD_STOP         = 2,
    TRANS_IPC_CONTROL_SUBCMD_SET_RES_TYPE = 3,
} trans_ipc_control_subcmd_e;

typedef struct {
    uint32_t len;
    uint8_t text[];
} __attribute__((packed)) trans_ipc_control_subcmd_translate_t;

typedef struct {
    int32_t type;
} __attribute__((packed)) trans_ipc_control_subcmd_set_res_type_t;

/* AP -> CP notify subcmds */

typedef enum {
    TRANS_IPC_NOTIFY_SUBCMD_TRANS_STATUS = 1,
} trans_ipc_notify_subcmd_e;

typedef struct {
    trans_ipc_notify_subcmd_e cmd;
} __attribute__((packed)) trans_ipc_notify_subcmd_hdr_t;

typedef struct {
    trans_ipc_notify_subcmd_hdr_t hdr;
    uint32_t status;
} __attribute__((packed, aligned(32))) trans_ipc_notify_subcmd_status_t;

#ifdef __cplusplus
}
#endif
