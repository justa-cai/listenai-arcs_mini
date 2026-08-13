/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "../acomp_palm_params.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACOMP_PALM_RES_NUMBER (2U)

typedef enum {
    RES_PALM_NULL = 0,
    RES_PALM_DETECT = 1,
    RES_PALM_VERIFY = 2,
    RES_PALM_COUNT,
    RES_PALM_MAX = 0XFFFFFFFF,
} acomp_palm_res_type_t;

typedef enum {
    PALM_IPC_CONTROL_SUBCMD_PARAMETER_SET = 1,
    PALM_IPC_CONTROL_SUBCMD_PARAMETER_GET = 2,
    PALM_IPC_CONTROL_SUBCMD_FEATURES_LOAD = 3,
} palm_ipc_control_subcmd_e;

typedef struct {
    acomp_palm_params_e key;
    float value;
} palm_param_t;

typedef struct {
    uint32_t params_cnt;
    uint8_t data[0];
} __attribute__((packed)) palm_ipc_control_subcmd_parameter_set_t;

typedef struct {
    uint32_t feature_cnt;
    uint8_t data[0];
} __attribute__((packed)) palm_ipc_control_subcmd_features_load_t;

typedef struct {
    uint32_t results_cnt;
    uint32_t max_area_results_index;
    uint8_t results[0];
} __attribute__((packed)) palm_ipc_notify_subcmd_palm_result_hdr_t;

#ifdef __cplusplus
}
#endif
