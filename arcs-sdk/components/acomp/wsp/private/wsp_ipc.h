/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
    WSP_IPC_NOTIFY_SUBCMD_VAD_BEGIN = 1,
    WSP_IPC_NOTIFY_SUBCMD_VAD_END = 2,

}wsp_ipc_notify_subcmd_e;


typedef struct{
    wsp_ipc_notify_subcmd_e cmd;
    
}__attribute__((packed)) wsp_ipc_notify_subcmd_hdr_t;
typedef struct{
    wsp_ipc_notify_subcmd_hdr_t hdr;
    uint32_t frame_idx;
}__attribute__((packed,aligned(32))) wsp_ipc_notify_subcmd_vad_t;

#ifdef __cplusplus
}
#endif
