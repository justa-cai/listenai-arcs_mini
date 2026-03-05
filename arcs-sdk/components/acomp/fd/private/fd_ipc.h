/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "../acomp_fd_params.h"


#ifdef __cplusplus
extern "C" {
#endif

#define ACOMP_FD_RES_NUMBER         (4)

typedef enum _ls_face_res_type
{
	RES_NULL 					= 0,
	RES_FACE_DETECT				= 1,
	RES_FACE_ALIGN				= 2,
	RES_FACE_LIVE				= 3,
	RES_FACE_VERIFY				= 4,
	RES_FACE_COUNT    			   ,
	RES_MAX                     = 0XFFFFFFFF,
}ls_face_res_type;

/* CP -> AP  control subcmd */

typedef enum{
    FD_ICP_CONTROL_SUBCMD_PARAMETER_SET = 1,
    FD_ICP_CONTROL_SUBCMD_PARAMETER_GET = 2,
    FD_ICP_CONTROL_SUBCMD_ALIGN_THRESHOLD_SET = 3,
    FD_ICP_CONTROL_SUBCMD_LIVE_DETECT_SET = 4,
    FD_ICP_CONTROL_SUBCMD_FEATURES_LOAD = 5,
}fd_ipc_control_subcmd_e;

typedef struct  {
	acomp_fd_params_e key;
	float value;
} fd_param_t;

typedef struct{
    uint32_t params_cnt;
    uint8_t data[0];
}__attribute__((packed)) fd_ipc_control_subcmd_parameter_set_t;

typedef struct{
    float yaw;
    float pitch;
    float roll;
}__attribute__((packed)) head_pose_t;

typedef struct {
    head_pose_t head_pose;
}__attribute__((packed)) fd_ipc_control_subcmd_align_threshold_set_t;

typedef struct{
    uint32_t enable;
    float score_threshold[2];
}__attribute__((packed)) fd_ipc_control_subcmd_live_detect_set_t;

typedef struct {
    uint32_t feature_cnt;
    uint8_t data[0];
}__attribute__((packed)) fd_ipc_control_subcmd_features_load_t;

/* AP -> CP  notify subcmd*/

typedef struct{
	uint32_t results_cnt;
    uint32_t max_area_results_index;
  	uint8_t results[0];
}__attribute__((packed)) fd_ipc_notify_subcmd_fd_result_hdr_t;

#ifdef __cplusplus
}
#endif
