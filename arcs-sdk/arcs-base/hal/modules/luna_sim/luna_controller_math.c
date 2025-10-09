/*
 * luna_controller_math.c
 *
 *  Created on: Sep 10, 2017
 *      Author: dwwang
 */

#include "luna_sim/luna.h"
#include "luna_sim/luna_controller_math.h"

void* bigop_alloc_normal_(int32_t type, uint32_t size);
int32_t bigop_execute_op_normal_(const void* api, uint32_t api_size, const void* param, uint32_t param_size);
int32_t bigop_execute_code_normal_(const void* api, uint32_t api_size, const void* param, uint32_t param_size);
bigop_alloc_func_t bigop_alloc = bigop_alloc_normal_;
bigop_execute_op_func_t bigop_execute_op = bigop_execute_op_normal_;
bigop_execute_code_func_t bigop_execute_code = bigop_execute_code_normal_;

bigop_handle_t bigop_init(void* objmem, uint32_t objmem_size, uint32_t code_size, uint32_t data_size) { return 0; }
int32_t bigop_begin(bigop_handle_t handle) { return 0; }
int32_t bigop_end(bigop_handle_t handle) { return 0; }
int32_t bigop_reset(bigop_handle_t handle) { return 0; }
int32_t bigop_run(bigop_handle_t handle) { return 0; }
int32_t bigop_run_async(bigop_handle_t handle) { return 0; }
int32_t bigop_wait_complete(bigop_handle_t handle) { return 0; }

void* bigop_alloc_normal_(int32_t type, uint32_t size) { return 0; } //type: 0-code, 1-data
int32_t bigop_execute_op_normal_(const void* api, uint32_t api_size, const void* param, uint32_t param_size) { return 0; }
int32_t bigop_execute_code_normal_(const void* api, uint32_t api_size, const void* param, uint32_t param_size) { return 0; }
int32_t bigop_set_mode(int32_t mode) { return 0; } //mode:0-normal 1-bigop 2-stat
int32_t bigop_stat(uint32_t* objmem_size, uint32_t* code_size, uint32_t* data_size) { return 0; }

int32_t luna_shift_mov_inline(const q31_t *src1, q31_t *dst) { return 0; }