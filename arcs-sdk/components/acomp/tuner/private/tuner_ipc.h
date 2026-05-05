#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TUNER_IPC_CONTROL_SUBCMD_SET_SAMPLERATE = 1,
    TUNER_IPC_CONTROL_SUBCMD_SET_PARAM      = 2,
    TUNER_IPC_CONTROL_SUBCMD_SET_LIMITER    = 3,
    TUNER_IPC_CONTROL_SUBCMD_ENABLE         = 4,
    TUNER_IPC_CONTROL_SUBCMD_SET_VOLUME     = 5,
} tuner_ipc_control_subcmd_e;

typedef struct {
    float sample_rate;
} __attribute__((packed)) tuner_ipc_control_subcmd_set_samplerate_t;

typedef struct {
    uint32_t len;
    uint8_t data[];
} __attribute__((packed)) tuner_ipc_control_subcmd_set_param_t;

typedef struct {
    uint32_t len;
    uint8_t data[];
} __attribute__((packed)) tuner_ipc_control_subcmd_set_limiter_t;

typedef struct {
    uint8_t en;
} __attribute__((packed)) tuner_ipc_control_subcmd_enable_t;

typedef struct {
    float volume;
} __attribute__((packed)) tuner_ipc_control_subcmd_set_volume_t;

typedef enum {
    TUNER_IPC_NOTIFY_SUBCMD_STATUS = 1,
} tuner_ipc_notify_subcmd_e;

typedef struct {
    tuner_ipc_notify_subcmd_e cmd;
} __attribute__((packed)) tuner_ipc_notify_subcmd_hdr_t;

typedef struct {
    tuner_ipc_notify_subcmd_hdr_t hdr;
    uint32_t status;
} __attribute__((packed, aligned(32))) tuner_ipc_notify_subcmd_status_t;

#ifdef __cplusplus
}
#endif
